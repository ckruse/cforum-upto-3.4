/**
 * \file cf_pthread.h
 * \author Christian Kruse, <ckruse@wwwtech.de>
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

/**
 * This struct is used to store a mutex and it's name
 */
typedef struct s_cf_mutex {
  u_char *name; /**< The name of the mutex */
  pthread_mutex_t mutex; /**< The mutex itself */
} t_cf_mutex;

/**
 * This struct is used to store a rwlock and it's name
 */
typedef struct s_cf_rwlock {
  u_char *name; /**< The name of the rwlock */
  pthread_rwlock_t rwlock; /**< The rwlock itself */
} t_cf_rwlock;

typedef struct s_cf_cond {
  u_char *name; /**< The name of the condition variable */
  pthread_mutex_t lock; /**< The locking mutex for this conditional */
	pthread_cond_t cond; /**< the conditional itself */
} t_cf_cond;

/**
 * Initialization function for the t_cf_mutex
 * \param name The name of the mutex
 * \param mutex The t_cf_mutex object
 */
void cf_mutex_init(const u_char *name,t_cf_mutex *mutex);

/**
 * The destructor for the t_cf_mutex class
 */
void cf_mutex_destroy(t_cf_mutex *mutex);

/**
 * This function initializes a rwlock
 * \param name The name of the read-write lock
 * \param rwlock The read-write lock object
 */
void cf_rwlock_init(const u_char *name,t_cf_rwlock *rwlock);

/**
 * This function is the destructor for the read-write lock class
 * \param rwlock The read-write lock object
 */
void cf_rwlock_destroy(t_cf_rwlock *rwlock);

/**
 * This function locks a mutex
 * \param file The file name
 * \param line The line number
 * \param mutex The mutex object
 */
void cf_lock_mutex(const u_char *file,const int line,t_cf_mutex *mutex);

/**
 * This function unlocks a mutex
 * \param file The file name
 * \param line The line number
 * \param mutex The mutex object
 */
void cf_unlock_mutex(const u_char *file,const int line,t_cf_mutex *mutex);

/**
 * This function locks a rwlock shared
 * \param file The file name
 * \param line The line number
 * \param rwlock The read-write lock
 */
void cf_rwlock_rdlock(const u_char *file,const int line,t_cf_rwlock *rwlock);

/**
 * This function locks a read-write lock writable
 * \param file The file name
 * \param line The line number
 * \param rwlock The read-write lock
 */
void cf_rwlock_wrlock(const u_char *file,const int line,t_cf_rwlock *rwlock);

/**
 * This function unlocks a read-write lock
 * \param file The file name
 * \param line The line number
 * \param rwlock The read-write lock
 */
void cf_rwlock_unlock(const u_char *file,const int line,t_cf_rwlock *rwlock);


void cf_cond_init(const u_char *name,t_cf_cond *cond,const pthread_condattr_t *attr);
void cf_cond_destroy(t_cf_cond *cond);
void cf_cond_signal(t_cf_cond *cond);
void cf_cond_broadcast(t_cf_cond *cond);
void cf_cond_wait(t_cf_cond *cond);
int cf_cond_timedwait(t_cf_cond *cond,const struct timespec *ts);


#endif

/* eof */
