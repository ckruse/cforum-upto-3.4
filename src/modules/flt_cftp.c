/**
 * \file flt_cftp.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 *
 * Implementation of the Classic Forum Transfer Protocol
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

#include "cf_pthread.h"

#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "readline.h"
#include "fo_server.h"
#include "serverlib.h"
/* }}} */

int flt_cftp_handler(int sockfd,const u_char **tokens,int tnum,rline_t *tsd) {
  t_name_value *sort_t_v = cfg_get_first_value(&fo_server_conf,"SortThreads"),
               *sort_m_v = cfg_get_first_value(&fo_server_conf,"SortMessages");
  int sort_m = atoi(sort_m_v->values[0]),
      sort_t = atoi(sort_t_v->values[0]),
      ret;
  size_t i,
         l,
         len,
         err = 0,
         one = 1;
  t_handler_config *handler;
  u_int64_t tid,
            mid;
  t_thread *t,
           *t1;
  t_posting *p,
            *p1;
  t_string str;
  u_char buff[512];
  t_srv_new_post_filter pfkt;
  t_srv_new_thread_filter tfkt;

  /* {{{ get */
  if(cf_strcmp(tokens[0],"GET") == 0) {
    if(tnum >= 2) {
      cf_log(LOG_DBG,__FILE__,__LINE__,"%s %s\n",tokens[0],tokens[1]);

      /* {{{ GET THREADLIST */
      if(cf_strcmp(tokens[1],"THREADLIST") == 0) {
        CF_RW_RD(&head.lock);

        if(tnum == 3 && cf_strcmp(tokens[2],"invisible=1") == 0) {
          writen(sockfd,head.cache_invisible.content,head.cache_invisible.len);
        }
        else {
          writen(sockfd,head.cache_visible.content,head.cache_visible.len);
        }

        CF_RW_UN(&head.lock);
      }
      /* }}} */

      /* {{{ GET POSTING */
      else if(cf_strcmp(tokens[1],"POSTING") == 0) {
        if(tnum < 4) {
          writen(sockfd,"501 Thread id or message id missing\n",36);
        }
        else if(tnum > 5) {
          writen(sockfd,"500 Syntax error\n",17);
        }
        else {
          tid = strtoull(tokens[2]+1,NULL,10),
          mid = strtoull(tokens[3]+1,NULL,10);

          cf_send_posting(sockfd,tid,mid,tnum == 5 && cf_strcmp(tokens[4],"invisible=1") == 0);
        }
      }
      /* }}} */

      /* {{{ GET LASTMODIFIED */
      else if(cf_strcmp(tokens[1],"LASTMODIFIED") == 0) {
        CF_RW_RD(&head.lock);

        if(tnum == 3 && *tokens[2] == '1') {
          l = snprintf(buff,50,"%ld\n",head.date_invisible);
        }
        else {
          l = snprintf(buff,50,"%ld\n",head.date_visible);
        }

        CF_RW_UN(&head.lock);

        writen(sockfd,buff,l);
      }
      /* }}} */

      /* {{{ GET MIDLIST */
      else if(cf_strcmp(tokens[1],"MIDLIST") == 0) {
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

        writen(sockfd,str.content,str.len);
        str_cleanup(&str);
      }
      /* }}} */

      else {
        return FLT_DECLINE;
      }
    }
  }
  /* }}} */
  /* {{{ stat */
  else if(cf_strcmp(tokens[0],"STAT") == 0) {
    if(tnum != 4) return FLT_DECLINE;

    /* {{{ STAT THREAD */
    if(cf_strcmp(tokens[1],"THREAD") == 0) {
      tid = strtoull(tokens[2]+1,NULL,10);

      if(cf_get_thread(tid)) {
        writen(sockfd,"200 Exists\n",11);
      }
      else {
        writen(sockfd,"404 Thread not found\n",21);
      }
    }
    /* }}} */

    /* {{{ STAT POST */
    else if(cf_strcmp(tokens[1],"POST") == 0) {
      tid = strtoull(tokens[2]+1,NULL,10);
      mid = strtoull(tokens[3]+1,NULL,10);

      if((t = cf_get_thread(tid)) != NULL) {
        if(cf_get_posting(t,mid)) writen(sockfd,"200 Posting exists\n",19);
        else writen(sockfd,"404 Posting not found\n",22);
      }
      else writen(sockfd,"404 Thread not found\n",21);
    }
    /* }}} */

    else {
      return FLT_DECLINE;
    }

  }
  /* }}} */
  /* {{{ post */
  else if(cf_strcmp(tokens[0],"POST") == 0) {
    if(tnum < 2) return FLT_DECLINE;

    /* {{{ POST ANSWER */
    if(cf_strcmp(tokens[1],"ANSWER") == 0) {
      p = fo_alloc(NULL,1,sizeof(t_posting),FO_ALLOC_CALLOC);

      if(tnum != 4) {
        writen(sockfd,"500 Sorry\n",10);
        cf_log(LOG_ERR,__FILE__,__LINE__,"Bad request\n");
      }
      else {
        tid = strtoull(tokens[2]+2,NULL,10);
        mid = strtoull(tokens[3]+2,NULL,10);

        t  = cf_get_thread(tid);

        if(!t) {
          writen(sockfd,"404 Thread Not Found\n",21);
          cf_log(LOG_ERR,__FILE__,__LINE__,"Thread not found\n");
          err = 1;
        }
        else {
          p1 = cf_get_posting(t,mid);
          CF_RW_RD(&t->lock);

          if(!p1 || p1->invisible == 1) {
            writen(sockfd,"404 Posting Not Found\n",22);
            cf_log(LOG_ERR,__FILE__,__LINE__,"Posting not found\n");
            err = 1;

            CF_RW_UN(&t->lock);
          }
          else {
            CF_RW_UN(&t->lock);

            if(cf_read_posting(p,sockfd,tsd)) {
              CF_RW_WR(&t->lock);

              CF_LM(&head.unique_ids_mutex);
              if(cf_hash_get(head.unique_ids,p->unid,p->unid_len) != NULL) {
                writen(sockfd,"502 Unid already got\n",21);
                cf_log(LOG_ERR,__FILE__,__LINE__,"Unid already got\n");
                err = 1;
              }
              CF_UM(&head.unique_ids_mutex);

              if(err == 0) {
                p->level     = p1->level + 1;
                p->invisible = 0;
                t->newest    = p;
                t->posts    += 1;

                if(sort_m == 2 || p1->next == NULL) {
                  p->next       = p1->next;
                  p1->next      = p;
                  p->prev       = p1;

                  if(p->next) p->next->prev = p;
                  else        t->last = p;
                }
                else {
                  for(p1=p1->next;p1->next && p1->level == p->level;p1=p1->next);
                  p->next       = p1->next;
                  p1->next      = p;
                  p->prev       = p1;

                  if(p->next) p->next->prev = p;
                  else        t->last       = p;
                }

                CF_LM(&head.unique_ids_mutex);
                cf_hash_set(head.unique_ids,p->unid,p->unid_len,&one,sizeof(one));
                CF_UM(&head.unique_ids_mutex);

                len = snprintf(buff,512,"200 Ok\nTid: %llu\nMid: %llu\n",t->tid,p->mid);
                writen(sockfd,buff,len);

                if(Modules[NEW_POST_HANDLER].elements) {
                  for(i=0;i<Modules[NEW_POST_HANDLER].elements;i++) {
                    handler = array_element_at(&Modules[NEW_POST_HANDLER],i);
                    pfkt    = (t_srv_new_post_filter)handler->func;
                    ret     = pfkt(&fo_default_conf,&fo_server_conf,t->tid,p);
                  }
                }

                CF_RW_UN(&t->lock);
                cf_generate_cache(NULL);
              }
              else {
                CF_RW_UN(&t->lock);

                if(p->unid)       free(p->unid);
                if(p->category)   free(p->subject);
                if(p->category)   free(p->category);
                if(p->content)    free(p->content);
                if(p->user.name)  free(p->user.name);
                if(p->user.email) free(p->user.email);
                if(p->user.hp)    free(p->user.hp);
                if(p->user.img)   free(p->user.img);
                if(p->user.ip)    free(p->user.ip);
                free(p);
              }
            }
            else {
              if(p->unid)       free(p->unid);
              if(p->subject)    free(p->subject);
              if(p->category)   free(p->category);
              if(p->content)    free(p->content);
              if(p->user.name)  free(p->user.name);
              if(p->user.email) free(p->user.email);
              if(p->user.hp)    free(p->user.hp);
              if(p->user.img)   free(p->user.img);
              if(p->user.ip)    free(p->user.ip);

              free(p);
            }
          }
        }
      }
    }
    /* }}} */

    /* {{{ POST THREAD */
    else if(cf_strcmp(tokens[1],"THREAD") == 0) {
      p = fo_alloc(NULL,1,sizeof(t_posting),FO_ALLOC_CALLOC);
      t = fo_alloc(NULL,1,sizeof(t_thread),FO_ALLOC_CALLOC);

      if(cf_read_posting(p,sockfd,tsd)) {
        CF_RW_RD(&head.lock);
        if(sort_t == 2) t1 = head.thread;
        else            t1 = head.last;
        CF_RW_UN(&head.lock);

        if(t1) {
          /*
           * we only need to check the unid of the first posting in the last thread, because the user
           * tried to create a new thread and the first message of the thread before has to have the
           * same unid if this is a double posting created by pressing SUBMIT two times.
           *
           * This it not enough. The user could hit SUBMIT and wait 5 minutes. Meanwhile another
           * user could have postet a new thread. After that user 1 hits SUBMIT again and there we
           * are, a doubly posted thread. Unfortunately this really happened, so I established a
           * hash containing all unique ids.
           */
          CF_LM(&head.unique_ids_mutex);
          if(cf_hash_get(head.unique_ids,p->unid,p->unid_len) != NULL) {
            writen(sockfd,"502 Unid already got\n",21);
            cf_log(LOG_ERR,__FILE__,__LINE__,"Unid already got\n");
            err = 1;
          }
          CF_UM(&head.unique_ids_mutex);
        }

        if(err == 0) {
          CF_RW_WR(&head.lock);

          snprintf(buff,50,"t%lld",head.tid+1);
          cf_rwlock_init(buff,&t->lock);
          CF_RW_WR(&t->lock);

          t->postings  = p;
          t->last      = p;
          t->newest    = p;
          t->tid       = ++head.tid;
          t->posts     = 1;

          head.fresh   = 0;

          CF_LM(&head.unique_ids_mutex);
          cf_hash_set(head.unique_ids,p->unid,p->unid_len,&one,sizeof(one));
          CF_UM(&head.unique_ids_mutex);

          if(t1) {
            CF_RW_WR(&t1->lock);

            if(sort_t == 1) {
              t1->next    = t;
              t->prev     = t1;
            }
            else {
              t->next       = head.thread;
              head.thread   = t;
              t->next->prev = t;
            }

            CF_RW_UN(&t1->lock);
          }
          else {
            head.thread = t;
          }

          CF_RW_UN(&head.lock);

          if(Modules[NEW_THREAD_HANDLER].elements) {
            for(i=0;i<Modules[NEW_THREAD_HANDLER].elements;i++) {
              handler = array_element_at(&Modules[NEW_THREAD_HANDLER],i);
              tfkt    = (t_srv_new_thread_filter)handler->func;
              ret     = tfkt(&fo_default_conf,&fo_server_conf,t);
            }
          }

          cf_register_thread(t);
          CF_RW_UN(&t->lock);

          len = snprintf(buff,512,"200 Ok\nTid: %llu\nMid: %llu\n",t->tid,p->mid);
          writen(sockfd,buff,len);

          cf_generate_cache(NULL);
        }
        else {
          if(p->unid)       free(p->unid);
          if(p->subject)    free(p->subject);
          if(p->category)   free(p->category);
          if(p->content)    free(p->content);
          if(p->user.name)  free(p->user.name);
          if(p->user.email) free(p->user.email);
          if(p->user.hp)    free(p->user.hp);
          if(p->user.img)   free(p->user.img);
          if(p->user.ip)    free(p->user.ip);

          free(p);
          free(t);
        }
      }
      else {
        if(p->unid)       free(p->unid);
        if(p->subject)    free(p->subject);
        if(p->category)   free(p->category);
        if(p->content)    free(p->content);
        if(p->user.name)  free(p->user.name);
        if(p->user.email) free(p->user.email);
        if(p->user.hp)    free(p->user.hp);
        if(p->user.img)   free(p->user.img);
        if(p->user.ip)    free(p->user.ip);

        free(p);
        free(t);
      }
    }
    /* }}} */
  }
  /* }}} */
  else if(cf_strcmp(tokens[0],"PING") == 0) {
    writen(sockfd,"200 PONG!\n",10);
  }
  else {
    /* hu? how could this happen? */
    return FLT_DECLINE;
  }

  return FLT_OK;
}

int flt_cftp_register_handlers(int sock) {
  cf_register_protocol_handler("GET",flt_cftp_handler);
  cf_register_protocol_handler("POST",flt_cftp_handler);
  cf_register_protocol_handler("PING",flt_cftp_handler);
  cf_register_protocol_handler("STAT",flt_cftp_handler);

  return FLT_OK;
}

t_conf_opt flt_cftp_config[] = {
  { NULL, NULL, 0, NULL }
};

t_handler_config flt_cftp_handlers[] = {
  { INIT_HANDLER,            flt_cftp_register_handlers   },
  { 0, NULL }
};

t_module_config flt_cftp = {
  flt_cftp_config,
  flt_cftp_handlers,
  NULL,
  NULL,
  NULL,
  NULL
};

/* eof */
