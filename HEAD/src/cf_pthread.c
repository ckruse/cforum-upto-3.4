/**
 * \file cf_pthread.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 * \brief Pthread function and datatype wrappers
 *
 * This file contains some functions to debug pthread
 * locking functions
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

#include <sys/time.h>

#include "cf_pthread.h"

#include "hashlib.h"
#include "readline.h"
#include "utils.h"
#include "fo_server.h"
#include "serverlib.h"
/* }}} */

void cf_mutex_init(const u_char *name,t_cf_mutex *mutex) {
  mutex->name = strdup(name);
  pthread_mutex_init(&mutex->mutex,NULL);
}
void cf_mutex_destroy(t_cf_mutex *mutex) {
  free(mutex->name);
  pthread_mutex_destroy(&mutex->mutex);
}

void cf_rwlock_init(const u_char *name,t_cf_rwlock *rwlock) {
  rwlock->name = strdup(name);
  pthread_rwlock_init(&rwlock->rwlock,NULL);
}
void cf_rwlock_destroy(t_cf_rwlock *rwlock) {
  free(rwlock->name);
  pthread_rwlock_destroy(&rwlock->rwlock);
}

void cf_lock_mutex(const u_char *file,const int line,t_cf_mutex *mutex) {
  #ifdef _FO_LOCK_DEBUG
  struct timeval tv1,tv2;
  struct timezone tz;

  unsigned long long diff;
  #endif

  int status;

  #ifdef _FO_LOCK_DEBUG
  if(gettimeofday(&tv1,&tz) == -1) return;

  cf_log(LOG_ERR,file,line,"PTHREAD MUTEX TRY LOCK '%s'\n",mutex->name);
  #endif

  if((status = pthread_mutex_lock(&mutex->mutex)) != 0) {
    cf_log(LOG_ERR,__FILE__,__LINE__,"pthread_mutex_lock: %s\n",strerror(status));
    exit(-1);
  }

  #ifdef _FO_LOCK_DEBUG
  if(gettimeofday(&tv2,&tz) == -1) return;

  diff  = tv2.tv_sec * 1000 + tv2.tv_usec;
  diff -= tv1.tv_sec * 1000 + tv1.tv_usec;

  /* why does this happen? it shall not... */
  if(diff < 0) diff = 0;

  cf_log(LOG_ERR,file,line,"PTHREAD MUTEX LOCKED '%s' %lld\n",mutex->name,diff);
  #endif
}

void cf_unlock_mutex(const u_char *file,const int line,t_cf_mutex *mutex) {
  #ifdef _FO_LOCK_DEBUG
  struct timeval tv1,tv2;
  struct timezone tz;
  unsigned long long diff;
  #endif

  int status;

  #ifdef _FO_LOCK_DEBUG
  if(gettimeofday(&tv1,&tz) == -1) return;

  cf_log(LOG_ERR,file,line,"PTHREAD MUTEX TRY UNLOCK '%s'\n",mutex->name);
  #endif

  if((status = pthread_mutex_unlock(&mutex->mutex)) != 0) {
    cf_log(LOG_ERR,__FILE__,__LINE__,"pthread_mutex_unlock: %s\n",strerror(status));
    exit(-1);
  }

  #ifdef _FO_LOCK_DEBUG
  if(gettimeofday(&tv2,&tz) == -1) return;

  diff  = tv2.tv_sec * 1000 + tv2.tv_usec;
  diff -= tv1.tv_sec * 1000 + tv1.tv_usec;

  /* why does this happen? it shall not... */
  if(diff < 0) diff = 0;

  cf_log(LOG_ERR,file,line,"PTHREAD MUTEX UNLOCK '%s' %lld\n",mutex->name,diff);
  #endif
}

void cf_rwlock_rdlock(const u_char *file,const int line,t_cf_rwlock *rwlock) {
  #ifdef _FO_LOCK_DEBUG
  struct timeval tv1,tv2;
  struct timezone tz;
  unsigned long long diff;
  #endif

  int status;

  #ifdef _FO_LOCK_DEBUG
  if(gettimeofday(&tv1,&tz) == -1) return;

  cf_log(LOG_ERR,file,line,"PTHREAD RWLOCK TRY RDLOCK '%s'\n",rwlock->name);
  #endif

  if((status = pthread_rwlock_rdlock(&rwlock->rwlock)) != 0) {
    cf_log(LOG_ERR,__FILE__,__LINE__,"pthread_rwlock_rdlock: %s\n",strerror(status));
    exit(-1);
  }

  #ifdef _FO_LOCK_DEBUG
  if(gettimeofday(&tv2,&tz) == -1) return;

  diff  = tv2.tv_sec * 1000 + tv2.tv_usec;
  diff -= tv1.tv_sec * 1000 + tv1.tv_usec;

  /* why does this happen? it shall not... */
  if(diff < 0) diff = 0;

  cf_log(LOG_ERR,file,line,"PTHREAD RWLOCK RDLOCKED '%s' %lld\n",rwlock->name,diff);
  #endif
}

void cf_rwlock_wrlock(const u_char *file,const int line,t_cf_rwlock *rwlock) {
  #ifdef _FO_LOCK_DEBUG
  struct timeval tv1,tv2;
  struct timezone tz;
  unsigned long long diff;
  #endif

  int status;

  #ifdef _FO_LOCK_DEBUG
  if(gettimeofday(&tv1,&tz) == -1) return;

  cf_log(LOG_ERR,file,line,"PTHREAD RWLOCK TRY WRLOCK '%s'\n",rwlock->name);
  #endif

  if((status = pthread_rwlock_wrlock(&rwlock->rwlock)) != 0) {
    cf_log(LOG_ERR,__FILE__,__LINE__,"pthread_rwlock_wrlock: %s\n",strerror(status));
    exit(-1);
  }

  #ifdef _FO_LOCK_DEBUG
  if(gettimeofday(&tv2,&tz) == -1) return;

  diff  = tv2.tv_sec * 1000 + tv2.tv_usec;
  diff -= tv1.tv_sec * 1000 + tv1.tv_usec;

  /* why does this happen? it shall not... */
  if(diff < 0) diff = 0;

  cf_log(LOG_ERR,file,line,"PTHREAD RWLOCK WRLOCKED '%s' %lld\n",rwlock->name,diff);
  #endif
}

void cf_rwlock_unlock(const u_char *file,const int line,t_cf_rwlock *rwlock) {
  #ifdef _FO_LOCK_DEBUG
  struct timeval tv1,tv2;
  struct timezone tz;
  unsigned long long diff;
  #endif

  int status;

  #ifdef _FO_LOCK_DEBUG
  if(gettimeofday(&tv1,&tz) == -1) return;

  cf_log(LOG_ERR,file,line,"PTHREAD RWLOCK TRY UNLOCK '%s'\n",rwlock->name);
  #endif

  if((status = pthread_rwlock_unlock(&rwlock->rwlock)) != 0) {
    cf_log(LOG_ERR,__FILE__,__LINE__,"pthread_rwlock_unlock: %s\n",strerror(status));
    exit(-1);
  }

  #ifdef _FO_LOCK_DEBUG
  if(gettimeofday(&tv2,&tz) == -1) return;

  diff  = tv2.tv_sec * 1000 + tv2.tv_usec;
  diff -= tv1.tv_sec * 1000 + tv1.tv_usec;

  /* why does this happen? it shall not... */
  if(diff < 0) diff = 0;

  cf_log(LOG_ERR,file,line,"PTHREAD RWLOCK UNLOCK '%s' %lld\n",rwlock->name,diff);
  #endif
}

/* eof */
