/**
 * \file serverlib.h
 * \author Christian Kruse, <ckruse@wwwtech.de>
 *
 * Data structure and function declarations for the server
 * library
 *
 */

typedef struct s_posting_flag {
  u_char *name;
  u_char *val;
} t_posting_flag;

struct s_forum;
typedef struct s_forum t_forum;

typedef void (*t_worker)(int); /**< This is a worker function */
typedef int (*t_server_protocol_handler)(int,t_forum *,const u_char **,int,rline_t *); /**< Used for protocol plugins */

/* {{{ t_posting */
/**
 * this is the datatype for the postings
 */
typedef struct s_posting {
  u_int64_t mid; /**< The message id. */

  t_string unid; /**< Unique id of the posting. Used to avoid double postings. */
  t_string subject; /**< The subject of the posting */
  t_string category; /**< The category of the posting */
  t_string content; /**< The content of the posting. */

  time_t date; /**< the date as time_t (it should be a long or an unsigned long) */

  u_int16_t level; /**< The level of the posting */
  u_int16_t invisible; /**< The visibility flag of the posting */

  u_int32_t votes_bad;
  u_int32_t votes_good;

  /** the name, email, homepage, image and ip as strings in an extra struct */
  struct {
    t_string name; /**< The name of the poster */
    t_string email; /**< The email address of the poster */
    t_string hp; /**< The homepage URL of the poster */
    t_string img; /**< The image URL of the poster */
    t_string ip; /**< The IP address of the  poster */
  } user;

  t_cf_list_head flags;

  struct s_posting *next,*prev;
} t_posting;
/* }}} */

/* {{{ t_thread */
/** this is the datatype for the threads */
typedef struct s_thread {
  u_int64_t tid; /**< The thread id as an unsigned long long */

  /** the read-write-lock */
  t_cf_rwlock lock;

  t_posting *postings; /**< A pointer to the first posting in the chain */

  t_posting *newest; /**< A pointer to the newest posting in the chain */
  t_posting *oldest; /**< A pointer to the oldest posting in the chain */
  t_posting *last; /**< A pointer to the last posting in the chain */

  u_int32_t posts; /**< The number of postings this thread contains */

  struct s_thread *next,*prev;
} t_thread;
/* }}} */

/* {{{ t_cf_client */
/** This struct is used to save client requests in the queque */
typedef struct s_client {
  int sock; /**< The connection descriptor to the client */
  t_worker worker; /**< A pointer to the worker function specified for this connection */
} t_cf_client;
/* }}} */

/* {{{ t_server */
/** This struct is used to save a server socket. */
typedef struct s_server {
  int sock; /**< The server socket itself */
  size_t size; /**< The size of the memory block where the struct sockaddr * pointer points to */
  t_worker worker; /**< The worker function which clients for this server socket should handle */
  struct sockaddr *addr; /**< The address structure for this socket */
} t_server;
/* }}} */

/* {{{ struct s_forum */
struct s_forum {
  /** forum locker */
  t_cf_rwlock lock;

  /** The name of the forum */
  u_char *name;

  struct {
    /** Is the cache fresh? */
    int fresh;

    /** The cache including invisible messages and threads */
    t_string visible;

    /** The cache excluding invisible messages and threads  */
    t_string invisible;
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
    t_cf_mutex lock; /**< The mutex to synchronize access to this information */
  } shm;
  #endif


  struct {
    /**< lock for the threads structure */
    t_cf_rwlock lock;

    /** This hash contains all threads */
    t_cf_hash *threads;

    /** The thread list */
    t_thread *list,*last;

    /** The last tid */
    u_int64_t last_tid;

    /** The last mid */
    u_int64_t last_mid;
  } threads;

  struct {
    /** This hash contains all unique ids */
    t_cf_hash *ids;
    t_cf_mutex lock; /**< The mutex to synchronize access to unique_ids */
  } uniques;
};
/* }}} */

typedef int (*t_data_loading_filter)(t_forum *);
typedef int (*t_srv_new_post_filter)(t_forum *,u_int64_t,t_posting *);
typedef int (*t_srv_new_thread_filter)(t_forum *,t_thread *);

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

int cf_shmdt(void *ptr);
void *cf_shmat(int shmid,void *addr,int shmflag);

void cf_generate_list(t_forum *forum,t_string *str,int del);
void cf_generate_shared_memory(t_forum *forum);
void *cf_generate_cache(void *arg);

t_thread *cf_get_thread(t_forum *forum,u_int64_t tid);
t_posting *cf_get_posting(t_thread *t,u_int64_t mid);

void *cf_worker(void *arg);

void cf_destroy_flag(void *data);
void cf_remove_thread(t_forum *forum,t_thread *t);

void cf_destroy_forum(t_forum *forum);
void cf_cleanup_forumtree(t_forum *forum);
void cf_cleanup_thread(t_thread *t);
void cf_cleanup_posting(t_posting *p);

void *cf_io_worker(void *arg);

void cf_send_posting(t_forum *forum,int sock,u_int64_t tid,u_int64_t mid,int invisible);
int cf_read_posting(t_forum *forum,t_posting *p,int sock,rline_t *tsd);

/* eof */
