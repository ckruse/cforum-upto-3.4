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

typedef int (*t_data_loading_filter)(t_forum *);

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

int cf_setup_socket(struct sockaddr_un *addr);
void cf_push_server(int sockfd,struct sockaddr *addr,size_t size,t_worker handler);
int cf_push_client(int connfd,t_worker handler,int spare_threads,int max_threads,pthread_attr_t *attr);
void cf_register_thread(t_forum *forum,t_thread *t);
void cf_unregister_thread(t_forum *forum,t_thread *t);
int cf_register_protocol_handler(u_char *handler_hook,t_server_protocol_handler handler);

int cf_tokenize(u_char *line,u_char ***tokens);
void cf_cftp_handler(int sockfd);

/* eof */
