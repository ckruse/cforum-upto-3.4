/**
 * \file memoryutils.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 * \brief memory utilities for the Classic Forum
 *
 * These utilities are written for the Classic Forum. Hope, they're useful.
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
#include "config.h"
#include "defines.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <errno.h>

#include <sys/time.h>

#include "charconvert.h"
#include "utils.h"
/* }}} */

/* {{{ cf_alloc
 * Returns:     NULL on false type attribute, the new pointer in any else case
 * Parameters:
 *   - void *ptr     The old pointer for realloc()
 *   - size_t nmemb  The number of objects to allocate
 *   - size_t size   The size of one object
 *   - int type      The type of the allocation (CF_ALLOC_MALLOC, CF_ALLOC_CALLOC or CF_ALLOC_REALLOC)
 *
 * Safely allocates new memory
 */
void *cf_alloc(void *ptr,size_t nmemb,size_t size,int type) {
  void *l_ptr = NULL;

  switch(type) {
    case CF_ALLOC_MALLOC:
      l_ptr = malloc(nmemb * size);
      break;
    case CF_ALLOC_CALLOC:
      l_ptr = calloc(nmemb,size);
      break;
    case CF_ALLOC_REALLOC:
      l_ptr = realloc(ptr,size * nmemb);
      break;
  }

  if(!l_ptr) {
    fprintf(stderr,"memory utils: error allocating memory: %s\n",strerror(errno));
    exit(EXIT_FAILURE);
  }

  return l_ptr;
}
/* }}} */

/* {{{ cf_memdup
 * Returns:  A pointer to the duplicated memory area
 * Parameters:
 *   - void *inptr  The pointer to the original memory area
 *   - size_t size  The size of the memory area
 *
 * This function duplicates a memory area
 *
 */
void *cf_memdup(void *inptr,size_t size) {
  void *outptr = cf_alloc(NULL,1,size,CF_ALLOC_MALLOC);
  memcpy(outptr,inptr,size);

  return outptr;
}
/* }}} */

/* {{{ cf_mem_init_growth */
void cf_mem_init_growth(cf_mem_pool_t *pool,unsigned growth) {
  pool->len      = 0;
  pool->reserved = 0;
  pool->growth   = growth;
  pool->content  = NULL;
}
/* }}} */

/* {{{ cf_mem_init */
void cf_mem_init(cf_mem_pool_t *pool) {
  pool->len      = 0;
  pool->reserved = 0;
  pool->growth   = CF_BUFSIZ;
  pool->content  = NULL;
}
/* }}} */

/* {{{ cf_mem_cleanup */
void cf_mem_cleanup(cf_mem_pool_t *pool) {
  pool->len      = 0;
  pool->reserved = 0;

  if(pool->content) free(pool->content);

  pool->content  = NULL;
}
/* }}} */

/* {{{ cf_mem_append */
void *cf_mem_append(cf_mem_pool_t *pool,const void *src,size_t length) {
  size_t len = pool->growth;

  if(pool->len + length >= pool->reserved) {
    if(length >= len) len += length;

    pool->reserved += len;
    pool->content   = cf_alloc(pool->content,(size_t)pool->reserved,1,CF_ALLOC_REALLOC);
  }

  memcpy(pool->content + pool->len,src,length);
  pool->len += length;

  return pool->content + pool->len - length;
}
/* }}} */

/* {{{ cf_mem_set */
size_t cf_mem_set(cf_mem_pool_t *pool,const void *src,size_t length) {
  size_t len = pool->growth;

  if(pool->len + length >= pool->reserved) {
    if(length >= len) len += length;

    pool->content   = cf_alloc(pool->content,pool->reserved + len,1,CF_ALLOC_REALLOC);
    pool->reserved += len;
  }

  memcpy(pool->content,src,length);
  pool->len = length;

  return length;
}
/* }}} */


/* eof */
