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

  struct s_posting *next,*prev;
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

  u_int32_t posts; /**< The number of postings this thread contains */

  struct s_thread *next,*prev;
} t_thread;

typedef void (*t_worker)(int); /**< This is a worker function */

/** This struct is used to save client requests in the queque */
typedef struct s_client {
  int sock; /**< The connection descriptor to the client */
  t_worker worker; /**< A pointer to the worker function specified for this connection */
} t_cf_client;

/** This struct is used to save a server socket. */
typedef struct s_server {
  int sock; /**< The server socket itself */
  size_t size; /**< The size of the memory block where the struct sockaddr * pointer points to */
  t_worker worker; /**< The worker function which clients for this server socket should handle */
  struct sockaddr *addr; /**< The address structure for this socket */
} t_server;

/* {{{ t_forum */
typedef struct s_forum {
  /** forum locker */
  t_cf_rwlock lock;

  /** The name of the forum */
  u_char *name;

  /** Is the cache fresh? */
  int fresh;

  struct {
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
    t_thread *list;

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
} t_forum;
/* }}} */

/** This is the structure for the global 'I contain all necessary information' variable */
typedef struct s_head {
  /** The read-write-lock */
  t_cf_rwlock lock;

  /** We wanna save the workers to pthread_join them at shutdown */
  struct {
    t_cf_rw_list_head list;
    int num;
  } workers;

  struct {
    /** The logfile mutex to protect the logfile handles */
    pthread_mutex_t lock;

    /** Logfile handle for standard output */
    FILE *std;

    /** Logfile handle for error output */
    FILE *err;
  } log;

  /**
   * Since the server cannot handle *very* high traffic, we use a
   * worker model and a queque
   */
  struct {
    int num;
    t_cf_list_head list;
    t_cf_mutex lock;
    t_cf_cond cond;
  } clients;

  struct {
    /**
     * The mutex to synchronize access to the server sockets
     */
    t_cf_mutex lock;

    /** the Server sockets */
    t_cf_list_head list;
  } servers;

  /** This hash contains all the CFTP protocol handlers */
  t_cf_hash *protocol_handlers;

  /** This hash contains all forums */
  t_cf_hash *forums;

} t_head;

extern int    RUN; /**< Shall the server still run or shall we shutdown? */
extern t_head head; /**< The head variable. Contains all neccessary information about the server. */

typedef int (*t_server_protocol_handler)(int,const u_char **,int,rline_t *); /**< Used for protocol plugins */
typedef int (*t_server_init_filter)(int); /**< Used for server initialization plugins */

#endif

/* eof */
