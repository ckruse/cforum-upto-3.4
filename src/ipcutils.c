/**
 * \file ipcutils.c
 * \author Christian Seiler, <self@christian-seiler.de>
 * \brief IPC utilities for the Classic Forum
 *
 * These utilities are written for the Classic Forum. Hope, they're useful.
 */

/* {{{ Initial headers */
/*
 * $LastChangedDate: 2005-02-01 12:15:36 +0100 (Tue, 01 Feb 2005) $
 * $LastChangedRevision: 527 $
 * $LastChangedBy: ckruse $
 *
 */
/* }}} */

/* {{{ Includes */
#include "config.h"
#include "defines.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/wait.h>

#include "utils.h"
/* }}} */

/* {{{ ipc_dpopen
 * Returns: int     -1 on failure, 0 on success
 * Parameters:
 *   - const char *filename        the filename to execute
 *   - char *const argv[]          the argv[] array of the program
 *   - char *const envp[]          the environment of the program
 *   - int *result                 a pointer to int[2] that will contain the
 *                                 two file descriptors: the first for reading,
 *                                 the second for writing
 *   - pid_t *res_pid              a pointer where the pid of the process should
 *                                 be stored (or NULL if the information is not
 *                                 wanted)
 *
 * this function opens two pipes, executes a program and returns the two file
 * descriptors for communicating with that program.
 *
 */
int ipc_dpopen(const char *filename,char *const argv[],char *const envp[],int *result,pid_t *res_pid) {
  /*
   * "input" here means that the programming calling this function gets its
   * input from that pipe
   */
  int input_pipe[2];
  int output_pipe[2];
  int res;
  int saved_errno;
  pid_t pid;
  
  res = pipe(input_pipe);
  if(res) {
    return -1;
  }
  res = pipe(output_pipe);
  if(res) {
    saved_errno = errno;
    close(input_pipe[0]);
    close(input_pipe[1]);
    errno = saved_errno;
    return -1;
  }
  if(!(pid = fork())) {
    /* child process */
    close(input_pipe[0]);
    close(output_pipe[1]);
    res = dup2(output_pipe[0],0);
    if(res == -1) {
      exit(1);
    }
    res = dup2(input_pipe[1],1);
    if(res == -1) {
      exit(1);
    }
    res = execve(filename,argv,envp);
    exit(1);
  } else if(pid < 0) {
    /* failure */
    saved_errno = errno;
    close(input_pipe[0]);
    close(input_pipe[1]);
    close(output_pipe[0]);
    close(output_pipe[1]);
    errno = saved_errno;
    return -1;
  }
  /* parent process */
  close(input_pipe[1]);
  close(output_pipe[0]);
  result[0] = input_pipe[0];
  result[1] = output_pipe[1];
  if(res_pid) {
    *res_pid = pid;
  }
  return 0;
}
/* }}} */

/* {{{ ipc_dpclose
 * Returns: int     -1 on failure, 0 on success
 * Parameters:
 *   - int *pipes                  a pointer to int[2] that contains the two
 *                                 pipes
 *   - pid_t *pid                  a pointer to the pid of the process or NULL
 *                                 (then, wait() will be used instead of
 *                                 waitpid())
 *
 * this function closes two pipes that were opened by ipc_dpopen() and then
 * waits for the process to terminate
 *
 */
int ipc_dpclose(int *pipes,pid_t *pid) {
  int res;
  if(pipes) {
    res = close(pipes[0]);
    if(res) {
      return res;
    }
    res = close(pipes[1]);
    if(res) {
      return res;
    }
  }
  if(pid) {
    return waitpid(*pid,NULL,0);
  } else {
    // just do a generic wait and ignore everything else...
    wait(NULL);
    return 0;
  }
  return 0;
}
/* }}} */

/* eof */
