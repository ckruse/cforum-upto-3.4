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

#include "serverutils.h"
#include "serverlib.h"
#include "fo_server.h"
#include "archiver.h"
/* }}} */

/* {{{ cf_run_archiver */
void cf_run_archiver(void) {
  thread_t *t,*oldest_t,*prev = NULL,**to_archive = NULL;
  long size,threadnum,pnum,max_bytes,max_threads,max_posts;
  int shall_archive = 0,len = 0,ret = FLT_OK,j,mb = 0;
  cf_name_value_t *max_bytes_v, *max_posts_v, *max_threads_v, *forums = cf_cfg_get_first_value(&fo_server_conf,NULL,"Forums");
  cf_handler_config_t *handler;
  archive_filter_t fkt;
  size_t i;
  forum_t *forum;

  for(i=0;i<forums->valnum;i++) {
    CF_RW_RD(&head.lock);
    forum = cf_hash_get(head.forums,forums->values[i],strlen(forums->values[i]));
    CF_RW_UN(&head.lock);

    max_bytes_v     = cf_cfg_get_first_value(&fo_server_conf,forum->name,"MainFileMaxBytes");
    max_posts_v     = cf_cfg_get_first_value(&fo_server_conf,forum->name,"MainFileMaxPostings");
    max_threads_v   = cf_cfg_get_first_value(&fo_server_conf,forum->name,"MainFileMaxThreads");

    max_bytes     = strtol(max_bytes_v->values[0],NULL,10);
    max_posts     = strtol(max_posts_v->values[0],NULL,10);
    max_threads   = strtol(max_threads_v->values[0],NULL,10);

    shall_archive = 0;
    len           = 0;
    ret           = FLT_OK;

    do {
      CF_RW_RD(&forum->lock);
      CF_RW_RD(&forum->threads.lock);

      mb            = 0;
      size          = forum->cache.invisible.len;
      t             = forum->threads.list;
      threadnum     = 0;
      pnum          = 0;
      shall_archive = 0;
      oldest_t      = NULL;

      CF_RW_UN(&forum->lock);
      CF_RW_UN(&forum->threads.lock);

      if(!t) break;

      /* since we have exclusive access to the messages, we need no longer locking to the messages itself */
      do {
        threadnum++;

        CF_RW_RD(&t->lock);

        pnum += t->posts;

        if(!oldest_t || t->newest->date < oldest_t->newest->date) oldest_t = t;

        prev = t;
        t    = t->next;

        CF_RW_UN(&prev->lock);
      } while(t);

      /* ok, we went through the hole threadlist. There we cannot slice very good, so yield */
      pthread_yield();

      if(size > max_bytes) {
        shall_archive = 1;
        mb = 1;
        cf_log(CF_STD,__FILE__,__LINE__,"Archiver: Criterium: max bytes, Values: Config: %ld, Real: %ld\n",max_bytes,size);
      }
      if(pnum > max_posts) {
        shall_archive = 1;
        cf_log(CF_STD,__FILE__,__LINE__,"Archiver: Criterium: max posts, Values: Config: %ld, Real: %ld\n",max_posts,pnum);
      }
      if(threadnum > max_threads) {
        shall_archive = 1;
        cf_log(CF_STD,__FILE__,__LINE__,"Archiver: Criterium: max threads, Values: Config: %ld, Real: %ld\n",max_threads,threadnum);
      }

      if(shall_archive) {
        to_archive        = cf_alloc(to_archive,++len,sizeof(thread_t *),CF_ALLOC_REALLOC);
        to_archive[len-1] = oldest_t;

        /*
        * This action is synchronized due to a mutex. So if the thread is
        * unregistered and pointers are re-set, everything is safe...
        */
        cf_unregister_thread(forum,oldest_t);
        cf_remove_thread(forum,oldest_t);

        /*
         * lock is no longer needed, it's been removed from the thread list; but
         * there may be threads waiting for this thread. This would be fatal, e.g.
         * if we destroy the complete thread it would lead into undefined behavior!
         *
         * To avoid the situation described above wie lock the thread exclusively.
         * This forces the scheduler to run the other threads first. And the exclusive
         * write lock ensures that no thread waits for this thread.
         */
        CF_RW_WR(&to_archive[len-1]->lock);
        CF_RW_UN(&to_archive[len-1]->lock);


        /* all references to this thread are released, so run the archiver plugins */
        if(Modules[ARCHIVE_HANDLER].elements) {
          ret = FLT_OK;

          for(i=0;i<Modules[ARCHIVE_HANDLER].elements && (ret == FLT_DECLINE || ret == FLT_OK);i++) {
            handler = cf_array_element_at(&Modules[ARCHIVE_HANDLER],i);
            fkt     = (archive_filter_t)handler->func;
            ret     = fkt(forum,oldest_t);
          }
        }

        if(ret == FLT_EXIT) {
          /* remove thread 'cause it wont be archived */
          cf_ar_remove_thread(forum,to_archive[len-1]);

          cf_cleanup_thread(to_archive[len-1]);
          free(to_archive[len-1]);

          if(len-1 == 0) {
            len = 0;
            free(to_archive);
            to_archive = NULL;
          }
          else to_archive = cf_alloc(to_archive,--len,sizeof(thread_t *),CF_ALLOC_REALLOC);
        }
      }

      /* in max-bytes-type we have to create new lists... THIS SUCKS! */
      if(mb) cf_generate_cache(forum);
    } while(shall_archive);

    /* after archiving, we re-generate the cache */
    cf_generate_cache(forum);

    /* ok, this may have token some time, so yield... */
    pthread_yield();

    cf_log(CF_STD|CF_FLSH,__FILE__,__LINE__,"archiver ran for forum %s. Writing threadlists...\n",forum->name);

    if(len) {
      cf_archive_threads(forum,to_archive,len);

      for(j=0;j<len;++j) {
        cf_cleanup_thread(to_archive[j]);
        free(to_archive[j]);
      }

      free(to_archive);
    }
  }

}
/* }}} */

