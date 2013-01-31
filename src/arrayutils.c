/**
 * \file arrayutils.c
 * \author Christian Kruse, <cjk@wwwtech.de>
 * \brief array utilities for the Classic Forum
 *
 * These utilities are written for the Classic Forum. Hope, they're useful.
 */

/* {{{ Initial headers */
/*
 * $LastChangedDate: 2009-01-16 14:47:41 +0100 (Fri, 16 Jan 2009) $
 * $LastChangedRevision: 1640 $
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
#include <time.h>
#include <ctype.h>
#include <errno.h>

#include <sys/time.h>
#include <sys/types.h>

#include "charconvert.h"
#include "utils.h"
/* }}} */

/* {{{ array_init */
void array_init(array_t *ary,size_t element_size,void (*array_destroy)(void *)) {
  ary->reserved      = 0;
  ary->elements      = 0;
  ary->element_size  = element_size;
  ary->array         = NULL;
  ary->array_destroy = array_destroy;
}
/* }}} */

/* {{{ array_push */
void array_push(array_t *ary,const void *element) {
  if(ary->elements + 1 >= ary->reserved) {
    ary->array     = fo_alloc(ary->array,ary->element_size,ary->reserved+1,FO_ALLOC_REALLOC);
    ary->reserved += 1;
  }

  memcpy(ary->array + (ary->elements * ary->element_size),element,ary->element_size);
  ary->elements += 1;
}
/* }}} */

/* {{{ array_pop */
void *array_pop(array_t *ary) {
  ary->elements -= 1;
  return memdup((void *)(ary->array + ((ary->elements) * ary->element_size)),ary->element_size);
}
/* }}} */

/* {{{ array_shift */
void *array_shift(array_t *ary) {
  void *elem = memdup(ary->array,ary->element_size);

  memmove(ary->array,ary->array+ary->element_size,(ary->elements - 1) * ary->element_size);
  ary->elements -= 1;
  return elem;
}
/* }}} */

/* {{{ array_unshift */
void array_unshift(array_t *ary,const void *element) {
  if(ary->elements + 1 >= ary->reserved) {
    ary->array     = fo_alloc(ary->array,ary->element_size,ary->reserved+1,FO_ALLOC_REALLOC);
    ary->reserved += 1;
  }

  memmove(ary->array+ary->element_size,ary->array,ary->elements  * ary->element_size);
  memcpy(ary->array,element,ary->element_size);
  ary->elements += 1;
}
/* }}} */

/* {{{ array_sort */
void array_sort(array_t *ary,int(*compar)(const void *,const void *)) {
  qsort(ary->array,ary->elements,ary->element_size,compar);
}
/* }}} */

/* {{{ array_bsearch */
void *array_bsearch(array_t *ary,const void *key,int (*compar)(const void *, const void *)) {
  return bsearch(key,ary->array,ary->elements,ary->element_size,compar);
}
/* }}} */

/* {{{ array_element_at */
void *array_element_at(array_t *ary,size_t index) {
  if(index >= ary->elements) {
    errno = EINVAL;
    return NULL;
  }

  return ary->array + (index * ary->element_size);
}
/* }}} */

/* {{{ array_destroy */
void array_destroy(array_t *ary) {
  size_t i;

  if(ary->array_destroy) {
    for(i=0;i<ary->elements;i++) {
      ary->array_destroy(ary->array + (i * ary->element_size));
    }
  }

  free(ary->array);
  memset(ary,0,sizeof(*ary));
}
/* }}} */

/* eof */
