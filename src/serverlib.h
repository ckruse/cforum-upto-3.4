/**
 * \file serverlib.h
 * \author Christian Kruse, <ckruse@wwwtech.de>
 *
 * Data structure and function declarations for the server
 * library
 *
 */

#ifndef _CF_SERVERLIB_H
#define _CF_SERVERLIB_H

typedef struct s_posting_flag {
  u_char *name;
  u_char *val;
} posting_flag_t;

struct s_forum;
typedef struct s_forum forum_t;

typedef void (*worker_t)(int); /**< This is a worker function */
typedef int (*server_protocol_handler_t)(int,forum_t *,const u_char **,int,rline_t *); /**< Used for protocol plugins */

/* {{{ posting_t */
/**
 * this is the datatype for the postings
 */
typedef struct s_posting {
  u_int64_t mid; /**< The message id. */

  cf_string_t unid; /**< Unique id of the posting. Used to avoid double postings. */
  cf_string_t subject; /**< The subject of the posting */
  cf_string_t category; /**< The category of the posting */
  cf_string_t content; /**< The content of the posting. */

  time_t date; /**< the date as time_t (it should be a long or an unsigned long) */

  u_int16_t level; /**< The level of the posting */
  u_int16_t invisible; /**< The visibility flag of the posting */

  u_int32_t votes_bad;
  u_int32_t votes_good;

  /** the name, email, homepage, image and ip as strings in an extra struct */
  struct {
    cf_string_t name; /**< The name of the poster */
    cf_string_t email; /**< The email address of the poster */
    cf_string_t hp; /**< The homepage URL of the poster */
    cf_string_t img; /**< The image URL of the poster */
    cf_string_t ip; /**< The IP address of the  poster */
  } user;

  cf_list_head_t flags;

  struct s_posting *next,*prev;
} posting_t;
/* }}} */

/* {{{ thread_t */
/** this is the datatype for the threads */
typedef struct s_thread {
  u_int64_t tid; /**< The thread id as an unsigned long long */

  /** the read-write-lock */
  cf_rwlock_t lock;

  posting_t *postings; /**< A pointer to the first posting in the chain */

  posting_t *newest; /**< A pointer to the newest posting in the chain */
  posting_t *oldest; /**< A pointer to the oldest posting in the chain */
  posting_t *last; /**< A pointer to the last posting in the chain */

  u_int32_t posts; /**< The number of postings this thread contains */

  struct s_thread *next,*prev;
} thread_t;
/* }}} */

/* {{{ cf_client_t */
/** This struct is used to save client requests in the queque */
typedef struct s_client {
  int sock; /**< The connection descriptor to the client */
  worker_t worker; /**< A pointer to the worker function specified for this connection */
} cf_client_t;
/* }}} */

/* {{{ server_t */
/** This struct is used to save a server socket. */
typedef struct s_server {
  int sock; /**< The server socket itself */
  size_t size; /**< The size of the memory block where the struct sockaddr * pointer points to */
  worker_t worker; /**< The worker function which clients for this server socket should handle */
  struct sockaddr *addr; /**< The address structure for this socket */
} server_t;
/* }}} */

/* {{{ struct s_forum */
struct s_forum {
  /** forum locker */
  cf_rwlock_t lock;

  /** The name of the forum */
  u_char *name;

  struct {
    /** Is the cache fresh? */
    int fresh;

    /** The cache including invisible messages and threads */
    cf_string_t visible;

    /** The cache excluding invisible messages and threads  */
    cf_string_t invisible;
  } cache;

  struct {
    /** The date when the cache containing only the visible messages and threads has been created */
    time_t visible;

    /** The date when the cache containing all messages and threads has been created */
    time_t invisible;
  } date;

  /** True if the forum is locked */
  int locked;

  #ifdef CF_SHARED_MEM
  /** Shared memory access rights. RW for creator, R for all other */
  #define CF_SHARED_MODE (SHM_R|SHM_W|(SHM_R>>3)|(SHM_R>>6))

  struct {
    int ids[2]; /**< The shared memory id */
    int sem; /**< The locking semaphore id */
    void *ptrs[2]; /**< The pointer to the shared memory segment */
    cf_mutex_t lock; /**< The mutex to synchronize access to this information */
  } shm;
  #endif


  struct {
    /**< lock for the threads structure */
    cf_rwlock_t lock;

    /** This hash contains all threads */
    cf_hash_t *threads;

    /** The thread list */
    thread_t *list,*last;

    /** The last tid */
    u_int64_t last_tid;

    /** The last mid */
    u_int64_t last_mid;
  } threads;

  struct {
    /** This hash contains all unique ids */
    cf_hash_t *ids;
    cf_mutex_t lock; /**< The mutex to synchronize access to unique_ids */
  } uniques;
};
/* }}} */

typedef struct s_periodical {
  unsigned long periode;
  void (*worker)(void);
} periodical_t;

typedef int (*data_loading_filter_t)(forum_t *);
typedef int (*srv_new_post_filter_t)(forum_t *,u_int64_t,posting_t *);
typedef int (*srv_new_thread_filter_t)(forum_t *,thread_t *);

forum_t *cf_register_forum(const u_char *name);
int cf_load_data(forum_t *forum);

void cf_log(int mode,const u_char *file,unsigned int line,const u_char *format, ...);

int cf_setup_socket(struct sockaddr_un *addr);
void cf_push_server(int sockfd,struct sockaddr *addr,size_t size,worker_t handler);
int cf_push_client(int connfd,worker_t handler,int spare_threads,int max_threads,pthread_attr_t *attr);
void cf_register_thread(forum_t *forum,thread_t *t);
void cf_unregister_thread(forum_t *forum,thread_t *t);
int cf_register_protocol_handler(u_char *handler_hook,server_protocol_handler_t handler);

int cf_tokenize(u_char *line,u_char ***tokens);
void cf_cftp_handler(int sockfd);

int cf_shmdt(void *ptr);
void *cf_shmat(int shmid,void *addr,int shmflag);

void cf_generate_list(forum_t *forum,cf_string_t *str,int del);
void cf_generate_shared_memory(forum_t *forum);
void *cf_generate_cache(void *arg);

thread_t *cf_get_thread(forum_t *forum,u_int64_t tid);
posting_t *cf_get_posting(thread_t *t,u_int64_t mid);

void *cf_worker(void *arg);

void cf_destroy_flag(void *data);
void cf_remove_thread(forum_t *forum,thread_t *t);

void cf_destroy_forum(forum_t *forum);
void cf_cleanup_forumtree(forum_t *forum);
void cf_cleanup_thread(thread_t *t);
void cf_cleanup_posting(posting_t *p);

void *cf_periodical_worker(void *arg);
void cf_io_worker(void);

void cf_send_posting(forum_t *forum,int sock,u_int64_t tid,u_int64_t mid,int invisible);
int cf_read_posting(forum_t *forum,posting_t *p,int sock,rline_t *tsd);

int cf_remove_flags(int sockfd,rline_t *tsd,posting_t *p1);
int cf_read_flags(int sockfd,rline_t *tsd,posting_t *p);
posting_flag_t *cf_get_flag_by_name(cf_list_head_t *flags,const u_char *name);

#endif
/* eof */
