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

#include "charconvert.h"
#include "utils.h"
/* }}} */

/* {{{ cf_list_init */
void cf_list_init(t_cf_list_head *head) {
  memset(head,0,sizeof(*head));
}
/* }}} */

/* {{{ cf_list_append */
void cf_list_append(t_cf_list_head *head,void *data,size_t size) {
  t_cf_list_element *elem = fo_alloc(NULL,1,sizeof(*elem),FO_ALLOC_CALLOC);

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
void cf_list_append_static(t_cf_list_head *head,void *data,size_t size) {
  t_cf_list_element *elem = fo_alloc(NULL,1,sizeof(*elem),FO_ALLOC_CALLOC);

  elem->data = data;
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

/* {{{ cf_list_prepend */
void cf_list_prepend(t_cf_list_head *head,void *data,size_t size) {
  t_cf_list_element *elem = fo_alloc(NULL,1,sizeof(*elem),FO_ALLOC_CALLOC);

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

/* {{{ cf_list_insert */
void cf_list_insert(t_cf_list_head *head,t_cf_list_element *prev,void *data,size_t size) {
  t_cf_list_element *elem = fo_alloc(NULL,1,sizeof(*elem),FO_ALLOC_CALLOC);

  elem->data = memdup(data,size);
  elem->size = size;

  elem->next = prev->next;
  elem->prev = prev;
  if(prev->next) prev->next->prev = elem;
  prev->next = elem;
}
/* }}} */

/* {{{ cf_list_search */
void *cf_list_search(t_cf_list_head *head,void *data,int (*compare)(const void *data1,const void *data2)) {
  t_cf_list_element *elem;

  for(elem=head->elements;elem;elem=elem->next) {
    if(compare(elem->data,data) == 0) return elem->data;
  }

  return NULL;
}
/* }}} */

/* {{{ cf_list_delete */
void cf_list_delete(t_cf_list_head *head,t_cf_list_element *elem) {
  if(elem->prev) elem->prev->next = elem->next;
  if(elem->next) elem->next->prev = elem->prev;
  
  if(head->elements == elem) head->elements = elem->next;
  if(head->last == elem) head->elements = elem->next;
}
/* }}} */

/* {{{ cf_list_destroy */
void cf_list_destroy(t_cf_list_head *head,void (*destroy)(void *data)) {
  t_cf_list_element *elem,*elem1;

  for(elem=head->elements;elem;elem=elem1) {
    elem1 = elem->next;

    if(destroy) destroy(elem);
    free(elem->data);
    free(elem);
  }
}
/* }}} */

/* eof */
