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
#include "cfconfig.h"
#include "defines.h"

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <ctype.h>

#include <sys/time.h>
#include <sys/types.h>

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


/* {{{ mutex functions */
/* {{{ cf_mutex_init */
void cf_mutex_init(const u_char *name,cf_mutex_t *mutex) {
  mutex->name = strdup(name);
  pthread_mutex_init(&mutex->mutex,NULL);
}
/* }}} */

/* {{{ cf_mutex_destroy */
void cf_mutex_destroy(cf_mutex_t *mutex) {
  free(mutex->name);
  pthread_mutex_destroy(&mutex->mutex);
}
/* }}} */

/* {{{ cf_lock_mutex */
void cf_lock_mutex(const u_char *file,const int line,cf_mutex_t *mutex) {
  #ifdef _FO_LOCK_DEBUG
  struct timeval tv1,tv2;
  struct timezone tz;

  unsigned long long diff;
  #endif

  int status;

  #ifdef _FO_LOCK_DEBUG
  if(gettimeofday(&tv1,&tz) == -1) return;

  cf_log(CF_ERR,file,line,"PTHREAD MUTEX TRY LOCK '%s'\n",mutex->name);
  #endif

  if((status = pthread_mutex_lock(&mutex->mutex)) != 0) {
    cf_log(CF_ERR,__FILE__,__LINE__,"%s:%d:pthread_mutex_lock(%s) %s\n",file,line,mutex->name,strerror(status));
    exit(-1);
  }

  #ifdef _FO_LOCK_DEBUG
  if(gettimeofday(&tv2,&tz) == -1) return;

  diff  = tv2.tv_sec * 1000 + tv2.tv_usec;
  diff -= tv1.tv_sec * 1000 + tv1.tv_usec;

  /* why does this happen? it shall not... */
  if(diff < 0) diff = 0;

  cf_log(CF_ERR,file,line,"PTHREAD MUTEX LOCKED '%s' %lld\n",mutex->name,diff);
  #endif
}
/* }}} */

/* {{{ cf_unlock_mutex */
void cf_unlock_mutex(const u_char *file,const int line,cf_mutex_t *mutex) {
  #ifdef _FO_LOCK_DEBUG
  struct timeval tv1,tv2;
  struct timezone tz;
  unsigned long long diff;
  #endif

  int status;

  #ifdef _FO_LOCK_DEBUG
  if(gettimeofday(&tv1,&tz) == -1) return;

  cf_log(CF_ERR,file,line,"PTHREAD MUTEX TRY UNLOCK '%s'\n",mutex->name);
  #endif

  if((status = pthread_mutex_unlock(&mutex->mutex)) != 0) {
    cf_log(CF_ERR,__FILE__,__LINE__,"%s:%d:pthread_mutex_unlock(%s): %s\n",file,line,mutex->name,strerror(status));
    exit(-1);
  }

  #ifdef _FO_LOCK_DEBUG
  if(gettimeofday(&tv2,&tz) == -1) return;

  diff  = tv2.tv_sec * 1000 + tv2.tv_usec;
  diff -= tv1.tv_sec * 1000 + tv1.tv_usec;

  /* why does this happen? it shall not... */
  if(diff < 0) diff = 0;

  cf_log(CF_ERR,file,line,"PTHREAD MUTEX UNLOCK '%s' %lld\n",mutex->name,diff);
  #endif
}
/* }}} */
/* }}} */


/* {{{ rwlock functions */
/* {{{ cf_rwlock_init */
void cf_rwlock_init(const u_char *name,cf_rwlock_t *rwlock) {
  rwlock->name = strdup(name);
  pthread_rwlock_init(&rwlock->rwlock,NULL);
}
/* }}} */

/* {{{ cf_rwlock_destroy */
void cf_rwlock_destroy(cf_rwlock_t *rwlock) {
  free(rwlock->name);
  pthread_rwlock_destroy(&rwlock->rwlock);
}
/* }}} */

/* {{{ cf_rwlock_rdlock */
void cf_rwlock_rdlock(const u_char *file,const int line,cf_rwlock_t *rwlock) {
  #ifdef _FO_LOCK_DEBUG
  struct timeval tv1,tv2;
  struct timezone tz;
  unsigned long long diff;
  #endif

  int status;

  #ifdef _FO_LOCK_DEBUG
  if(gettimeofday(&tv1,&tz) == -1) return;

  cf_log(CF_ERR,file,line,"PTHREAD RWLOCK TRY RDLOCK '%s'\n",rwlock->name);
  #endif

  if((status = pthread_rwlock_rdlock(&rwlock->rwlock)) != 0) {
    cf_log(CF_ERR,__FILE__,__LINE__,"%s:%d:pthread_rwlock_rdlock(%s): %s\n",file,line,rwlock->name,strerror(status));
    exit(-1);
  }

  #ifdef _FO_LOCK_DEBUG
  if(gettimeofday(&tv2,&tz) == -1) return;

  diff  = tv2.tv_sec * 1000 + tv2.tv_usec;
  diff -= tv1.tv_sec * 1000 + tv1.tv_usec;

  /* why does this happen? it shall not... */
  if(diff < 0) diff = 0;

  cf_log(CF_ERR,file,line,"PTHREAD RWLOCK RDLOCKED '%s' %lld\n",rwlock->name,diff);
  #endif
}
/* }}} */

