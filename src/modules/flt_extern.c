/**
 * \file flt_extern.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 *
 * Implementation of the Classic Forum Transfer Protocol for
 * external read-only access
 */

/* {{{ Initial comments */
/*
 * $LastChangedDate$
 * $LastChangedRevision$
 * $LastChangedBy$
 *
 */
/* }}} */

/* {{{ Includes */
#include "config.h"
#include "defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include <sys/types.h>
#include <errno.h>

/* socket includes */
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <pthread.h>

#include "cf_pthread.h"

#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "readline.h"
#include "fo_server.h"
#include "serverlib.h"
/* }}} */

static struct sockaddr_in *Extern_addr      = NULL;
static u_char             *Extern_interface = NULL;
static int                 Extern_port      = 0;

int flt_extern_set_us_up_the_socket(struct sockaddr_in *addr) {
  int sock,ret,one = 1;

  if((sock = socket(AF_INET,SOCK_STREAM,0)) == -1) {
    cf_log(LOG_ERR,__FILE__,__LINE__,"flt_extern: socket: %s\n",sock,strerror(errno));
    return -1;
  }

  if((setsockopt(sock, SOL_SOCKET,SO_REUSEADDR,&one,sizeof(one))) == -1) {
    cf_log(LOG_ERR,__FILE__,__LINE__,"flt_extern: setsockopt(SO_REUSEADDR): %s\n",strerror(errno));
    close(sock);
    return -1;
  }

  memset(addr,0,sizeof(*addr));
  addr->sin_family = AF_INET;
  addr->sin_port   = htons(Extern_port);

  if(Extern_interface) {
    if((ret = inet_aton(Extern_interface,&(addr->sin_addr))) != 0) {
      cf_log(LOG_ERR,__FILE__,__LINE__,"flt_extern: inet_aton(\"%s\"): %s\n",Extern_interface,strerror(ret));
      close(sock);
      return -1;
    }
  }
  else {
    addr->sin_addr.s_addr = htonl(INADDR_ANY);
  }

  if(bind(sock,(struct sockaddr *)addr,sizeof(*addr)) < 0) {
    cf_log(LOG_ERR,__FILE__,__LINE__,"flt_extern: bind: %s\n",strerror(errno));
    close(sock);
    return -1;
  }

  if(listen(sock,LISTENQ) != 0) {
    cf_log(LOG_ERR,__FILE__,__LINE__,"flt_extern: listen: %s\n",strerror(errno));
    close(sock);
    return -1;
  }

  return sock;
}

void flt_extern_send_list(int sock,time_t date) {
  t_string str;
  t_thread *t,*t1;
  t_posting *p;
  char buff[256];
  size_t n;
  int first;

  str_init(&str);
  str_chars_append(&str,"200 Ok\n",7);

  CF_RW_RD(&head.lock);
  t = head.thread;
  CF_RW_UN(&head.lock);

  for(;t;t=t1) {
    CF_RW_RD(&t->lock);
    first = 1;

    for(p=t->postings;p;p=p->next) {
      if(p->date >= date) {
        if(p->invisible) {
          for(;p && p->invisible;p=p->next);
          if(!p) break;
        }


        /* thread/posting header */
        if(first) {
          first = 0;
          n = snprintf(buff,256,"THREAD t%llu m%llu\n",t->tid,p->mid);
        }
        else {
          n = snprintf(buff,256,"MSG m%lld\n",p->mid);
        }

        str_chars_append(&str,buff,n);

        /* author */
        str_chars_append(&str,"Author:",7);
        str_chars_append(&str,p->user.name,p->user.name_len);

        /* subject */
        str_chars_append(&str,"\nSubject:",9);
        str_chars_append(&str,p->subject,p->subject_len);

        /* category */
        if(p->category) {
          str_chars_append(&str,"\nCategory:",10);
          str_chars_append(&str,p->category,p->category_len);
        }

        /* date */
        n = snprintf(buff,256,"\nDate:%ld\n",p->date);
        str_chars_append(&str,buff,n);

        /* level */
        n = snprintf(buff,256,"Level:%d\n",p->level);
        str_chars_append(&str,buff,n);

        str_chars_append(&str,"END\n",4);
      }
    }

    t1 = t->next;
    CF_RW_UN(&t->lock);
  }

  str_char_append(&str,'\n');

  writen(sock,str.content,str.len);
  str_cleanup(&str);
}

