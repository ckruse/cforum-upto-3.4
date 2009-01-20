/**
 * \file semaphores.h
 * \author Christian Kruse, <cjk@wwwtech.de>
 * \brief Wrappers around the sem* functions
 *
 * This module provides some wrapper functions for the semaphore
 * functions provided by POSIX. These functions make the handling
 * of semaphores much easier
 */

/* {{{ Initial headers */
/*
 * $LastChangedDate$
 * $LastChangedRevision$
 * $LastChangedBy$
 *
 */
/* }}} */

#ifndef _CF_SEMAPHORE_H
#define _CF_SEMAPHORE_H

#ifdef _SEM_SEMUN_UNDEFINED
union semun {
  int val;
  struct semid_ds *buf;
  unsigned short int *array;
  struct seminfo *__buf;
};
#endif

/**
 * This macro decreases the value of the semaphore by 1 (equivalent to the P() action)
 */
#define CF_SEM_LOCK(id,sem) (cf_sem_pv(id,-1,sem))

/**
 * This macro decreases the value of a semaphore by 1
 */
#define CF_SEM_DOWN(id,sem) (cf_sem_pv(id,-1,sem))

/**
 * This macro increases the value of the semaphore by 1 (equivalent to the V() action)
 */
#define CF_SEM_UNLOCK(id,sem) (cf_sem_pv(id,1,sem))

/**
 * This macro increases the value of the semaphore by 1
 */
#define CF_SEM_UP(id,sem)     (cf_sem_pv(id,1,sem))

/**
 * This macro blocks until the specified semaphore has reached a zero value
 */
#define CF_SEM_WAITZERO(id,sem) (cf_sem_pv(id,0,sem))

/**
 * \brief This function performs semaphore operations
 * \param id The semaphore id
 * \param operation The size to increase or decrease the value of the semaphore.
 * \param semnum The index of the semaphore in the set.
 * \return 0 on success, -1 on failure
 */
int cf_sem_pv(int id,int operation,int semnum);

/**
 * This function returns the value of the selected semaphore
 * \param id The semaphore ide
 * \param semnum The semaphore index in the set
 * \param num The number of semaphores in the set
 * \param val Reference to a integer variable where to store the value
 * \return 0 on success, -1 on failure
 */
int cf_sem_getval(int id,int semnum,int num,unsigned short *val);

#endif

/* eof */
