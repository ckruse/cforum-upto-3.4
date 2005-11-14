/**
 * \file listutils.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 * \brief list utilities for the Classic Forum
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
#include <sys/types.h>

#include "charconvert.h"
#include "utils.h"
/* }}} */

/* {{{ cf_list_init */
void cf_list_init(cf_list_head_t *head) {
  memset(head,0,sizeof(*head));
}
/* }}} */

/* {{{ cf_list_append */
void cf_list_append(cf_list_head_t *head,void *data,size_t size) {
  cf_list_element_t *elem = fo_alloc(NULL,1,sizeof(*elem),FO_ALLOC_CALLOC);

  elem->data = memdup(data,size);
  elem->size = size;

  if(head->last == NULL) {
    head->last = head->elements = elem;
  }
  else {
    head->last->next = elem;
    elem->prev       = head->last;
    head->last       = elem;
  }
}
/* }}} */

/* {{{ cf_list_append_static */
void cf_list_append_static(cf_list_head_t *head,void *data,size_t size) {
  cf_list_element_t *elem = fo_alloc(NULL,1,sizeof(*elem),FO_ALLOC_CALLOC);

  elem->data = data;
  elem->size = size;
  elem->type = 1;

  if(head->last == NULL) {
    head->last = head->elements = elem;
  }
  else {
    head->last->next = elem;
    elem->prev       = head->last;
    head->last       = elem;
  }
}
/* }}} */

/* {{{ cf_list_prepend */
void cf_list_prepend(cf_list_head_t *head,void *data,size_t size) {
  cf_list_element_t *elem = fo_alloc(NULL,1,sizeof(*elem),FO_ALLOC_CALLOC);

  elem->data = memdup(data,size);
  elem->size = size;

  if(head->elements) {
    elem->next = head->elements;
    head->elements->prev = elem;
    head->elements = elem;
  }
  else {
    head->last = head->elements = elem;
  }
}
/* }}} */

/* {{{ cf_list_prepend_static */
void cf_list_prepend_static(cf_list_head_t *head,void *data,size_t size) {
  cf_list_element_t *elem = fo_alloc(NULL,1,sizeof(*elem),FO_ALLOC_CALLOC);

  elem->data = data;
  elem->size = size;
  elem->type = 1;

  if(head->elements) {
    elem->next = head->elements;
    head->elements->prev = elem;
    head->elements = elem;
  }
  else {
    head->last = head->elements = elem;
  }
}
/* }}} */

/* {{{ cf_list_insert */
void cf_list_insert(cf_list_head_t *head,cf_list_element_t *prev,void *data,size_t size) {
  cf_list_element_t *elem = fo_alloc(NULL,1,sizeof(*elem),FO_ALLOC_CALLOC);

  elem->data = memdup(data,size);
  elem->size = size;

  elem->next = prev->next;
  elem->prev = prev;
  if(prev->next) prev->next->prev = elem;
  prev->next = elem;
}
/* }}} */

/* {{{ cf_list_search */
void *cf_list_search(cf_list_head_t *head,void *data,int (*compare)(const void *data1,const void *data2)) {
  cf_list_element_t *elem;

  for(elem=head->elements;elem;elem=elem->next) {
    if(compare(elem->data,data) == 0) return elem->data;
  }

  return NULL;
}
/* }}} */

/* {{{ cf_list_delete */
void cf_list_delete(cf_list_head_t *head,cf_list_element_t *elem) {
  if(elem->prev) elem->prev->next = elem->next;
  if(elem->next) elem->next->prev = elem->prev;
  
  if(head->elements == elem) head->elements = elem->next;
  if(head->last == elem) head->last = elem->next;
}
/* }}} */

/* {{{ cf_list_destroy */
void cf_list_destroy(cf_list_head_t *head,void (*destroy)(void *data)) {
  cf_list_element_t *elem,*elem1;

  for(elem=head->elements;elem;elem=elem1) {
    elem1 = elem->next;

    if(destroy) destroy(elem->data);
    if(elem->type == 0) free(elem->data);
    free(elem);
  }
}
/* }}} */

/* eof */
