/**
 * \file cf_pthread.h
 * \author Christian Kruse, <cjk@wwwtech.de>
 * \brief Pthread function and datatype wrappers
 *
 * This file contains some datatypes to debug pthread
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

#ifndef _CF_PTHREAD_H
#define _CF_PTHREAD_H

#define CF_LM(mutex) cf_lock_mutex(__FILE__,__LINE__,(mutex))   /**< Lock a mutex */
#define CF_UM(mutex) cf_unlock_mutex(__FILE__,__LINE__,(mutex)) /**< Unlock a mutex */

#define CF_RW_RD(rwlock) cf_rwlock_rdlock(__FILE__,__LINE__,(rwlock)) /**< Lock a rwlock shared */
#define CF_RW_WR(rwlock) cf_rwlock_wrlock(__FILE__,__LINE__,(rwlock)) /**< Lock a rwlock writable */
#define CF_RW_UN(rwlock) cf_rwlock_unlock(__FILE__,__LINE__,(rwlock)) /**< Unlock a rwlock */

#define CF_CD_SG(cnd) cf_cond_signal(__FILE__,__LINE__,(cnd))
#define CF_CD_BC(cnd) cf_cond_broadcast(__FILE__,__LINE__,(cnd))
#define CF_CD_WT(cnd) cf_cond_wait(__FILE__,__LINE__,(cnd))
#define CF_CD_TW(cnd,ts) cf_cond_timedwait(__FILE__,__LINE__,(cnd),(ts))

/**
 * This struct is used to store a mutex and it's name
 */
typedef struct s_cf_mutex {
  u_char *name; /**< The name of the mutex */
  pthread_mutex_t mutex; /**< The mutex itself */
} cf_mutex_t;

/**
 * This struct is used to store a rwlock and it's name
 */
typedef struct s_cf_rwlock {
  u_char *name; /**< The name of the rwlock */
  pthread_rwlock_t rwlock; /**< The rwlock itself */
} cf_rwlock_t;

typedef struct s_cf_cond {
  u_char *name; /**< The name of the condition variable */
  pthread_mutex_t lock; /**< The locking mutex for this conditional */
  pthread_cond_t cond; /**< the conditional itself */
} cf_cond_t;

/**
 * Initialization function for the cf_mutex_t
 * \param name The name of the mutex
 * \param mutex The cf_mutex_t object
 */
void cf_mutex_init(const u_char *name,cf_mutex_t *mutex);

/**
 * The destructor for the cf_mutex_t class
 */
void cf_mutex_destroy(cf_mutex_t *mutex);

/**
 * This function initializes a rwlock
 * \param name The name of the read-write lock
 * \param rwlock The read-write lock object
 */
void cf_rwlock_init(const u_char *name,cf_rwlock_t *rwlock);

/**
 * This function is the destructor for the read-write lock class
 * \param rwlock The read-write lock object
 */
void cf_rwlock_destroy(cf_rwlock_t *rwlock);

/**
 * This function locks a mutex
 * \param file The file name
 * \param line The line number
 * \param mutex The mutex object
 */
void cf_lock_mutex(const u_char *file,const int line,cf_mutex_t *mutex);

/**
 * This function unlocks a mutex
 * \param file The file name
 * \param line The line number
 * \param mutex The mutex object
 */
void cf_unlock_mutex(const u_char *file,const int line,cf_mutex_t *mutex);

/**
 * This function locks a rwlock shared
 * \param file The file name
 * \param line The line number
 * \param rwlock The read-write lock
 */
void cf_rwlock_rdlock(const u_char *file,const int line,cf_rwlock_t *rwlock);

/**
 * This function locks a read-write lock writable
 * \param file The file name
 * \param line The line number
 * \param rwlock The read-write lock
 */
void cf_rwlock_wrlock(const u_char *file,const int line,cf_rwlock_t *rwlock);

/**
 * This function unlocks a read-write lock
 * \param file The file name
 * \param line The line number
 * \param rwlock The read-write lock
 */
void cf_rwlock_unlock(const u_char *file,const int line,cf_rwlock_t *rwlock);


void cf_cond_init(const u_char *name,cf_cond_t *cond,const pthread_condattr_t *attr);
void cf_cond_destroy(cf_cond_t *cond);
void cf_cond_signal(const u_char *file,int line,cf_cond_t *cond);
void cf_cond_broadcast(const u_char *file,int line,cf_cond_t *cond);
void cf_cond_wait(const u_char *file,int line,cf_cond_t *cond);
int cf_cond_timedwait(const u_char *file,int line,cf_cond_t *cond,const struct timespec *ts);


#endif

/* eof */
