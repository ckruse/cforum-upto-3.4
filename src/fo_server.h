/**
 * \file fo_server.h
 * \author Christian Kruse, <ckruse@wwwtech.de>
 * \brief fo_server function forwards and types
 */

/* {{{ Initial headers */
/*
 * $LastChangedDate$
 * $LastChangedRevision$
 * $LastChangedBy$
 *
 */
/* }}} */

#ifndef __FO_SERVER_H
#define __FO_SERVER_H

/**
 * Scheduling policy; either SCHED_RR, SCHED_FIFO or SCHED_OTHER
 */
#define SCHEDULING SCHED_OTHER

/**
 * this is the datatype for the postings
 */
typedef struct s_posting {
  u_int64_t mid; /**< The message id. */

  u_char *unid; /**< Unique id of the posting. Used to avoid double postings. */
  u_int32_t unid_len; /**< Length of the unique id. */

  u_char *subject; /**< The subject of the posting */
  u_int32_t subject_len; /**< The length of the subject. */

  u_char *category; /**< The category of the posting */
  u_int32_t category_len; /**< The length of the category. */

  u_char *content; /**< The content of the posting. */
  u_int32_t content_len; /**< The length of the content. */

  time_t date; /**< the date as time_t (it should be a long or an unsigned long) */

  u_int16_t level; /**< The level of the posting */
  u_int16_t invisible; /**< The visibility flag of the posting */

  u_int32_t votes_bad;
  u_int32_t votes_good;

  /** the name, email, homepage, image and ip as strings in an extra struct */
  struct {
    u_char *name; /**< The name of the poster */
    u_char *email; /**< The email address of the poster */
    u_char *hp; /**< The homepage URL of the poster */
    u_char *img; /**< The image URL of the poster */
    u_char *ip; /**< The IP address of the  poster */
    u_int32_t name_len; /**< The length of the name */
    u_int32_t email_len; /**< The length of the email address */
    u_int32_t hp_len; /**< The length of the homepage address */
    u_int32_t img_len; /**< The length of the image URL */
    u_int32_t ip_len; /**< The length of the IP address */
  } user;

  struct s_posting *next; /**< The next posting in the chain */
  struct s_posting *prev; /**< The previous posting in the chain */
} t_posting;


/** this is the datatype for the threads */
typedef struct s_thread {
  u_int64_t tid; /**< The thread id as an unsigned long long */

  /** the read-write-lock */
  t_cf_rwlock lock;

  t_posting *postings; /**< A pointer to the first posting in the chain */
  t_posting *newest; /**< A pointer to the newest posting in the chain */
  t_posting *oldest; /**< A pointer to the oldest posting in the chain */
  t_posting *last; /**< A pointer to the last posting in the chain */

  int32_t posts; /**< The number of postings this thread contains */

  struct s_thread *next; /**< The pointer to the next thread in the chain */
  struct s_thread *prev; /**< The pointer to the previous thread in the chain */
} t_thread;

typedef void (*t_worker)(int); /**< This is a worker function */

/** This struct is used to save client requests in the queque */
typedef struct s_client {
  int sock; /**< The connection descriptor to the client */
  t_worker worker; /**< A pointer to the worker function specified for this connection */
  struct s_client *next; /**< A pointer to the next element in the queque */
} t_client;

#define INITIAL_WORKERS_NUM 40 /**< The number of initial workers */
#define MAX_WORKERS_NUM     45 /**< The number of maximum workers */
#define CLIENT_PRIORITY_NUM 43 /**< The number of clients when to change the priority settings */
#define MAX_CLIENT_NUM      45 /**< The number of clients when to handle a request directly, not by the queque. */

/** This struct is used to manage a client queque */
typedef struct s_client_queque {
  t_cf_mutex lock; /**< The mutex to lock the queque */
  t_cf_mutex cond_lock; /**< The mutex to lock the conditional */
  pthread_cond_t  cond; /**< The conditional variable to wait for new connections */

  u_int32_t workers; /**< The number of worker threads started */
  u_int32_t clientnum; /**< The number of clients in the queque */
  int down; /**< Conditional, if set the workers have to shut down */

  t_client *clients; /**< A pointer to the first client in the chain */
  t_client *last; /**< A pointer to the last client in the chain */
} t_client_queque;

/** This struct is used to save a server socket. */
typedef struct s_server {
  int sock; /**< The server socket itself */
  u_int32_t size; /**< The size of the memory block where the struct sockaddr * pointer points to */
  t_worker worker; /**< The worker function which clients for this server socket should handle */
  struct sockaddr *addr; /**< The address structure for this socket */
  struct s_server *next; /**< A pointer to the next server socket in the chain */
} t_server;

/** This is the head of the thread chain and a global 'I contain all necessary information' variable */
typedef struct s_head {
  /** The read-write-lock */
  t_cf_rwlock lock;

  /** We wanna save the workers to pthread_join them at shutdown */
  pthread_t workers[MAX_WORKERS_NUM];

  /** Is the cache fresh? */
  int fresh;

  /** The cache including invisible messages and threads */
  t_string cache_visible;

  /** The cache excluding invisible messages and threads  */
  t_string cache_invisible;

  /** The date when the cache containing only the visible messages and threads has been created */
  time_t date_visible;

  /** The date when the cache containing all messages and threads has been created */
  time_t date_invisible;

  /** The pointer to the first thread */
  t_thread *thread,*last;

  /** The last tid */
  u_int64_t tid;

  /** The last mid */
  u_int64_t mid;

  /** True if the forum is locked */
  int locked;

  /** The logfile mutex to protect the logfile handles */
  pthread_mutex_t log_lock;

  /** Logfile handle for standard output */
  FILE *std;

  /** Logfile handle for error output */
  FILE *err;

  /**
   * Since the server cannot handle *very* high traffic, we use a
   * worker model and a queque
   */
   t_client_queque clients;

  /**
   * The mutex to synchronize access to the server sockets
   */
  t_cf_mutex server_lock;

  /** the Server sockets */
  t_server *servers;

  #ifdef CF_SHARED_MEM
  /** Shared memory access rights. RW for creator, R for all other */
  #define CF_SHARED_MODE (SHM_R|SHM_W|(SHM_R>>3)|(SHM_R>>6))

  int shm_ids[2]; /**< The shared memory id */
  int shm_sem; /**< The locking semaphore id */
  void *shm_ptrs[2]; /**< The pointer to the shared memory segment */
  t_cf_mutex shm_lock; /**< The mutex to synchronize access to this information */
  #endif

  /** This hash contains all the CFTP protocol handlers */
  t_cf_hash *protocol_handlers;

  /** This hash contains all threads */
  t_cf_hash *threads;
  t_cf_rwlock threads_lock; /**< lock for the threads hash */

  /** This hash contains all unique ids */
  t_cf_hash *unique_ids;
  t_cf_mutex unique_ids_mutex; /**< The mutex to synchronize access to unique_ids */

} t_head;

extern int    RUN; /**< Shall the server still run or shall we shutdown? */
extern t_head head; /**< The head variable. Contains all neccessary information about the server. */

/**
 * This function will be called when receiving a signal which says 'Hey, server, terminate!'
 * \param n The signal received
 */
void terminate(int n);

typedef int (*t_server_protocol_handler)(int,const u_char **,int,rline_t *); /**< Used for protocol plugins */
typedef int (*t_server_init_filter)(int); /**< Used for server initialization plugins */

#endif

/* eof */
