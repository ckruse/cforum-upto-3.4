/**
 * \file shm_locking.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 * \brief Implementation of a read-write lock using semaphores.
 *
 */

/* {{{ Initial headers */
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

#ifdef CF_SHARED_MEM

#include <sys/types.h>

#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

#include <unistd.h>

#include "semaphores.h"
#include "shm_locking.h"
/* }}} */

/**
 * We want to lock the shared memory segment exclusively (e.g. for write access).
 * Therefore we have to set the exclusive-bit and we have to wait until the shared
 * counter is 0. The semaphore has to be locked during the exclusive operations, so
 * we *DON'T* unlock it in this routine!
 */
int shm_lock_exclusive(int semid) {
  /* decrease writer */
  if(CF_SEM_UNLOCK(semid,CF_WRITER_SEM) == -1) return -1;

  /* wait until reader number is 0 */
  if(CF_SEM_WAITZERO(semid,CF_READER_SEM) == -1) {
    CF_SEM_LOCK(semid,CF_WRITER_SEM);
    return -1;
  }

  /* lock semaphore set */
  if(CF_SEM_LOCK(semid,CF_LOCK_SEM) == -1) {
    CF_SEM_LOCK(semid,CF_WRITER_SEM);
    return -1;
  }

  return 0;
}

/**
 * We release an exclusive lock by setting the value of the writer-semaphore to 0.
 */
int shm_unlock_exclusive(int semid) {
  if(CF_SEM_UNLOCK(semid,0) == -1) return -1;

  return CF_SEM_LOCK(semid,2);
}

/**
 * We can lock a shared memory segment shared if the exclusive bit is not set
 * and the semaphore is not locked. The shared lock itself is just a incrementation
 * of the shared counter. The semaphore will be unlocked at the end of this routine,
 * but the shared counter has to be decremented, too.
 */
int shm_lock_shared(int semid) {
  if(CF_SEM_WAITZERO(semid,CF_WRITER_SEM) == -1) return -1;

  return CF_SEM_UNLOCK(semid,CF_READER_SEM);
}

/**
 * We release the shared lock by decrementing the shared counter.
 */
int shm_unlock_shared(int semid) {
  return CF_SEM_LOCK(semid,CF_READER_SEM);
}

#endif

/* eof */
