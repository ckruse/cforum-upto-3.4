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

#include <pthread.h>

#include <sys/stat.h>
#include <sys/types.h>

/* for sockets */
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <errno.h>

struct sockaddr_un;

#include "cf_pthread.h"

#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "readline.h"

#include "serverutils.h"
#include "serverlib.h"
#include "fo_server.h"
/* }}} */

static struct sockaddr_in *Extern_addr      = NULL;
static u_char             *Extern_interface = NULL;
static int                 Extern_port      = 0;

/* {{{ flt_extern_set_us_up_the_socket */
int flt_extern_set_us_up_the_socket(struct sockaddr_in *addr) {
  int sock,ret,one = 1;

  if((sock = socket(AF_INET,SOCK_STREAM,0)) == -1) {
    cf_log(CF_ERR,__FILE__,__LINE__,"flt_extern: socket: %s\n",sock,strerror(errno));
    return -1;
  }

  if((setsockopt(sock, SOL_SOCKET,SO_REUSEADDR,&one,sizeof(one))) == -1) {
    cf_log(CF_ERR,__FILE__,__LINE__,"flt_extern: setsockopt(SO_REUSEADDR): %s\n",strerror(errno));
    close(sock);
    return -1;
  }

  memset(addr,0,sizeof(*addr));
  addr->sin_family = AF_INET;
  addr->sin_port   = htons(Extern_port);

  if(Extern_interface) {
    if((ret = inet_aton(Extern_interface,&(addr->sin_addr))) != 0) {
      cf_log(CF_ERR,__FILE__,__LINE__,"flt_extern: inet_aton(\"%s\"): %s\n",Extern_interface,strerror(ret));
      close(sock);
      return -1;
    }
  }
  else {
    addr->sin_addr.s_addr = htonl(INADDR_ANY);
  }

  if(bind(sock,(struct sockaddr *)addr,sizeof(*addr)) < 0) {
    cf_log(CF_ERR,__FILE__,__LINE__,"flt_extern: bind: %s\n",strerror(errno));
    close(sock);
    return -1;
  }

  if(listen(sock,LISTENQ) != 0) {
    cf_log(CF_ERR,__FILE__,__LINE__,"flt_extern: listen: %s\n",strerror(errno));
    close(sock);
    return -1;
  }

  return sock;
}
/* }}} */

/* {{{ flt_extern_send_list */
void flt_extern_send_list(forum_t *forum,int sock,time_t date) {
  string_t str;
  thread_t *t,*t1;
  posting_t *p;
  char buff[256];
  size_t n;
  int first;

  str_init(&str);
  str_chars_append(&str,"200 Ok\n",7);

  CF_RW_RD(&forum->threads.lock);
  t = forum->threads.list;
  CF_RW_UN(&forum->threads.lock);

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
          str_chars_append(&str,"THREAD t",8);
          u_int64_to_str(&str,t->tid);
          str_chars_append(&str," m",2);
          u_int64_to_str(&str,p->mid);
          str_char_append(&str,'\n');
        }
        else {
          str_chars_append(&str,"MSG m",5);
          u_int64_to_str(&str,p->mid);
          str_char_append(&str,'\n');
        }

        str_chars_append(&str,buff,n);

        /* author */
        str_chars_append(&str,"Author:",7);
        str_chars_append(&str,p->user.name.content,p->user.name.len);

        /* subject */
        str_chars_append(&str,"\nSubject:",9);
        str_chars_append(&str,p->subject.content,p->subject.len);

        /* category */
        if(p->category.len) {
          str_chars_append(&str,"\nCategory:",10);
          str_chars_append(&str,p->category.content,p->category.len);
        }

        /* date */
        str_chars_append(&str,"\nDate:",6);
        u_int32_to_str(&str,(u_int32_t)p->date);
        str_char_append(&str,'\n');

        /* level */
        str_chars_append(&str,"Level:",6);
        u_int16_to_str(&str,p->level);
        str_char_append(&str,'\n');

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
/* }}} */

