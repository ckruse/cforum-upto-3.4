/**
 * \file serverlib.h
 * \author Christian Kruse, <ckruse@wwwtech.de>
 *
 * Data structure and function declarations for the server
 * library
 *
 */

struct s_cf_rwlocked_list_head {
  t_cf_rwlock lock;
  t_cf_list_head head;
} t_cf_rw_list_head;

void cf_rw_list_init(const u_char *name,t_cf_rw_list_head *head);
void cf_rw_list_append(t_cf_rw_list_head *head,void *data,size_t size);
void cf_rw_list_append_static(t_cf_rw_list_head *head,void *data,size_t size);
void cf_rw_list_prepend(t_cf_rw_list_head *head,void *data,size_t size);
void cf_rw_list_prepend_static(t_cf_rw_list_head *head,void *data,size_t size);
void cf_list_insert(t_cf_rw_list_head *head,t_cf_list_element *prev,void *data,size_t size);
void *cf_rw_list_search(t_cf_rw_list_head *head,void *data,int (*compare)(const void *data1,const void *data2));
void cf_rw_list_delete(t_cf_rw_list_head *head,t_cf_list_element *elem);
void cf_rw_list_destroy(t_cf_rw_list_head *head,void (*destroy)(void *data));

t_forum *cf_register_forum(const u_char *name);
int cf_load_data(t_forum *forum);

void cf_log(int mode,const u_char *file,unsigned int line,const u_char *format, ...);

/* eof */
