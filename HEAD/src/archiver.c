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

#include "cf_pthread.h"

#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "readline.h"
#include "fo_server.h"
#include "serverlib.h"
#include "xml_handling.h"
#include "charconvert.h"
#include "archiver.h"
/* }}} */

/* {{{ cf_delete_threadfile */
void cf_delete_threadfile(t_thread *t) {
  t_name_value *msgdir = cfg_get_first_value(&fo_default_conf,"MessagePath");
  u_char buff[256];

  snprintf(buff,256,"%s/t%lld.xml",msgdir->values[0],t->tid);
  unlink(buff);
}
/* }}} */

/* {{{ cf_get_time */
time_t cf_get_time(GdomeNode *n) {
  time_t ret = 0;
  GdomeException e;
  GdomeDOMString *dstr = gdome_str_mkref("Date");
  GdomeNodeList  *nl   = gdome_el_getElementsByTagName((GdomeElement *)n,dstr,&e);

  if(nl) {
    GdomeDOMString    *ls_str = gdome_str_mkref("longSec");
    GdomeNode         *n1     = gdome_nl_item(nl,0,&e);
    GdomeNamedNodeMap *nnm    = gdome_n_attributes(n1,&e);
    GdomeNode         *date   = gdome_nnm_getNamedItem(nnm,ls_str,&e);
    u_char             *dt     = get_node_value(date);

    if(dt) {
      ret = strtol(dt,NULL,10);
      free(dt);
    }

    gdome_n_unref(date,&e);
    gdome_str_unref(ls_str);
    gdome_nnm_unref(nnm,&e);
    gdome_n_unref(n1,&e);

    gdome_nl_unref(nl,&e);
  }

  gdome_str_unref(dstr);

  return ret;
}
/* }}} */

/* {{{ cf_make_path */
void cf_make_path(u_char *path) {
  register u_char *ptr = path+1;
  int ret;

  /*
   * search whole string for a directory separator
   */
  for (;*ptr;ptr++) {
    /*
     * when a directory separator is given, create path 'till there
     */
    if (*ptr == '/') {
      *ptr = '\0';
      ret = mkdir(path,S_IRWXU|S_IRWXG|S_IRWXO);
      *ptr = '/';

      if (ret && errno != EEXIST) {
        cf_log(LOG_ERR,__FILE__,__LINE__,"mkdir: %s",strerror(errno));
        return;
      }
    }
  }

  return;
}
/* }}} */