/* {{{ flt_extern_handle_request */
void flt_extern_handle_request(int sock) {
  rline_t tsd;
  u_char *line;
  u_char **tokens;
  int ShallRun = 1;
  int tnum;
  int selected = 0;
  u_int64_t tid,mid;
  u_char buff[256];
  string_t str;
  thread_t *t,*t1;
  posting_t *p;
  forum_t *forum = NULL;

  size_t len;


  memset(&tsd,0,sizeof(tsd));

  writen(sock,"200 External Classic Forum Transfer Protocol plugin ready\n",58);

  while(ShallRun) {
    line = readline(sock,&tsd);

    #ifdef DEBUG
    cf_log(CF_DBG,__FILE__,__LINE__,"flt_nntp: line: '%s'\n",line);
    #endif

    if(line) {
      if((tnum = cf_tokenize(line,&tokens)) != 0) {
        /* {{{ SELECT */
        if(cf_strcasecmp(tokens[0],"SELECT") == 0) {
          if(tnum == 2) {
            if((forum = cf_hash_get(head.forums,tokens[1],strlen(tokens[1]))) == NULL) writen(sock,"404 Not found\n",14);
            else {
              selected = 1;
              writen(sock,"200 Ok\n",7);
            }
          }
          else writen(sock,"500 Syntax error\n",17);
        }
        /* }}} */

        /* {{{ QUIT */
        else if(cf_strcasecmp(tokens[0],"QUIT") == 0) {
          writen(sock,"200 Bye\n",8);
          ShallRun = 0;
        }
        /* }}} */

        /* {{{ check for GET token */
        else if(cf_strcasecmp(tokens[0],"GET") != 0) {
          writen(sock,"500 Syntax error\n",17);
        }
        /* }}} */

        /* all other may only be used after SELECT */
        else if(selected == 1) {

          /* {{{ GET THREADLIST */
          if(cf_strcmp(tokens[1],"THREADLIST") == 0) {
            if(tnum == 3) {
              /* we got a date. So we have to go through each thread manually */
              flt_extern_send_list(forum,sock,strtoul(tokens[2],NULL,10));
            }
            else {
              #ifndef CF_SHARED_MEM
              CF_RW_RD(&forum->lock);
              writen(sock,forum->cache.visible.content,forum->cache.visible.len);
              CF_RW_UN(&forum->lock);
              #else
              /* no head.cache available, go through each thread manually */
              flt_extern_send_list(forum,sock,0);
              #endif
            }

          }
          /* }}} */

          /* {{{ GET POSTING */
          else if(cf_strcmp(tokens[1],"POSTING") == 0) {
            if(tnum < 3) writen(sock,"501 Thread id or message id missing\n",36);
            else if(tnum > 4) writen(sock,"500 Syntax error\n",17);
            else {
              tid = str_to_u_int64(tokens[2]+1);
              mid = 0;

              /*
               * message id is optional. If no message id is given,
               * we use 0 to indicate that we want to get the whole
               * thread
               */
              if(tnum == 4) mid = str_to_u_int64(tokens[3]+1);

              cf_send_posting(forum,sock,tid,mid,0);
            }
          }
          /* }}} */

          /* {{{ GET LASTMODIFIED */
          else if(cf_strcmp(tokens[1],"LASTMODIFIED") == 0) {
            CF_RW_RD(&forum->lock);
            len = snprintf(buff,250,"200 Ok\n%ld\n",forum->date.visible);
            CF_RW_UN(&forum->lock);

            writen(sock,buff,len);
          }
          /* }}} */

          /* {{{ GET MIDLIST */
          else if(cf_strcmp(tokens[1],"MIDLIST") == 0) {
            str_init(&str);
            str_chars_append(&str,"200 List Follows\n",17);

            CF_RW_RD(&forum->threads.lock);
            t = forum->threads.list;
            CF_RW_UN(&forum->threads.lock);

            for(;t;t=t1) {
              CF_RW_RD(&t->lock);

              for(p=t->postings;p;p=p->next) {
                str_char_append(&str,'m');
                u_int64_to_str(&str,p->mid);
                str_char_append(&str,'\n');
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
        else writen(sock,"500 SELECT first\n",17);
      }
      else {
        ShallRun = 0;
      }

      if(line) free(line);
    }
  }

  close(sock);
}
/* }}} */

/* {{{ flt_extern_register_server */
int flt_extern_register_server(int sock) {
  Extern_addr = fo_alloc(NULL,1,sizeof(*Extern_addr),FO_ALLOC_CALLOC);
  sock        = flt_extern_set_us_up_the_socket(Extern_addr);

  if(sock < 0) return FLT_EXIT;
  cf_push_server(sock,(struct sockaddr *)Extern_addr,sizeof(*Extern_addr),flt_extern_handle_request);

  return FLT_OK;
}
/* }}} */

void flt_extern_cleanup(void) {
  if(Extern_addr) free(Extern_addr);
  if(Extern_interface) free(Extern_interface);
}

/* {{{ flt_extern_handle */
int flt_extern_handle(configfile_t *cf,conf_opt_t *opt,const u_char *context,u_char **args,size_t argnum) {
  if(cf_strcmp(opt->name,"ExternPort") == 0)           Extern_port      = atoi(args[0]);
  else if(cf_strcmp(opt->name,"ExternInterface") == 0) Extern_interface = strdup(args[0]);

  return 0;
}
/* }}} */

conf_opt_t flt_extern_config[] = {
  { "ExternPort",      flt_extern_handle, CFG_OPT_CONFIG|CFG_OPT_NEEDED|CFG_OPT_GLOBAL, NULL },
  { "ExternInterface", flt_extern_handle, CFG_OPT_CONFIG|CFG_OPT_GLOBAL,                NULL },
  { NULL, NULL, 0, NULL }
};

handler_config_t flt_extern_handlers[] = {
  { INIT_HANDLER,            flt_extern_register_server   },
  { 0, NULL }
};

module_config_t flt_extern = {
  MODULE_MAGIC_COOKIE,
  flt_extern_config,
  flt_extern_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  flt_extern_cleanup
};

/* eof */