/* {{{ cf_rwlock_wrlock */
void cf_rwlock_wrlock(const u_char *file,const int line,cf_rwlock_t *rwlock) {
  #ifdef _FO_LOCK_DEBUG
  struct timeval tv1,tv2;
  struct timezone tz;
  unsigned long long diff;
  #endif

  int status;

  #ifdef _FO_LOCK_DEBUG
  if(gettimeofday(&tv1,&tz) == -1) return;

  cf_log(CF_ERR,file,line,"PTHREAD RWLOCK TRY WRLOCK '%s'\n",rwlock->name);
  #endif

  if((status = pthread_rwlock_wrlock(&rwlock->rwlock)) != 0) {
    cf_log(CF_ERR,__FILE__,__LINE__,"%s:%d:pthread_rwlock_wrlock(%s): %s\n",file,line,rwlock->name,strerror(status));
    exit(-1);
  }

  #ifdef _FO_LOCK_DEBUG
  if(gettimeofday(&tv2,&tz) == -1) return;

  diff  = tv2.tv_sec * 1000 + tv2.tv_usec;
  diff -= tv1.tv_sec * 1000 + tv1.tv_usec;

  /* why does this happen? it shall not... */
  if(diff < 0) diff = 0;

  cf_log(CF_ERR,file,line,"PTHREAD RWLOCK WRLOCKED '%s' %lld\n",rwlock->name,diff);
  #endif
}
/* }}} */

/* {{{ cf_rwlock_unlock */
void cf_rwlock_unlock(const u_char *file,const int line,cf_rwlock_t *rwlock) {
  int status;

  if((status = pthread_rwlock_unlock(&rwlock->rwlock)) != 0) {
    cf_log(CF_ERR,__FILE__,__LINE__,"%s:%d:pthread_rwlock_unlock(%s): %s\n",file,line,rwlock->name,strerror(status));
    exit(-1);
  }

  #ifdef _FO_LOCK_DEBUG
  if(gettimeofday(&tv2,&tz) == -1) return;

  diff  = tv2.tv_sec * 1000 + tv2.tv_usec;
  diff -= tv1.tv_sec * 1000 + tv1.tv_usec;

  /* why does this happen? it shall not... */
  if(diff < 0) diff = 0;

  cf_log(CF_ERR,file,line,"PTHREAD RWLOCK UNLOCK '%s' %lld\n",rwlock->name,diff);
  #endif
}
/* }}} */
/* }}} */


/* {{{ cond functions */
/* {{{ cf_cond_init */
void cf_cond_init(const u_char *name,cf_cond_t *cond,const pthread_condattr_t *attr) {
  cond->name = strdup(name);
  pthread_mutex_init(&cond->lock,NULL);
  pthread_cond_init(&cond->cond,attr);
}
/* }}} */

/* {{{ cf_cond_destroy */
void cf_cond_destroy(cf_cond_t *cond) {
  free(cond->name);
  pthread_mutex_destroy(&cond->lock);
  pthread_cond_destroy(&cond->cond);
}
/* }}} */

/* {{{ cf_cond_signal */
void cf_cond_signal(const u_char *file,int line,cf_cond_t *cond) {
  #ifdef _FO_LOCK_DEBUG
  struct timeval tv1,tv2;
  struct timezone tz;
  unsigned long long diff;
  #endif

  int status;

  #ifdef _FO_LOCK_DEBUG
  if(gettimeofday(&tv1,&tz) == -1) return;

  cf_log(CF_ERR,file,line,"PTHREAD COND SIGNAL '%s'\n",cond->name);
  #endif

  if((status = pthread_mutex_lock(&cond->lock)) != 0) {
    cf_log(CF_ERR,__FILE__,__LINE__,"%s:%d:pthread_mutex_lock(%s): %s\n",file,line,cond->name,strerror(status));
    exit(-1);
  }

  if((status = pthread_cond_signal(&cond->cond)) != 0) {
    cf_log(CF_ERR,__FILE__,__LINE__,"%s:%d:pthread_cond_signal(%s): %s\n",file,line,cond->name,strerror(status));
    exit(-1);
  }

  pthread_mutex_unlock(&cond->lock);

  #ifdef _FO_LOCK_DEBUG
  if(gettimeofday(&tv2,&tz) == -1) return;

  diff  = tv2.tv_sec * 1000 + tv2.tv_usec;
  diff -= tv1.tv_sec * 1000 + tv1.tv_usec;

  /* why does this happen? it shall not... */
  if(diff < 0) diff = 0;

  cf_log(CF_ERR,file,line,"PTHREAD COND SIGNAL '%s' %lld\n",lock->name,diff);
  #endif
}
/* }}} */

