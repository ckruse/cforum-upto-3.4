/**
 * \file arrayutils.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 * \brief array utilities for the Classic Forum
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

/* {{{ array_init */
void array_init(t_array *ary,size_t element_size,void (*array_destroy)(void *)) {
  ary->reserved      = 0;
  ary->elements      = 0;
  ary->element_size  = element_size;
  ary->array         = NULL;
  ary->array_destroy = array_destroy;
}
/* }}} */

/* {{{ array_push */
void array_push(t_array *ary,const void *element) {
  if(ary->elements + 1 >= ary->reserved) {
    ary->array     = fo_alloc(ary->array,ary->element_size,ary->reserved+1,FO_ALLOC_REALLOC);
    ary->reserved += 1;
  }

  memcpy(ary->array + (ary->elements * ary->element_size),element,ary->element_size);
  ary->elements += 1;
}
/* }}} */

/* {{{ array_pop */
void *array_pop(t_array *ary) {
  ary->elements -= 1;
  return memdup((void *)(ary->array + ((ary->elements + 1) * ary->element_size)),ary->element_size);
}
/* }}} */

/* {{{ array_shift */
void *array_shift(t_array *ary) {
  void *elem = memdup(ary->array,ary->element_size);

  memmove(ary->array,ary->array+ary->element_size,(ary->elements - 1) * ary->element_size);
  ary->elements -= 1;
  return elem;
}
/* }}} */

/* {{{ array_unshift */
void array_unshift(t_array *ary,const void *element) {
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
void array_sort(t_array *ary,int(*compar)(const void *,const void *)) {
  qsort(ary->array,ary->elements,ary->element_size,compar);
}
/* }}} */

/* {{{ array_bsearch */
void *array_bsearch(t_array *ary,const void *key,int (*compar)(const void *, const void *)) {
  return bsearch(key,ary->array,ary->elements,ary->element_size,compar);
}
/* }}} */

/* {{{ array_element_at */
void *array_element_at(t_array *ary,size_t index) {
  if(index < 0 || index > ary->elements) {
    errno = EINVAL;
    return NULL;
  }

  return ary->array + (index * ary->element_size);
}
/* }}} */

/* {{{ array_destroy */
void array_destroy(t_array *ary) {
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
