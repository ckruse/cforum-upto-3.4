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

#ifndef _CF_SERVER_H
#define _CF_SERVER_H

/**
 * Scheduling policy; either SCHED_RR, SCHED_FIFO or SCHED_OTHER
 */
#define SCHEDULING SCHED_OTHER


/* {{{ head_t */
/** This is the structure for the global 'I contain all necessary information' variable */
typedef struct s_head {
  /** The read-write-lock */
  cf_rwlock_t lock;

  /** We wanna save the workers to pthread_join them at shutdown */
  struct {
    cf_rw_list_head_t list;
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
    cf_list_head_t list;
    cf_mutex_t lock;
    cf_cond_t cond;
  } clients;

  struct {
    /**
     * The mutex to synchronize access to the server sockets
     */
    cf_mutex_t lock;

    /** the Server sockets */
    cf_list_head_t list;
  } servers;

  /** jobs to run periodical */
  cf_list_head_t periodicals;

  /** This hash contains all the CFTP protocol handlers */
  cf_hash_t *protocol_handlers;

  /** This hash contains all forums */
  cf_hash_t *forums;

} head_t;
/* }}} */

extern int    RUN; /**< Shall the server still run or shall we shutdown? */
extern head_t head; /**< The head variable. Contains all neccessary information about the server. */

typedef int (*cf_server_init_filter_t)(cf_cfg_config_t *,int); /**< Used for server initialization plugins */

#endif

/* eof */