/* {{{ cf_cond_broadcast */
void cf_cond_broadcast(const u_char *file,int line,cf_cond_t *cond) {
  #ifdef _FO_LOCK_DEBUG
  struct timeval tv1,tv2;
  struct timezone tz;
  unsigned long long diff;
  #endif

  int status;

  #ifdef _FO_LOCK_DEBUG
  if(gettimeofday(&tv1,&tz) == -1) return;

  cf_log(CF_ERR,file,line,"PTHREAD COND BROADCAST '%s'\n",cond->name);
  #endif

  if((status = pthread_mutex_lock(&cond->lock)) != 0) {
    cf_log(CF_ERR,__FILE__,__LINE__,"%s:%d:pthread_mutex_lock(%s): %s\n",file,line,cond->name,strerror(status));
    exit(-1);
  }

  if((status = pthread_cond_broadcast(&cond->cond)) != 0) {
    cf_log(CF_ERR,__FILE__,__LINE__,"%s:%d:pthread_cond_broadcast(%s): %s\n",file,line,cond->name,strerror(status));
    exit(-1);
  }

  pthread_mutex_unlock(&cond->lock);

  #ifdef _FO_LOCK_DEBUG
  if(gettimeofday(&tv2,&tz) == -1) return;

  diff  = tv2.tv_sec * 1000 + tv2.tv_usec;
  diff -= tv1.tv_sec * 1000 + tv1.tv_usec;

  /* why does this happen? it shall not... */
  if(diff < 0) diff = 0;

  cf_log(CF_ERR,file,line,"PTHREAD COND BROADCAST '%s' %lld\n",lock->name,diff);
  #endif
}
/* }}} */

/* {{{ cf_cond_wait */
void cf_cond_wait(const u_char *file,int line,cf_cond_t *cond) {
  #ifdef _FO_LOCK_DEBUG
  struct timeval tv1,tv2;
  struct timezone tz;
  unsigned long long diff;
  #endif

  int status;

  #ifdef _FO_LOCK_DEBUG
  if(gettimeofday(&tv1,&tz) == -1) return;

  cf_log(CF_ERR,file,line,"PTHREAD COND WAIT '%s'\n",cond->name);
  #endif

  if((status = pthread_mutex_lock(&cond->lock)) != 0) {
    cf_log(CF_ERR,__FILE__,__LINE__,"%s:%d:pthread_mutex_lock(%s): %s\n",file,line,cond->name,strerror(status));
    exit(-1);
  }

  if((status = pthread_cond_wait(&cond->cond,&cond->lock)) != 0) {
    cf_log(CF_ERR,__FILE__,__LINE__,"%s:%d:pthread_cond_wait(%s): %s\n",file,line,cond->name,strerror(status));
    exit(-1);
  }

  pthread_mutex_unlock(&cond->lock);

  #ifdef _FO_LOCK_DEBUG
  if(gettimeofday(&tv2,&tz) == -1) return;

  diff  = tv2.tv_sec * 1000 + tv2.tv_usec;
  diff -= tv1.tv_sec * 1000 + tv1.tv_usec;

  /* why does this happen? it shall not... */
  if(diff < 0) diff = 0;

  cf_log(CF_ERR,file,line,"PTHREAD COND WAIT '%s' %lld\n",lock->name,diff);
  #endif
}
/* }}} */

/* {{{ cf_cond_timedwait */
int cf_cond_timedwait(const u_char *file,int line,cf_cond_t *cond,const struct timespec *ts) {
  #ifdef _FO_LOCK_DEBUG
  struct timeval tv1,tv2;
  struct timezone tz;
  unsigned long long diff;
  #endif

  int status;

  #ifdef _FO_LOCK_DEBUG
  if(gettimeofday(&tv1,&tz) == -1) return;

  cf_log(CF_ERR,file,line,"PTHREAD COND WAIT '%s'\n",cond->name);
  #endif

  if((status = pthread_mutex_lock(&cond->lock)) != 0) {
    cf_log(CF_ERR,__FILE__,__LINE__,"%s:%d:pthread_mutex_lock(%s): %s\n",file,line,cond->name,strerror(status));
    exit(-1);
  }

  if((status = pthread_cond_timedwait(&cond->cond,&cond->lock,ts)) != 0) {
    if(status != ETIMEDOUT) {
      cf_log(CF_ERR,__FILE__,__LINE__,"%s:%d:pthread_cond_wait(%s): %s\n",file,line,cond->name,strerror(status));
      exit(-1);
    }
  }

  pthread_mutex_unlock(&cond->lock);

  #ifdef _FO_LOCK_DEBUG
  if(gettimeofday(&tv2,&tz) == -1) return;

  diff  = tv2.tv_sec * 1000 + tv2.tv_usec;
  diff -= tv1.tv_sec * 1000 + tv1.tv_usec;

  /* why does this happen? it shall not... */
  if(diff < 0) diff = 0;

  cf_log(CF_ERR,file,line,"PTHREAD COND TIMEDWAIT '%s' %lld\n",lock->name,diff);
  #endif

  return status;
}
/* }}} */

/* }}} */


/* eof */