/* {{{ cf_archive_threads */
void cf_archive_threads(forum_t *forum,thread_t **to_archive,size_t len) {
  int ret;
  size_t j;
  cf_handler_config_t *handler;
  archive_thread_t fkt;

  if(Modules[ARCHIVE_THREAD_HANDLER].elements) {
    ret = FLT_DECLINE;

    for(j=0;j<Modules[ARCHIVE_THREAD_HANDLER].elements && ret == FLT_DECLINE;j++) {
      handler = cf_array_element_at(&Modules[ARCHIVE_THREAD_HANDLER],j);
      fkt     = (archive_thread_t)handler->func;
      ret     = fkt(forum,to_archive,len);
    }
  }
}
/* }}} */

/* {{{ cf_write_threadlist */
void cf_write_threadlist(forum_t *forum) {
  int ret;
  size_t i,j,k;
  cf_name_value_t *forums = cf_cfg_get_first_value(&fo_server_conf,NULL,"Forums");
  cf_handler_config_t *handler;
  archive_thrdlst_writer_t fkt;

  if(forum) {
    if(Modules[THRDLST_WRITE_HANDLER].elements) {
      ret = FLT_DECLINE;

      for(i=0;i<Modules[THRDLST_WRITE_HANDLER].elements && ret == FLT_DECLINE;i++) {
        handler = cf_array_element_at(&Modules[THRDLST_WRITE_HANDLER],i);
        fkt     = (archive_thrdlst_writer_t)handler->func;

        cf_log(CF_DBG|CF_FLSH,__FILE__,__LINE__,"starting disc writer\n");
        ret     = fkt(forum);
        cf_log(CF_DBG|CF_FLSH,__FILE__,__LINE__,"Discwriter ended\n");

        if(ret == FLT_OK && Modules[THRDLST_WRITTEN_HANDLER].elements) {
          for(j=0;j<Modules[THRDLST_WRITTEN_HANDLER].elements;++j) {
            handler = cf_array_element_at(&Modules[THRDLST_WRITTEN_HANDLER],j);
            fkt     = (archive_thrdlst_writer_t)handler->func;

            fkt(forum);
          }
        }
      }
    }
  }
  else {
    if(Modules[THRDLST_WRITE_HANDLER].elements) {
      for(j=0;j<forums->valnum;++j) {
        forum = cf_hash_get(head.forums,forums->values[j],strlen(forums->values[j]));
        ret   = FLT_DECLINE;

        for(i=0;i<Modules[THRDLST_WRITE_HANDLER].elements && ret == FLT_DECLINE;i++) {
          handler = cf_array_element_at(&Modules[THRDLST_WRITE_HANDLER],i);
          fkt     = (archive_thrdlst_writer_t)handler->func;

          cf_log(CF_DBG|CF_FLSH,__FILE__,__LINE__,"starting disc writer\n");
          ret     = fkt(forum);
          cf_log(CF_DBG|CF_FLSH,__FILE__,__LINE__,"Discwriter ended\n");

          if(ret == FLT_OK && Modules[THRDLST_WRITTEN_HANDLER].elements) {
            for(k=0;k<Modules[THRDLST_WRITTEN_HANDLER].elements;++k) {
              handler = cf_array_element_at(&Modules[THRDLST_WRITTEN_HANDLER],k);
              fkt     = (archive_thrdlst_writer_t)handler->func;

              fkt(forum);
            }
          }
        }
      }
    }
  }

}
/* }}} */

/* {{{ cf_archive_thread */
int cf_archive_thread(forum_t *forum,u_int64_t tid) {
  thread_t *t = cf_get_thread(forum,tid);
  thread_t **list;

  if(t) {
    cf_unregister_thread(forum,t);
    cf_remove_thread(forum,t);

    list = cf_alloc(NULL,1,sizeof(*list),CF_ALLOC_MALLOC);
    list[0] = t;

    cf_archive_threads(forum,list,1);
  }

  return 0;
}
/* }}} */

/* {{{ cf_remove_thread */
void cf_ar_remove_thread(forum_t *forum,thread_t *thr) {
  int ret;
  size_t j;
  cf_handler_config_t *handler;
  remove_thread_t fkt;

  if(Modules[REMOVE_THREAD_HANDLER].elements) {
    ret = FLT_DECLINE;

    for(j=0;j<Modules[REMOVE_THREAD_HANDLER].elements && ret == FLT_DECLINE;j++) {
      handler = cf_array_element_at(&Modules[REMOVE_THREAD_HANDLER],j);
      fkt     = (remove_thread_t)handler->func;
      ret     = fkt(forum,thr);
    }
  }
}
/* }}} */

/* eof */
