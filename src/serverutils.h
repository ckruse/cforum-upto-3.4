/**
 * \file serverutils.h
 * \author Christian Kruse, <ckruse@wwwtech.de>
 *
 * Utility function definitions for the server
 *
 */

/* {{{ Initial headers */
/*
 * $LastChangedDate$
 * $LastChangedRevision$
 * $LastChangedBy$
 *
 */
/* }}} */

#ifndef _CF_SERVERUTILS_H
#define _CF_SERVERUTILS_H

typedef struct s_cf_rwlocked_lishead_t {
  cf_rwlock_t lock;
  cf_list_head_t head;
} cf_rw_list_head_t;

void cf_rw_list_init(const u_char *name,cf_rw_list_head_t *head);
void cf_rw_list_append(cf_rw_list_head_t *head,void *data,size_t size);
void cf_rw_list_append_static(cf_rw_list_head_t *head,void *data,size_t size);
void cf_rw_list_prepend(cf_rw_list_head_t *head,void *data,size_t size);
void cf_rw_list_prepend_static(cf_rw_list_head_t *head,void *data,size_t size);
void cf_rw_list_insert(cf_rw_list_head_t *head,cf_list_element_t *prev,void *data,size_t size);
void *cf_rw_list_search(cf_rw_list_head_t *head,void *data,int (*compare)(const void *data1,const void *data2));
void cf_rw_list_delete(cf_rw_list_head_t *head,cf_list_element_t *elem);
void cf_rw_list_destroy(cf_rw_list_head_t *head,void (*destroy)(void *data));

#endif

/* eof */
