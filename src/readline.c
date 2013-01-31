/**
 * \file readline.c
 * \brief The readline implementation.
 *
 * This file contains a thread safe version of the readline algorithm.
 */

/* {{{ Initial headers */
/*
 * $LastChangedDate: 2005-03-04 16:28:28 +0100 (Fri, 04 Mar 2005) $
 * $LastChangedRevision: 698 $
 * $LastChangedBy: ckruse $
 *
 */
/* }}} */

/* {{{ Includes */
#include "config.h"
#include "defines.h"

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <errno.h>

#include <sys/types.h>
#include <unistd.h>

#include "readline.h"
/* }}} */

/*
 * Returns: static ssize_t   number of characters read
 * Parameters:
 *    - rline_t *tsd   the readline structure
 *    - int fd         the socket descriptor
 *    - u_char *ptr     a pointer to a character variable
 *
 * this function reads a character from a socket stream
 *
 */
static ssize_t my_read(rline_t *tsd,int fd,u_char *ptr) {
  int calls = 0;

  if(tsd->rl_cnt <= 0) {
    again:
    if((tsd->rl_cnt = read(fd,tsd->rl_buf,MAXLINE)) < 0) {
      if(errno == EINTR || errno == EAGAIN) {
        calls += 1;
        if(calls <= 3) {
          goto again;
        }
      }

      return -1;
    } else if(tsd->rl_cnt == 0) {
      return 0;
    }

    tsd->rl_bufptr = tsd->rl_buf;
  }

  tsd->rl_cnt--;
  *ptr = *tsd->rl_bufptr++;

  return 1;
}

/*
 * Returns: u_char *     a string read from the socket
 * Parameters:
 *    - int fd         the socket descriptor
 *    - rline_t *tsd   a thread structure
 *
 * this function reads a complete line from a socket
 *
 */
u_char *readline(int fd,rline_t *tsd) {
  int  n,rc,len = MAXLINE;
  u_char c = '\0',*ptr,*line = malloc(MAXLINE);

  if(!line) return NULL;

  tsd->rl_mem = MAXLINE;
  tsd->rl_len = 0;

  ptr = line;
  for(n=1;c != '\n';n++) {
    if((rc = my_read(tsd,fd,&c)) == 1) {

      if(n >= len) {
        len += MAXLINE;

        line = realloc(line,len);
        if(!line) return NULL;

        ptr = &line[n-1];

        tsd->rl_mem = len;
      }

      *ptr++ = c;
      tsd->rl_len++;

      if(c == '\n') break;
    }
    else if(rc == 0) {
      if(n == 1) { /* no data read */
        free(line);
        return NULL;
      }
      else break;
    }
    else {
      free(line);
      return NULL;
    }
  }

  *ptr = '\0';
  return line;
}

/*
 * Returns: ssize_t       number of characters written
 * Parameters:
 *    - int fd            the socket descriptor
 *    - const void *vptr  the data pointer
 *    - size_t n          the length of the data
 *
 * this function writes data to a socket. It makes sure,
 * that all data is sent.
 *
 */
ssize_t writen(int fd,const void *vptr,size_t n) {
  size_t  nleft;
  ssize_t nwritten;
  const u_char *ptr;

  ptr   = vptr;
  nleft = n;
  while(nleft > 0) {
    if((nwritten = write(fd,ptr,nleft)) <= 0) {
      if(errno == EINTR || errno == EAGAIN) {
        nwritten = 0;
      }
      else {
        return -1;
      }
    }

    nleft -= nwritten;
    ptr   += nwritten;
  }

  return n;
}

/* eof */
