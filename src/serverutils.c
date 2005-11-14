/**
 * \file serverutils.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 *
 * Implementation of the server utility functions
 */

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
#include <pthread.h>
#include <sys/types.h>

#include "cf_pthread.h"

#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "readline.h"

#include "serverutils.h"
/* }}} */

/* {{{ Initial comments */
/*
 * $LastChangedDate$
 * $LastChangedRevision$
 * $LastChangedBy$
 *
 */
/* }}} */

/* {{{ cf_rw_list_init */
void cf_rw_list_init(const u_char *name,cf_rw_list_head_t *head) {
  cf_rwlock_init(name,&head->lock);
  cf_list_init(&head->head);
}
/* }}} */

/* {{{ cf_rw_list_append */
void cf_rw_list_append(cf_rw_list_head_t *head,void *data,size_t size) {
  CF_RW_WR(&head->lock);
  cf_list_append(&head->head,data,size);
  CF_RW_UN(&head->lock);
}
/* }}} */

/* {{{ cf_rw_list_append_static */
void cf_rw_list_append_static(cf_rw_list_head_t *head,void *data,size_t size) {
  CF_RW_WR(&head->lock);
  cf_list_append_static(&head->head,data,size);
  CF_RW_UN(&head->lock);
}
/* }}} */

/* {{{ cf_rw_list_prepend */
void cf_rw_list_prepend(cf_rw_list_head_t *head,void *data,size_t size) {
  CF_RW_WR(&head->lock);
  cf_list_prepend(&head->head,data,size);
  CF_RW_UN(&head->lock);
}
/* }}} */

/* {{{ cf_rw_list_prepend_static */
void cf_rw_list_prepend_static(cf_rw_list_head_t *head,void *data,size_t size) {
  CF_RW_WR(&head->lock);
  cf_list_prepend_static(&head->head,data,size);
  CF_RW_UN(&head->lock);
}
/* }}} */

/* {{{ cf_rw_list_insert */
void cf_rw_list_insert(cf_rw_list_head_t *head,cf_list_element_t *prev,void *data,size_t size) {
  CF_RW_WR(&head->lock);
  cf_list_insert(&head->head,prev,data,size);
  CF_RW_UN(&head->lock);
}
/* }}} */

/* {{{ cf_rw_list_search */
void *cf_rw_list_search(cf_rw_list_head_t *head,void *data,int (*compare)(const void *data1,const void *data2)) {
  void *tmp;

  CF_RW_RD(&head->lock);
  tmp = cf_list_search(&head->head,data,compare);
  CF_RW_UN(&head->lock);

  return tmp;
}
/* }}} */

/* {{{ cf_rw_list_delete */
void cf_rw_list_delete(cf_rw_list_head_t *head,cf_list_element_t *elem) {
  CF_RW_WR(&head->lock);
  cf_list_delete(&head->head,elem);
  CF_RW_UN(&head->lock);
}
/* }}} */

/* {{{ cf_rw_list_destroy */
void cf_rw_list_destroy(cf_rw_list_head_t *head,void (*destroy)(void *data)) {
  CF_RW_WR(&head->lock);
  cf_list_destroy(&head->head,destroy);
  CF_RW_UN(&head->lock);

  cf_rwlock_destroy(&head->lock);
}
/* }}} */

/* eof */
