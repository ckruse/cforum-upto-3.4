/**
 * \file flt_cftp.c
 * \author Christian Kruse, <cjk@wwwtech.de>
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
#include <inttypes.h>

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

int flt_cftp_handler(cf_configuration_t *cfg,int sockfd,forum_t *forum,const u_char **tokens,int tnum,rline_t *tsd) {
  cf_cfg_config_value_t *sort_t_v = cf_cfg_get_value_w_nam(cfg,forum->name,"FS:SortThreads",1),
               *sort_m_v = cf_cfg_get_value_w_nam(cfg,forum->name,"FS:SortMessages",1);

  int sort_m = cf_strcmp(sort_m_v->sval,"ascending") == 0 ? CF_SORT_ASCENDING : CF_SORT_DESCENDING,
      sort_t = cf_strcmp(sort_t_v->sval,"ascending") == 0 ? CF_SORT_ASCENDING : CF_SORT_DESCENDING,
      ret;

  size_t i,
         l,
         err = 0,
         one = 1;

  cf_handler_config_t *handler;

  u_int64_t tid,
            mid;

  thread_t *t,
           *t1;

  posting_t *p,
            *p1,
            *p2;

  cf_string_t str;
  u_char buff[512];
  srv_new_post_filter_t pfkt;
  srv_new_thread_filter_t tfkt;

  /* {{{ get */
  if(cf_strcmp(tokens[0],"GET") == 0) {
    if(tnum >= 2) {
      cf_log(CF_DBG,__FILE__,__LINE__,"%s %s\n",tokens[0],tokens[1]);

      /* {{{ GET THREADLIST */
      if(cf_strcmp(tokens[1],"THREADLIST") == 0) {
        CF_RW_RD(&forum->lock);

        if(tnum == 3 && cf_strcmp(tokens[2],"invisible=1") == 0) writen(sockfd,forum->cache.invisible.content,forum->cache.invisible.len);
        else writen(sockfd,forum->cache.visible.content,forum->cache.visible.len);

        CF_RW_UN(&forum->lock);
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
          tid = cf_str_to_uint64(tokens[2]+1),
          mid = cf_str_to_uint64(tokens[3]+1);

          cf_send_posting(forum,sockfd,tid,mid,tnum == 5 && cf_strcmp(tokens[4],"invisible=1") == 0);
        }
      }
      /* }}} */

      /* {{{ GET LASTMODIFIED */
      else if(cf_strcmp(tokens[1],"LASTMODIFIED") == 0) {
        CF_RW_RD(&head.lock);

        if(tnum == 3 && *tokens[2] == '1') l = snprintf(buff,50,"%ld\n",forum->date.invisible);
        else l = snprintf(buff,50,"%ld\n",forum->date.visible);

        CF_RW_UN(&head.lock);

        writen(sockfd,buff,l);
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

        writen(sockfd,str.content,str.len);
        cf_str_cleanup(&str);
      }
      /* }}} */

      /* {{{ GET TIDLIST */
      else if(cf_strcmp(tokens[1],"TIDLIST") == 0) {
        cf_str_init(&str);
        cf_str_chars_append(&str,"200 List Follows\n",17);

        CF_RW_RD(&forum->threads.lock);
        t = forum->threads.list;
        CF_RW_UN(&forum->threads.lock);

        for(;t;t=t1) {
          CF_RW_RD(&t->lock);

          cf_str_char_append(&str,'t');
          cf_uint64_to_str(&str,t->tid);
          cf_str_char_append(&str,'\n');

          t1 = t->next;
          CF_RW_UN(&t->lock);
        }

        cf_str_char_append(&str,'\n');

        writen(sockfd,str.content,str.len);
        cf_str_cleanup(&str);
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
    /* {{{ STAT THREAD */
    if(tnum == 3 && cf_strcmp(tokens[1],"THREAD") == 0) {
      tid = cf_str_to_uint64(tokens[2]+1);

      if(cf_get_thread(forum,tid)) {
        writen(sockfd,"200 Exists\n",11);
      }
      else {
        writen(sockfd,"404 Thread not found\n",21);
      }
    }
    /* }}} */

    /* {{{ STAT POST */
    else if(tnum == 4 && cf_strcmp(tokens[1],"POST") == 0) {
      tid = cf_str_to_uint64(tokens[2]+1);
      mid = cf_str_to_uint64(tokens[3]+1);

      if((t = cf_get_thread(forum,tid)) != NULL) {
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
      p = cf_alloc(NULL,1,sizeof(posting_t),CF_ALLOC_CALLOC);

      if(tnum != 4) {
        writen(sockfd,"500 Sorry\n",10);
        cf_log(CF_ERR,__FILE__,__LINE__,"Bad request\n");
      }
      else {
        tid = cf_str_to_uint64(tokens[2]+2);
        mid = cf_str_to_uint64(tokens[3]+2);

        t  = cf_get_thread(forum,tid);

        if(!t) {
          writen(sockfd,"404 Thread Not Found\n",21);
          cf_log(CF_ERR,__FILE__,__LINE__,"Thread not found\n");
          err = 1;
        }
        else {
          p1 = cf_get_posting(t,mid);
          CF_RW_RD(&t->lock);

          if(!p1) {
            writen(sockfd,"404 Posting Not Found\n",22);
            cf_log(CF_ERR,__FILE__,__LINE__,"Posting not found\n");
            err = 1;

            CF_RW_UN(&t->lock);
          }
          else {
            CF_RW_UN(&t->lock);

            if(cf_read_posting(forum,p,sockfd,tsd)) {
              CF_RW_WR(&t->lock);

              if(p1->invisible == 1 && p->invisible == 0) {
                writen(sockfd,"404 Posting Not Found\n",22);
                cf_log(CF_ERR,__FILE__,__LINE__,"p1->invisible = 1 and p->invisible = 0\n");
                err = 1;
              }

              if(err == 0) {
                CF_LM(&forum->uniques.lock);
                if(cf_hash_get(forum->uniques.ids,p->unid.content,p->unid.len) != NULL) {
                  writen(sockfd,"502 Unid already got\n",21);
                  cf_log(CF_ERR,__FILE__,__LINE__,"Unid already got\n");
                  err = 1;
                }
                CF_UM(&forum->uniques.lock);
              }

              if(err == 0) {
                p->level     = p1->level + 1;
                p->invisible = 0;
                t->newest    = p;
                t->posts    += 1;

                if(sort_m == CF_SORT_DESCENDING || p1->next == NULL) {
                  p->next       = p1->next;
                  p1->next      = p;
                  p->prev       = p1;

                  if(p->next) p->next->prev = p;
                  else        t->last = p;
                }
                else {
                  /* let's find the end of the subtree */
                  for(p2=p1->next;p2->next && p2->level > p1->level;p2=p2->next);
                  if(p2->level > p1->level) {
                    p2->next = p;
                    p->prev = p2;
                  }
                  else {
                    p->next = p2;
                    p->prev = p2->prev;
                    p2->prev->next = p;
                    p2->prev = p;
                  }
                }

                CF_LM(&forum->uniques.lock);
                cf_hash_set(forum->uniques.ids,p->unid.content,p->unid.len,&one,sizeof(one));
                CF_UM(&forum->uniques.lock);

                /* {{{ create answer */
                cf_str_init_growth(&str,128);
                cf_str_chars_append(&str,"200 Ok\nTid: ",12);
                cf_uint64_to_str(&str,t->tid);
                cf_str_chars_append(&str,"\nMid: ",6);
                cf_uint64_to_str(&str,p->mid);
                cf_str_chars_append(&str,"\n\n",2);
                writen(sockfd,str.content,str.len);
                cf_str_cleanup(&str);
                /* }}} */

                if(Modules[NEW_POST_HANDLER].elements) {
                  for(i=0;i<Modules[NEW_POST_HANDLER].elements;i++) {
                    handler = cf_array_element_at(&Modules[NEW_POST_HANDLER],i);
                    pfkt    = (srv_new_post_filter_t)handler->func;
                    ret     = pfkt(forum,t->tid,p);
                  }
                }

                CF_RW_UN(&t->lock);
                cf_generate_cache(forum);
              }
              else {
                CF_RW_UN(&t->lock);

                cf_cleanup_posting(p);
                free(p);
              }
            }
            else {
              cf_cleanup_posting(p);
              free(p);
            }
          }
        }
      }
    }
    /* }}} */

    /* {{{ POST THREAD */
    else if(cf_strcmp(tokens[1],"THREAD") == 0) {
      p = cf_alloc(NULL,1,sizeof(*p),CF_ALLOC_CALLOC);
      t = cf_alloc(NULL,1,sizeof(*t),CF_ALLOC_CALLOC);

      if(cf_read_posting(forum,p,sockfd,tsd)) {
        CF_RW_RD(&forum->threads.lock);
        if(sort_t == CF_SORT_DESCENDING) t1 = forum->threads.list;
        else                             t1 = forum->threads.last;
        CF_RW_UN(&forum->threads.lock);

        if(t1) {
          CF_LM(&forum->uniques.lock);
          if(cf_hash_get(forum->uniques.ids,p->unid.content,p->unid.len) != NULL) {
            writen(sockfd,"502 Unid already got\n",21);
            cf_log(CF_ERR,__FILE__,__LINE__,"Unid already got\n");
            err = 1;
          }
          CF_UM(&forum->uniques.lock);
        }

        if(err == 0) {
          CF_RW_WR(&forum->threads.lock);

          snprintf(buff,50,"t%" PRIu64,forum->threads.last_tid+1);
          cf_rwlock_init(buff,&t->lock);
          CF_RW_WR(&t->lock);

          t->postings  = p;
          t->last      = p;
          t->newest    = p;
          t->tid       = ++forum->threads.last_tid;
          t->posts     = 1;

          CF_RW_WR(&forum->lock);
          forum->cache.fresh = 0;
          CF_RW_UN(&forum->lock);

          CF_LM(&forum->uniques.lock);
          cf_hash_set(forum->uniques.ids,p->unid.content,p->unid.len,&one,sizeof(one));
          CF_UM(&forum->uniques.lock);

          if(t1) {
            CF_RW_WR(&t1->lock);

            if(sort_t == CF_SORT_ASCENDING) {
              t1->next    = t;
              t->prev     = t1;
              forum->threads.last = t;
            }
            else {
              t->next = forum->threads.list;
              forum->threads.list = t;
              t->next->prev = t;
            }

            CF_RW_UN(&t1->lock);
          }
          else {
            forum->threads.list = forum->threads.last = t;
          }

          CF_RW_UN(&forum->threads.lock);

          if(Modules[NEW_THREAD_HANDLER].elements) {
            for(i=0;i<Modules[NEW_THREAD_HANDLER].elements;i++) {
              handler = cf_array_element_at(&Modules[NEW_THREAD_HANDLER],i);
              tfkt    = (srv_new_thread_filter_t)handler->func;
              ret     = tfkt(forum,t);
            }
          }

          cf_register_thread(forum,t);
          CF_RW_UN(&t->lock);

          /* {{{ create answer */
          cf_str_init_growth(&str,128);
          cf_str_chars_append(&str,"200 Ok\nTid: ",12);
          cf_uint64_to_str(&str,t->tid);
          cf_str_chars_append(&str,"\nMid: ",6);
          cf_uint64_to_str(&str,p->mid);
          cf_str_chars_append(&str,"\n\n",2);
          writen(sockfd,str.content,str.len);
          cf_str_cleanup(&str);
          /* }}} */

          cf_generate_cache(forum);
        }
        else {
          cf_cleanup_posting(p);
          free(p);
          free(t);
        }
      }
      else {
        cf_cleanup_posting(p);
        free(p);
        free(t);
      }
    }
    /* }}} */
  }
  /* }}} */
  /* {{{ flags */
  else if(cf_strcmp(tokens[0],"FLAG") == 0) {
    if(tnum < 2) return FLT_DECLINE;

    /* {{{ set */
    if(cf_strcmp(tokens[1],"SET") == 0) {
      if(tnum != 4) {
        writen(sockfd,"500 Sorry\n",10);
        cf_log(CF_ERR,__FILE__,__LINE__,"Bad request\n");
        return FLT_OK;
      }

      tid = cf_str_to_uint64(tokens[2]+1);
      mid = cf_str_to_uint64(tokens[3]+1);

      t  = cf_get_thread(forum,tid);

      if(!t) {
        writen(sockfd,"404 Thread Not Found\n",21);
        cf_log(CF_ERR,__FILE__,__LINE__,"Thread not found\n");
        return FLT_OK;
      }

      p1 = cf_get_posting(t,mid);

      if(!p1) {
        writen(sockfd,"404 Posting Not Found\n",22);
        cf_log(CF_ERR,__FILE__,__LINE__,"Posting not found\n");
        return FLT_OK;
      }

      CF_RW_RD(&t->lock);
      ret = cf_read_flags(sockfd,tsd,p1);
      CF_RW_UN(&t->lock);

      if(ret == 0) writen(sockfd,"200 Ok\n",7);

      cf_generate_cache(forum);

      return FLT_OK;
    }
    /* }}} */
    /* {{{ remove */
    else if(cf_strcmp(tokens[1],"REMOVE") == 0) {
      if(tnum != 4) {
        writen(sockfd,"500 Sorry\n",10);
        cf_log(CF_ERR,__FILE__,__LINE__,"Bad request\n");
        return FLT_OK;
      }

      tid = cf_str_to_uint64(tokens[2]+1);
      mid = cf_str_to_uint64(tokens[3]+1);

      t  = cf_get_thread(forum,tid);

      if(!t) {
        writen(sockfd,"404 Thread Not Found\n",21);
        cf_log(CF_ERR,__FILE__,__LINE__,"Thread not found\n");
        return FLT_OK;
      }

      p1 = cf_get_posting(t,mid);

      if(!p1) {
        writen(sockfd,"404 Posting Not Found\n",22);
        cf_log(CF_ERR,__FILE__,__LINE__,"Posting not found\n");
        return FLT_OK;
      }

      CF_RW_RD(&t->lock);
      ret = cf_remove_flags(sockfd,tsd,p1);
      CF_RW_UN(&t->lock);

      if(ret == 0) writen(sockfd,"200 Ok\n",7);

      cf_generate_cache(forum);

      return FLT_OK;
    }
    /* }}} */
    else {
      writen(sockfd,"500 Sorry\n",10);
      cf_log(CF_ERR,__FILE__,__LINE__,"Bad request\n");
      return FLT_OK;
    }
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
  cf_register_protocol_handler("FLAG",flt_cftp_handler);

  return FLT_OK;
}

cf_handler_config_t flt_cftp_handlers[] = {
  { INIT_HANDLER, flt_cftp_register_handlers   },
  { 0, NULL }
};

cf_module_config_t flt_cftp = {
  MODULE_MAGIC_COOKIE,
  flt_cftp_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};

/* eof */
