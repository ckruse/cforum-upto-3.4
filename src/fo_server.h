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


/* {{{ t_head */
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
/* }}} */

extern int    RUN; /**< Shall the server still run or shall we shutdown? */
extern t_head head; /**< The head variable. Contains all neccessary information about the server. */

typedef int (*t_server_init_filter)(int); /**< Used for server initialization plugins */

#endif

/* eof */
