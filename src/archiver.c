/**
 * \file archiver.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 * \brief The archiver functions
 *
 * This file contains the archiver functions. The archiver is complex enough to
 * give him an own file.
 *
 * \todo Archive only if voting is high enough
 * \todo Implement indexer for the "SELFHTML Suche" and the "Neue SELFHTML Suche"
 *
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

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>

#include <gdome.h>

struct sockaddr_un;

#include "cf_pthread.h"

#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "readline.h"

#include "serverlib.h"
#include "fo_server.h"
#include "archiver.h"
/* }}} */


void cf_run_archiver(void) {
  t_thread *t,*oldest_t,*prev = NULL,**to_archive = NULL,*oldest_prev,*max_posts_t;
  t_posting *oldest,*newest_in_t;
  long size,threadnum,pnum,max_bytes,max_threads,max_posts;
  int shall_archive = 0,len = 0,ret = FLT_OK;
  t_name_value *max_bytes_v, *max_posts_v, *max_threads_v, *forums = cfg_get_value(&fo_server_conf,NULL,"Forums");
  t_handler_config *handler;
  t_archive_filter fkt;
  size_t i;
  u_char *fname;
  t_forum *forum;
  t_posting *p,*p1;

  for(i=0;i<forums->valnum;i++) {
    CF_RW_RD(&head.lock);
    forum = cf_hash_get(head.forums,forums->values[i],strlen(forums->values[i]));
    CF_RW_UN(&head.lock);

    max_bytes_v     = cfg_get_first_value(&fo_server_conf,forum->name,"MainFileMaxBytes");
    max_posts_v     = cfg_get_first_value(&fo_server_conf,forum->name,"MainFileMaxPostings");
    max_threads_v   = cfg_get_first_value(&fo_server_conf,forum->name,"MainFileMaxThreads");

    max_bytes     = strtol(max_bytes_v->values[0],NULL,10);
    max_posts     = strtol(max_posts_v->values[0],NULL,10);
    max_threads   = strtol(max_threads_v->values[0],NULL,10);


    do {
      CF_RW_RD(&forum->lock);

      size          = forum->cache.invisible.len;
      t             = forum->threads.list;
      threadnum     = 0;
      pnum          = 0;
      shall_archive = 0;
      oldest        = NULL;
      oldest_t      = NULL;
      oldest_prev   = NULL;
      newest_in_t   = NULL;

      CF_RW_UN(&forum->lock);

      if(!t) return;

      /* since we have exclusive access to the messages, we need no longer locking to the messages itself */
      do {
        threadnum++;

        CF_RW_RD(&t->lock);

        newest_in_t = t->newest;
        pnum       += t->posts;

        if(!max_posts_t || max_posts_t->posts < t->posts) max_posts_t = t;

        if(!oldest || newest_in_t->date < oldest->date) {
          oldest      = newest_in_t;
          oldest_t    = t;
          oldest_prev = prev;
        }

        prev = t;
        t    = t->next;

        CF_RW_UN(&prev->lock);
      } while(t);

      /* ok, we went through the hole threadlist. There we cannot slice very good, so yield */
      pthread_yield();

      if(size > max_bytes) {
        shall_archive = 1;
        cf_log(LOG_STD,__FILE__,__LINE__,"Archiver: Criterium: max bytes, Values: Config: %ld, Real: %ld\n",max_bytes,size);
      }
      if(pnum > max_posts) {
        shall_archive = 1;
        cf_log(LOG_STD,__FILE__,__LINE__,"Archiver: Criterium: max posts, Values: Config: %ld, Real: %ld\n",max_posts,pnum);
      }
      if(threadnum > max_threads) {
        shall_archive = 1;
        cf_log(LOG_STD,__FILE__,__LINE__,"Archiver: Criterium: max threads, Values: Config: %ld, Real: %ld\n",max_threads,threadnum);
      }

      if(shall_archive) {
        to_archive        = fo_alloc(to_archive,++len,sizeof(t_thread *),FO_ALLOC_REALLOC);
        to_archive[len-1] = oldest_t;

        /*
        * This action is synchronized due to a mutex. So if the thread is
        * unregistered and pointers are re-set, everything is safe...
        */
        cf_unregister_thread(forum,oldest_t);

        /*
        * if we lock oldest_t before oldest_prev this
        * could cause a dead lock or some undefined behavior
        */
        if(oldest_prev) CF_RW_WR(&oldest_prev->lock);

        CF_RW_WR(&oldest_t->lock);

        if(oldest_prev) {
          oldest_prev->next       = oldest_t->next;
          if(oldest_prev->next) oldest_prev->next->prev = oldest_prev; /* is NULL if the last thread is being archived */

          CF_RW_UN(&oldest_prev->lock);
        }
        else {
          CF_RW_WR(&head.lock);
          head.thread = oldest_t->next;
          CF_RW_UN(&head.lock);
        }

        /* all references to this thread are released, so run the archiver plugins */
        if(Modules[ARCHIVE_HANDLER].elements) {
          ret = FLT_OK;

          for(i=0;i<Modules[ARCHIVE_HANDLER].elements && (ret == FLT_DECLINE || ret == FLT_OK);i++) {
            handler = array_element_at(&Modules[ARCHIVE_HANDLER],i);
            fkt     = (t_archive_filter)handler->func;
            ret     = fkt(oldest_t);
          }
        }

        if(ret == FLT_EXIT) {
          for(p=to_archive[len-1]->postings;p;p=p1) {
            free(p->user.name);
            free(p->user.ip);
            free(p->subject);
            free(p->unid);
            free(p->content);

            if(p->user.email) free(p->user.email);
            if(p->user.img) free(p->user.img);
            if(p->user.hp) free(p->user.hp);
            if(p->category) free(p->category);

            p1 = p->next;
            free(p);
          }

          free(to_archive[len-1]);
          to_archive = fo_alloc(to_archive,--len,sizeof(t_thread *),FO_ALLOC_REALLOC);
        }
      }
    } while(shall_archive);


    /* after archiving, we re-generate the cache */
    cf_generate_cache(forum);

    /* ok, this may have token some time, so yield... */
    pthread_yield();

    cf_log(LOG_STD,__FILE__,__LINE__,"archiver ran for forum %s. Writing threadlists...\n",forum->name);

    cf_write_threadlist(forum);

    if(len) {
      cf_archive_threads(forum,to_archive,len);

      for(i=0;i<len;i++) {
        for(p=to_archive[i]->postings;p;p=p1) {
          free(p->user.name);
          free(p->user.ip);
          free(p->subject);
          free(p->unid);
          free(p->content);
          if(p->user.email) free(p->user.email);
          if(p->user.img) free(p->user.img);
          if(p->user.hp) free(p->user.hp);
          if(p->category) free(p->category);

          p1 = p->next;
          free(p);
        }

        CF_RW_UN(&to_archive[i]->lock);
        cf_rwlock_destroy(&to_archive[i]->lock);
        free(to_archive[i]);
      }

      free(to_archive);
    }
  }

}

void cf_archive_threads(t_forum *forum,t_thread **to_archive,int len) {
  return 0;
}

void cf_write_threadlist(t_forum *forum) {
}

int cf_archive_thread(t_forum *forum,u_int64_t tid) {
  return 0;
}

/* eof */