/* {{{ cf_archive_threads */
void cf_archive_threads(t_thread **to_archive,int len) {
  t_posting *p,*p1;
  GdomeException e;
  GdomeDOMImplementation *impl = gdome_di_mkref();
  GdomeDocument *doc1          = NULL,*doc2;
  t_name_value  *apath         = cfg_get_first_value(&fo_default_conf,"ArchivePath");

  GdomeDOMString *str_thread   = gdome_str_mkref("Thread");

  GdomeElement *thread1;
  GdomeElement *thread2;
  GdomeElement *msgcnt;

  GdomeNodeList *nl;
  GdomeNode *parent_thr;

  struct tm *tm;
  int i,j,nlen,mon = -1,year = -1;
  u_char buff[256];
  struct stat st;

  for(i=0;i<len;i++) {
    parent_thr = NULL;
    tm = localtime(&to_archive[i]->postings->date);

    if(tm->tm_mon != mon || tm->tm_year != year) {
      if(doc1) {
        snprintf(buff,256,"%s/%d/%d/index.xml",apath->values[0],year+1900,mon+1);
        if(!gdome_di_saveDocToFile(impl,doc1,buff,0,&e)) {
          cf_log(LOG_ERR,__FILE__,__LINE__,"ERROR! COULD NOT WRITE XML FILE!\n");
        }
        gdome_doc_unref(doc1,&e);
      }

      snprintf(buff,256,"%s/%d/%d/index.xml",apath->values[0],tm->tm_year+1900,tm->tm_mon+1);
      cf_make_path(buff);

      if(stat(buff,&st) == 0) {
        if((doc1 = gdome_di_createDocFromURI(impl,buff,GDOME_LOAD_PARSING,&e)) == NULL) {
          cf_log(LOG_ERR,__FILE__,__LINE__,"ERROR! COULD NOT READ XML FILE!\n");
          return;
        }
      }
      else {
        doc1 = xml_create_doc(impl,FORUM_DTD);
      }

      mon  = tm->tm_mon;
      year = tm->tm_year;
    }

    cf_log(LOG_STD,__FILE__,__LINE__,"Archiver: archiving thread t%lld\n",to_archive[i]->tid);

    doc2    = xml_create_doc(impl,FORUM_DTD);

    /* first lets create the necessary elements */
    thread1 = xml_create_element(doc1,"Thread");
    thread2 = xml_create_element(doc2,"Thread");
    msgcnt  = xml_create_element(doc2,"ContentList");

    /* lets set the tid attribute */
    sprintf(buff,"t%lld",to_archive[i]->tid);
    xml_set_attribute(thread1,"id",buff);
    xml_set_attribute(thread2,"id",buff);

    /* find the previous sibling of the thread: search all thread elements, ... */
    nl   = gdome_doc_getElementsByTagName(doc1,str_thread,&e);
    nlen = gdome_nl_length(nl,&e);

    /* and then search the right thread element */
    for(j=0;j<nlen;j++) {
      parent_thr = gdome_nl_item(nl,j,&e);
      if(cf_get_time(parent_thr) < to_archive[i]->postings->date) {
        break;
      }

      gdome_n_unref(parent_thr,&e);
    }

    stringify_posting(doc1,thread1,doc2,thread2,to_archive[i]->postings);

    if(j == nlen || j == 0) {
      GdomeElement *root = gdome_doc_documentElement(doc1,&e);
      gdome_el_appendChild(root,(GdomeNode *)thread1,&e);
      gdome_el_unref(root,&e);
    }
    else {
      GdomeElement *root = gdome_doc_documentElement(doc1,&e);
      gdome_n_insertBefore((GdomeNode *)root,(GdomeNode *)thread1,parent_thr,&e);
      gdome_el_unref(root,&e);
    }

    for(p=to_archive[i]->postings;p;p=p1) {
      GdomeDOMString *str;
      GdomeElement *el = xml_create_element(doc2,"MessageContent");
      GdomeCDATASection *cd;

      sprintf(buff,"m%lld",p->mid);
      xml_set_attribute(el,"mid",buff);

      str = gdome_str_mkref_dup(p->content);

      cd = gdome_doc_createCDATASection(doc2,str,&e);
      gdome_el_appendChild(el,(GdomeNode *)cd,&e);
      gdome_el_appendChild(msgcnt,(GdomeNode *)el,&e);

      gdome_str_unref(str);
      gdome_el_unref(el,&e);
      gdome_cds_unref(cd,&e);

      free(p->user.name);
      free(p->user.ip);
      free(p->subject);
      free(p->unid);
      free(p->content);
      if(p->user.email) free(p->user.email);
      if(p->user.img)   free(p->user.img);
      if(p->user.hp)    free(p->user.hp);
      if(p->category)   free(p->category);

      p1 = p->next;
      free(p);
    }

    if(parent_thr) gdome_n_unref(parent_thr,&e);

    parent_thr = (GdomeNode *)gdome_doc_documentElement(doc2,&e);
    gdome_n_appendChild(parent_thr,(GdomeNode *)thread2,&e);
    gdome_n_appendChild(parent_thr,(GdomeNode *)msgcnt,&e);
    gdome_n_unref(parent_thr,&e);

    gdome_nl_unref(nl,&e);
    gdome_el_unref(msgcnt,&e);
    gdome_el_unref(thread1,&e);
    gdome_el_unref(thread2,&e);

    cf_rwlock_destroy(&to_archive[i]->lock);
    cf_delete_threadfile(to_archive[i]);

    snprintf(buff,256,"%s/%d/%d/t%lld.xml",apath->values[0],year+1900,mon+1,to_archive[i]->tid);
    if(!gdome_di_saveDocToFile(impl,doc2,buff,0,&e)) {
      cf_log(LOG_ERR,__FILE__,__LINE__,"ERROR! COULD NOT WRITE XML FILE!\n");
    }
    gdome_doc_unref(doc2,&e);

    free(to_archive[i]);
  }

  snprintf(buff,256,"%s/%d/%d/index.xml",apath->values[0],year+1900,mon+1);
  if(!gdome_di_saveDocToFile(impl,doc1,buff,0,&e)) {
    cf_log(LOG_ERR,__FILE__,__LINE__,"ERROR! COULD NOT WRITE XML FILE!\n");
  }
  gdome_doc_unref(doc1,&e);

  gdome_str_unref(str_thread);
  gdome_di_unref(impl,&e);
}
/* }}} */

