/**
 * \file semaphores.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 * \brief Wrappers around the sem* functions
 *
 * This module provides some wrapper functions for the semaphore
 * functions provided by POSIX. These functions make the handling
 * of semaphores much easier
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>

#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

#include <unistd.h>
#include <errno.h>

#include "semaphores.h"
/* }}} */

/*
 * Generally, this function does a semop()-operation to a specified semaphore in a set.
 * The SEM_UNDO flag will be set. Operation can either be greater 0, smaller 0 and equal 0. Greater
 * 0 means, increase the value of a semaphore by the specified value, < 0 means, decrease the semaphore
 * value by this specified value and equal 0 means, wait until the semaphore is 0.
 */
int cf_sem_pv(int id,int operation,int semnum) {
  struct sembuf semaphor;
  semaphor.sem_op   = operation;
  semaphor.sem_flg  = SEM_UNDO;
  semaphor.sem_num  = semnum;

  return semop(id,&semaphor,1);
}

int cf_sem_getval(int id,int semnum,int num,unsigned short *val) {
  unsigned short x[num];
  union semun n;

  memset(x,0,num * sizeof(unsigned short));
  memset(&n,0,sizeof(n));

  n.array = x;

  if(semctl(id,0,GETALL,n) == -1) return -1;

  *val = x[semnum];

  return 0;
}

/* eof */
