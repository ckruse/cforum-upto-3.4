/**
 * \file shm_locking.h
 * \author Christian Kruse, <ckruse@wwwtech.de>
 * \brief Implementation of a read-write lock
 *
 * This module implements a read-write-lock of a shared memory segment using SysV semaphores.
 * It consists of a semaphore set of three semaphores: one semaphore to lock the set, one semaphore
 * to count readers and one semaphore to show write locking. If the write lock has been set,
 * new readers will wait until the write lock has been released. So no writer could starve to
 * death.
 */

/* {{{ Initial headers */
/*
 * $LastChangedDate$
 * $LastChangedRevision$
 * $LastChangedBy$
 *
 */
/* }}} */

#ifndef __CF_SHM_LOCKING_H
#define __CF_SHM_LOCKING_H

#ifdef CF_SHARED_MEM

#define CF_LOCK_SEM   0 /**< The semaphore locking semaphore number */
#define CF_READER_SEM 1 /**< The reader locking semaphore number */
#define CF_WRITER_SEM 2 /**< The writer locking semaphore number */

/**
 * \brief Try to lock a shared memory segment exclusive
 * \param semid The semaphore set id
 * \return 0 on success, -1 on failure
 */
int shm_lock_exclusive(int semid);

/**
 * \brief Unlock an exclusive lock
 * \param semid The semaphore set id
 * \return 0 on success, -1 on failure
 */
int shm_unlock_exclusive(int semid);


/**
 * \brief Try to lock a shared memory segment shared.
 * \param semid The semaphore set id
 * \return 0 on success, -1 on failure
 */
int shm_lock_shared(int semid);

/**
 * \brief Try to unlock a shared lock.
 * \param semid The semaphore set id
 * \return 0 on success, -1 on failure
 */
int shm_unlock_shared(int semid);

#endif
#endif

/* eof */