/* {{{ cf_run_archiver_and_write_to_disk
 * Returns: nothing
 * Parameters:
 *
 * This function writes everything to disk and runs the archiver.
 *
 */
void cf_run_archiver_and_write_to_disk(void) {
  t_thread *t,*oldest_t,*prev = NULL,**to_archive = NULL,*oldest_prev;
  t_posting *oldest,*newest_in_t;
  long size,threadnum,pnum,max_bytes,max_threads,max_posts;
  int shall_archive = 0,len = 0,ret = FLT_OK;
  t_name_value *max_bytes_v     = cfg_get_first_value(&fo_server_conf,"MainFileMaxBytes");
  t_name_value *max_posts_v     = cfg_get_first_value(&fo_server_conf,"MainFileMaxPostings");
  t_name_value *max_threads_v   = cfg_get_first_value(&fo_server_conf,"MainFileMaxThreads");
  GdomeDOMImplementation *impl = gdome_di_mkref();
  GdomeException e;
  GdomeDocument *doc           = xml_create_doc(impl,FORUM_DTD);
  GdomeElement *el             = gdome_doc_documentElement(doc,&e);
  t_name_value  *mpath         = cfg_get_first_value(&fo_default_conf,"MessagePath");
  u_char buff[256];
  pid_t pid;
  int status;
  size_t i;
  t_handler_config *handler;
  t_archive_filter fkt;

  max_bytes   = strtol(max_bytes_v->values[0],NULL,10);
  max_posts   = strtol(max_posts_v->values[0],NULL,10);
  max_threads = strtol(max_threads_v->values[0],NULL,10);

  do {
    CF_RW_RD(&head.lock);

    size          = head.cache_invisible.len;
    t             = head.thread;
    threadnum     = 0;
    pnum          = 0;
    shall_archive = 0;
    oldest        = NULL;
    oldest_t      = NULL;
    oldest_prev   = NULL;
    newest_in_t   = NULL;

    CF_RW_UN(&head.lock);

    if(!t) return;

    /* since we have exclusive access to the messages, we need no longer locking to the messages itself */
    do {
      threadnum++;

      CF_RW_RD(&t->lock);

      newest_in_t = t->newest;
      pnum       += t->posts;

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
      cf_unregister_thread(oldest_t);

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
        t_posting *p,*p1;
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
  cf_generate_cache(NULL);

  /* ok, this may have token some time, so yield... */
  pthread_yield();

  cf_log(LOG_STD,__FILE__,__LINE__,"archiver ran. Writing threadlists...\n");

  CF_RW_RD(&head.lock);
  t = head.thread;
  CF_RW_UN(&head.lock);

  sprintf(buff,"m%lld",head.mid);
  xml_set_attribute(el,"lastMessage",buff);

  sprintf(buff,"t%lld",head.tid);
  xml_set_attribute(el,"lastThread",buff);

  gdome_el_unref(el,&e);

  /*
   * *very* nasty workaround for a memory leek in the gdome lib
   *
   * Hm, this has a nice effect: normally when the archiver runs, the
   * server would lag a little bit. But since all expensive operations
   * are done in a child process, the server itself would not lag longer
   */
  pid = fork();
  switch(pid) {
  case -1:
    cf_log(LOG_ERR,__FILE__,__LINE__,"fork: %s\n",strerror(errno));
    break;

  case 0:
    /* we write the threadlist */
    while(t) {
      /* we need no locking in the child process */
      stringify_thread_and_write_to_disk(doc,t);
      t = t->next;
    };

    snprintf(buff,256,"%s/forum.xml",mpath->values[0]);

    if(!gdome_di_saveDocToFile(impl,doc,buff,0,&e)) {
      cf_log(LOG_ERR,__FILE__,__LINE__,"ERROR! COULD NOT WRITE XML FILE!\n");
    }
    gdome_doc_unref(doc,&e);

    exit(0);
    break;

  default:
    cf_log(LOG_STD,__FILE__,__LINE__,"writing threadlist...\n");
    waitpid(pid,&status,0);
    cf_log(LOG_STD,__FILE__,__LINE__,"finished writing threadlist!\n");
    break;
  }

  gdome_doc_unref(doc,&e);

  if(len) {
    long i;
    pid = fork();

    /* nasty workaround... */
    switch(pid) {
    case -1:
      cf_log(LOG_ERR,__FILE__,__LINE__,"fork: %s\n",strerror(errno));
      break;
    case 0:
      cf_archive_threads(to_archive,len);
      exit(0);
    default:
      cf_log(LOG_STD,__FILE__,__LINE__,"waiting for archive_threads\n");
      waitpid(pid,&status,0);
      cf_log(LOG_STD,__FILE__,__LINE__,"archive_threads finished!\n");
      break;
    }

    for(i=0;i<len;i++) {
      t_posting *p = NULL,*p1 = NULL;

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

  gdome_di_unref(impl,&e);
}
/* }}} */

/* {{{ cf_archive_thread */
void cf_archive_thread(int sockfd,u_int64_t tid) {
  t_thread *t = NULL,*prev = NULL,**list = NULL;
  t_posting *p,*p1;
  pid_t pid;
  int status;

  CF_RW_RD(&head.lock);
  t = head.thread;
  CF_RW_UN(&head.lock);

  if(t->tid != tid) {
    do {
      CF_RW_RD(&t->lock);

      prev = t;
      t    = t->next;

      CF_RW_UN(&prev->lock);
    } while(t && t->tid != tid);
  }

  if(!t) {
    writen(sockfd,"404 Thread Not Found\n",21);
    cf_log(LOG_ERR,__FILE__,__LINE__,"Thread not found\n");
    return;
  }
  else {
    list  = fo_alloc(NULL,1,sizeof(t_thread **),FO_ALLOC_MALLOC);
    *list = t;

    cf_unregister_thread(t);

    if(!prev) {
      CF_RW_WR(&head.lock);
      head.thread = head.thread->next;
      CF_RW_UN(&head.lock);
    }
    else {
      prev->next = t->next;
    }

    pid = fork();
    switch(pid) {
    case -1:
      cf_log(LOG_ERR,__FILE__,__LINE__,"fork: %s\n",strerror(errno));
      break;
    case 0:
      cf_archive_threads(list,1);
      exit(0);
    default:
      cf_log(LOG_STD,__FILE__,__LINE__,"waiting for archiver...\n");
      waitpid(pid,&status,0);
      cf_log(LOG_STD,__FILE__,__LINE__,"archiver finished!\n");
    }

    for(p=t->postings;p;p=p1) {
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

    cf_rwlock_destroy(&t->lock);
    free(t);
    free(list);

    writen(sockfd,"200 Ok\n",7);
  }
}
/* }}} */

/* eof */
