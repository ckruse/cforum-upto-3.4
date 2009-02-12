/**
 * \file flt_extern.c
 * \author Christian Kruse, <cjk@wwwtech.de>
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
#include "cfconfig.h"
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
#include "cfgcomp.h"
#include "readline.h"

#include "serverutils.h"
#include "serverlib.h"
#include "fo_server.h"
/* }}} */

static struct sockaddr_in *flt_extern_addr      = NULL;

/* {{{ flt_extern_set_us_up_the_socket */
int flt_extern_set_us_up_the_socket(cf_configuration_t *cfg,struct sockaddr_in *addr) {
  int sock,ret,one = 1;
  cf_cfg_config_value_t *port = cf_cfg_get_value(cfg,"Extern:Port"),*cfg_addr = cf_cfg_get_value(cfg,"Extern:Interface");

  if(!port || port->type != CF_ASM_ARG_NUM || (addr && addr->type != CF_ASM_ARG_STR)) {
    cf_log(CF_ERR,__FILE__,__LINE__,"flt_extern: socket: port or interface not given\n");
    return -1;
  }

  if((sock = socket(AF_INET,SOCK_STREAM,0)) == -1) {
    cf_log(CF_ERR,__FILE__,__LINE__,"flt_extern: socket: %s\n",strerror(errno));
    return -1;
  }

  if((setsockopt(sock, SOL_SOCKET,SO_REUSEADDR,&one,sizeof(one))) == -1) {
    cf_log(CF_ERR,__FILE__,__LINE__,"flt_extern: setsockopt(SO_REUSEADDR): %s\n",strerror(errno));
    close(sock);
    return -1;
  }

  memset(addr,0,sizeof(*addr));
  addr->sin_family = AF_INET;
  addr->sin_port   = htons(port->ival);

  if(cfg_addr) {
    if((ret = inet_aton(cfg_addr->sval,&(addr->sin_addr))) != 0) {
      cf_log(CF_ERR,__FILE__,__LINE__,"flt_extern: inet_aton(\"%s\"): %s\n",cfg_addr->sval,strerror(ret));
      close(sock);
      return -1;
    }
  }
  else addr->sin_addr.s_addr = htonl(INADDR_ANY);

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
void flt_extern_send_list(cf_forum_t *forum,int sock,time_t date) {
  cf_string_t str;
  cf_thread_t *t,*t1;
  cf_posting_t *p;
  char buff[256];
  size_t n;
  int first;

  cf_str_init(&str);
  cf_str_chars_append(&str,"200 Ok\n",7);

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
          cf_str_chars_append(&str,"THREAD t",8);
          cf_uint64_to_str(&str,t->tid);
          cf_str_chars_append(&str," m",2);
          cf_uint64_to_str(&str,p->mid);
          cf_str_char_append(&str,'\n');
        }
        else {
          cf_str_chars_append(&str,"MSG m",5);
          cf_uint64_to_str(&str,p->mid);
          cf_str_char_append(&str,'\n');
        }

        cf_str_chars_append(&str,buff,n);

        /* author */
        cf_str_chars_append(&str,"Author:",7);
        cf_str_chars_append(&str,p->user.name.content,p->user.name.len);

        /* subject */
        cf_str_chars_append(&str,"\nSubject:",9);
        cf_str_chars_append(&str,p->subject.content,p->subject.len);

        /* category */
        if(p->category.len) {
          cf_str_chars_append(&str,"\nCategory:",10);
          cf_str_chars_append(&str,p->category.content,p->category.len);
        }

        /* date */
        cf_str_chars_append(&str,"\nDate:",6);
        cf_uint32_to_str(&str,(u_int32_t)p->date);
        cf_str_char_append(&str,'\n');

        /* level */
        cf_str_chars_append(&str,"Level:",6);
        cf_uint16_to_str(&str,p->level);
        cf_str_char_append(&str,'\n');

        cf_str_chars_append(&str,"END\n",4);
      }
    }

    t1 = t->next;
    CF_RW_UN(&t->lock);
  }

  cf_str_char_append(&str,'\n');

  writen(sock,str.content,str.len);
  cf_str_cleanup(&str);
}
/* }}} */

/* {{{ flt_extern_handle_request */
void flt_extern_handle_request(cf_configuration_t *cfg,int sock) {
  rline_t tsd;
  u_char *line;
  u_char **tokens;
  int ShallRun = 1;
  int tnum;
  int selected = 0;
  u_int64_t tid,mid;
  u_char buff[256];
  cf_string_t str;
  cf_thread_t *t,*t1;
  cf_posting_t *p;
  cf_forum_t *forum = NULL;

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
              tid = cf_str_to_uint64(tokens[2]+1);
              mid = 0;

              /*
               * message id is optional. If no message id is given,
               * we use 0 to indicate that we want to get the whole
               * thread
               */
              if(tnum == 4) mid = cf_str_to_uint64(tokens[3]+1);

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
            cf_str_init(&str);
            cf_str_chars_append(&str,"200 List Follows\n",17);

            CF_RW_RD(&forum->threads.lock);
            t = forum->threads.list;
            CF_RW_UN(&forum->threads.lock);

            for(;t;t=t1) {
              CF_RW_RD(&t->lock);

              for(p=t->postings;p;p=p->next) {
                cf_str_char_append(&str,'m');
                cf_uint64_to_str(&str,p->mid);
                cf_str_char_append(&str,'\n');
              }

              t1 = t->next;
              CF_RW_UN(&t->lock);
            }

            cf_str_char_append(&str,'\n');

            writen(sock,str.content,str.len);
            cf_str_cleanup(&str);
          }
          /* }}} */
        }
        else writen(sock,"500 SELECT first\n",17);
      }
      else ShallRun = 0;

      if(line) free(line);
    }
  }

  close(sock);
}
/* }}} */

/* {{{ flt_extern_register_server */
int flt_extern_register_server(int sock) {
  flt_extern_addr = cf_alloc(NULL,1,sizeof(*flt_extern_addr),CF_ALLOC_CALLOC);
  sock = flt_extern_set_us_up_the_socket(flt_extern_addr);

  if(sock < 0) return FLT_EXIT;
  cf_push_server(sock,(struct sockaddr *)flt_extern_addr,sizeof(*flt_extern_addr),flt_extern_handle_request);

  return FLT_OK;
}
/* }}} */

void flt_extern_cleanup(void) {
  if(flt_extern_addr) free(flt_extern_addr);
}

/**
 * Config options:
 * Extern:Port = 666;
 * Extern:Interface = "192.168.0.1";
 */

cf_handler_config_t flt_extern_handlers[] = {
  { INIT_HANDLER,            flt_extern_register_server   },
  { 0, NULL }
};

cf_module_config_t flt_extern = {
  MODULE_MAGIC_COOKIE,
  flt_extern_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  flt_extern_cleanup
};

/* eof */
