/**
 * \file serverutils.h
 * \author Christian Kruse, <ckruse@wwwtech.de>
 *
 * Utility function definitions for the server
 *
 */

/* {{{ Initial headers */
/*
 * $LastChangedDate: 2005-01-04 15:52:21 +0100 (Di, 04 Jan 2005) $
 * $LastChangedRevision$
 * $LastChangedBy: ckruse $
 *
 */
/* }}} */

#ifndef _CF_SERVERUTILS_H

typedef struct s_cf_rwlocked_list_head {
  t_cf_rwlock lock;
  t_cf_list_head head;
} t_cf_rw_list_head;

void cf_rw_list_init(const u_char *name,t_cf_rw_list_head *head);
void cf_rw_list_append(t_cf_rw_list_head *head,void *data,size_t size);
void cf_rw_list_append_static(t_cf_rw_list_head *head,void *data,size_t size);
void cf_rw_list_prepend(t_cf_rw_list_head *head,void *data,size_t size);
void cf_rw_list_prepend_static(t_cf_rw_list_head *head,void *data,size_t size);
void cf_rw_list_insert(t_cf_rw_list_head *head,t_cf_list_element *prev,void *data,size_t size);
void *cf_rw_list_search(t_cf_rw_list_head *head,void *data,int (*compare)(const void *data1,const void *data2));
void cf_rw_list_delete(t_cf_rw_list_head *head,t_cf_list_element *elem);
void cf_rw_list_destroy(t_cf_rw_list_head *head,void (*destroy)(void *data));

#define _CF_SERVERUTILS_H
#endif
