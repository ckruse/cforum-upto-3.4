/**
 * \file serverlib.c
 * \author Christian Kruse
 *
 * Contains all server library functions
 */

/* {{{ rw list functions */

/* {{{ cf_rw_list_init */
void cf_rw_list_init(const u_char *name,t_cf_rw_list_head *head) {
  cf_rwlock_init(name,&head->lock);
  cf_list_init(&head->head);
}
/* }}} */

/* {{{ cf_rw_list_append */
void cf_rw_list_append(t_cf_rw_list_head *head,void *data,size_t size) {
  CF_RW_WR(&head->lock);
  cf_list_append(&head->head,data,size);
  CF_RW_UN(&head->lock);
}
/* }}} */

/* {{{ cf_rw_list_append_static */
void cf_rw_list_append_static(t_cf_rw_list_head *head,void *data,size_t size) {
  CF_RW_WR(&head->lock);
  cf_list_append_static(&head->head,data,size);
  CF_RW_UN(&head->lock);
}
/* }}} */

/* {{{ cf_rw_list_prepend */
void cf_rw_list_prepend(t_cf_rw_list_head *head,void *data,size_t size) {
  CF_RW_WR(&head->lock);
  cf_list_prepend(&head->head,data,size);
  CF_RW_UN(&head->lock);
}
/* }}} */

/* {{{ cf_rw_list_prepend_static */
void cf_rw_list_prepend_static(t_cf_rw_list_head *head,void *data,size_t size) {
  CF_RW_WR(&head->lock);
  cf_list_prepend_static(&head->head,data,size);
  CF_RW_UN(&head->lock);
}
/* }}} */

/* {{{ cf_list_insert */
void cf_list_insert(t_cf_rw_list_head *head,t_cf_list_element *prev,void *data,size_t size) {
  CF_RW_WR(&head->lock);
  cf_list_insert(&head->head,prev,data,size);
  CF_RW_UN(&head->lock);
}
/* }}} */

/* {{{ cf_rw_list_search */
void *cf_rw_list_search(t_cf_rw_list_head *head,void *data,int (*compare)(const void *data1,const void *data2)) {
  void *tmp;
  
  CF_RW_RD(&head->lock);
  tmp = cf_list_search(&head->head,data,compare);
  CF_RW_UN(&head->lock);

  return tmp;
}
/* }}} */

/* {{{ cf_rw_list_delete */
void cf_rw_list_delete(t_cf_rw_list_head *head,t_cf_list_element *elem) {
  CF_RW_WR(&head->lock);
  cf_list_delete(&head->head,elem);
  CF_RW_UN(&head->lock);
}
/* }}} */

/* {{{ cf_rw_list_destroy */
void cf_rw_list_destroy(t_cf_rw_list_head *head,void (*destroy)(void *data)) {
  CF_RW_WR(&head.lock);
  cf_list_destroy(&head->head,destroy);
  CF_RW_UN(&head.lock);

  cf_rwlock_destroy(&head->lock);
}
/* }}} */
/* }}} */


/* eof */