void flt_extern_handle_request(int sock) {
  rline_t tsd;
  u_char *line;
  u_char **tokens;
  int ShallRun = 1;
  int tnum;

  memset(&tsd,0,sizeof(tsd));

  writen(sock,"200 External Classic Forum Transfer Protocol plugin ready\n",58);

  while(ShallRun) {
    line = readline(sock,&tsd);

    #ifdef DEBUG
    cf_log(LOG_DBG,__FILE__,__LINE__,"flt_nntp: line: '%s'\n",line);
    #endif

    if(line) {
      if((tnum = cf_tokenize(line,&tokens)) != 0) {
        /* {{{ quit */
        if(cf_strcasecmp(tokens[0],"QUIT") == 0) {
          writen(sock,"200 Bye\n",8);
          ShallRun = 0;
        }
        /* }}} */
        /* {{{ check for GET token */
        else if(cf_strcasecmp(tokens[0],"GET") != 0) {
          writen(sock,"500 Syntax error\n",17);
        }
        /* }}} */

        /* {{{ GET THREADLIST */
        if(cf_strcmp(tokens[1],"THREADLIST") == 0) {
          if(tnum == 3) {
            /* we got a date. So we have to go through each thread manually */
            flt_extern_send_list(sock,strtoul(tokens[2],NULL,10));
          }
          else {
            #ifndef CF_SHARED_MEM
            CF_RW_RD(&head.lock);
            writen(sock,head.cache_visible.content,head.cache_visible.len);
            CF_RW_UN(&head.lock);
            #else
            /* no head.cache available, go through each thread manually */
            flt_extern_send_list(sock,0);
            #endif
          }

        }
        /* }}} */

        /* {{{ GET POSTING */
        else if(cf_strcmp(tokens[1],"POSTING") == 0) {
          if(tnum < 3) {
            writen(sock,"501 Thread id or message id missing\n",36);
          }
          else if(tnum > 4) {
            writen(sock,"500 Syntax error\n",17);
          }
          else {
            u_int64_t tid = strtoull(tokens[2]+1,NULL,10);
            u_int64_t mid = 0;

            /*
             * message id is optional. If no message id is given,
             * we use 0 to indicate that we want to get the whole
             * thread
             */
            if(tnum == 4) mid = strtoull(tokens[3]+1,NULL,10);

            cf_send_posting(sock,tid,mid,0);
          }
        }
        /* }}} */

        /* {{{ GET LASTMODIFIED */
        else if(cf_strcmp(tokens[1],"LASTMODIFIED") == 0) {
          u_char buff[50];
          int l;

          CF_RW_RD(&head.lock);
          l = snprintf(buff,50,"%ld\n",head.date_visible);
          CF_RW_UN(&head.lock);

          writen(sock,buff,l);
        }
        /* }}} */

        /* {{{ GET MIDLIST */
        else if(cf_strcmp(tokens[1],"MIDLIST") == 0) {
          t_string str;
          t_thread *t,*t1;
          t_posting *p;
          char buff[256];
          size_t len;

          str_init(&str);
          str_chars_append(&str,"200 List Follows\n",17);

          CF_RW_RD(&head.lock);
          t = head.thread;
          CF_RW_UN(&head.lock);

          for(;t;t=t1) {
            CF_RW_RD(&t->lock);

            for(p=t->postings;p;p=p->next) {
              len = snprintf(buff,256,"m%lld\n",p->mid);
              str_chars_append(&str,buff,len);
            }

            t1 = t->next;
            CF_RW_UN(&t->lock);
          }

          str_char_append(&str,'\n');

          writen(sock,str.content,str.len);
          str_cleanup(&str);
        }
        /* }}} */
      }
      else {
        ShallRun = 0;
      }

      if(line) free(line);
    }
  }

  close(sock);
}

int flt_extern_register_server(int sock) {
  Extern_addr = calloc(1,sizeof(*Extern_addr));
  sock        = flt_extern_set_us_up_the_socket(Extern_addr);

  if(sock < 0) return FLT_EXIT;

  if(cf_push_server(sock,(struct sockaddr *)Extern_addr,sizeof(*Extern_addr),flt_extern_handle_request) != 0) {
    cf_log(LOG_ERR,__FILE__,__LINE__,"cf_push_server returned not 0!\n");
    return FLT_EXIT;
  }

  return FLT_OK;
}

int flt_extern_handle(t_configfile *cf,t_conf_opt *opt,u_char **args,int argnum) {
  if(cf_strcmp(opt->name,"ExternPort") == 0)           Extern_port      = atoi(args[0]);
  else if(cf_strcmp(opt->name,"ExternInterface") == 0) Extern_interface = strdup(args[0]);

  return 0;
}

t_conf_opt flt_extern_config[] = {
  { "ExternPort",      flt_extern_handle, NULL },
  { "ExternInterface", flt_extern_handle, NULL },
  { NULL,         NULL,              NULL }
};

t_handler_config flt_extern_handlers[] = {
  { INIT_HANDLER,            flt_extern_register_server   },
  { 0, NULL }
};

t_module_config flt_extern = {
  flt_extern_config,
  flt_extern_handlers,
  NULL,
  NULL,
  NULL,
  NULL
};

/* eof */
