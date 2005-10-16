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

/* {{{ cf_array_init */
void cf_array_init(cf_array_t *ary,size_t element_size,void (*cf_array_destroy)(void *)) {
  ary->reserved      = 0;
  ary->elements      = 0;
  ary->element_size  = element_size;
  ary->array         = NULL;
  ary->cf_array_destroy = cf_array_destroy;
}
/* }}} */

/* {{{ cf_array_push */
void cf_array_push(cf_array_t *ary,const void *element) {
  if(ary->elements + 1 >= ary->reserved) {
    ary->array     = cf_alloc(ary->array,ary->element_size,ary->reserved+1,CF_ALLOC_REALLOC);
    ary->reserved += 1;
  }

  memcpy(ary->array + (ary->elements * ary->element_size),element,ary->element_size);
  ary->elements += 1;
}
/* }}} */

/* {{{ cf_array_pop */
void *cf_array_pop(cf_array_t *ary) {
  ary->elements -= 1;
  return cf_memdup((void *)(ary->array + ((ary->elements) * ary->element_size)),ary->element_size);
}
/* }}} */

/* {{{ cf_array_shift */
void *cf_array_shift(cf_array_t *ary) {
  void *elem = cf_memdup(ary->array,ary->element_size);

  memmove(ary->array,ary->array+ary->element_size,(ary->elements - 1) * ary->element_size);
  ary->elements -= 1;
  return elem;
}
/* }}} */

/* {{{ cf_array_unshift */
void cf_array_unshift(cf_array_t *ary,const void *element) {
  if(ary->elements + 1 >= ary->reserved) {
    ary->array     = cf_alloc(ary->array,ary->element_size,ary->reserved+1,CF_ALLOC_REALLOC);
    ary->reserved += 1;
  }

  memmove(ary->array+ary->element_size,ary->array,ary->elements  * ary->element_size);
  memcpy(ary->array,element,ary->element_size);
  ary->elements += 1;
}
/* }}} */

/* {{{ cf_array_sort */
void cf_array_sort(cf_array_t *ary,int(*compar)(const void *,const void *)) {
  qsort(ary->array,ary->elements,ary->element_size,compar);
}
/* }}} */

/* {{{ cf_array_bsearch */
void *cf_array_bsearch(cf_array_t *ary,const void *key,int (*compar)(const void *, const void *)) {
  return bsearch(key,ary->array,ary->elements,ary->element_size,compar);
}
/* }}} */

/* {{{ cf_array_element_at */
void *cf_array_element_at(cf_array_t *ary,size_t index) {
  if(index < 0 || index >= ary->elements) {
    errno = EINVAL;
    return NULL;
  }

  return ary->array + (index * ary->element_size);
}
/* }}} */

/* {{{ cf_array_destroy */
void cf_array_destroy(cf_array_t *ary) {
  size_t i;

  if(ary->cf_array_destroy) {
    for(i=0;i<ary->elements;i++) {
      ary->cf_array_destroy(ary->array + (i * ary->element_size));
    }
  }

  free(ary->array);
  memset(ary,0,sizeof(*ary));
}
/* }}} */

/* eof */
