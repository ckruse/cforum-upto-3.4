/**
 * \file archiver.h
 * \author Christian Kruse, <ckruse@wwwtech.de>
 * \brief The archiver function declarations
 *
 * This file contains the archiver functions. The archiver is complex enough to
 * give him an own file.
 *
 */

/* {{{ Initial headers */
/*
 * $LastChangedDate$
 * $LastChangedRevision$
 * $LastChangedBy$
 *
 */
/* }}} */

#ifndef _ARCHIVER_H
#define _ARCHIVER_H

/**
 * Function type for archiver plugins.
 * \param thr The thread structure of the thread to archive
 * \return FLT_OK, FLT_DECLINE or FLT_EXIT. FLT_EXIT means, do *not* archive, so be careful!
 */
typedef int (*t_archive_filter)(t_thread *thr);

/**
 * This function archives a list of threads to their right position in the archive.
 * \param forum The forum struct
 * \param to_archive The thread list of to archive threads
 * \param len The length of the list
 */
void cf_archive_threads(t_forum *forum,t_thread **to_archive,int len);

/**
 * This function runs the archiver and writes the actual threadlist to disk. It gets
 * exclusive access to the postings and it's very expensive, so don't call this
 * function *too* often!
 */
void cf_run_archiver(void);

/**
 * This function archives a specific thread.
 * \param forum The forum struct
 * \param tid The thread id of the thread to archive
 */
int cf_archive_thread(t_forum *forum,u_int64_t tid);

#endif
/* eof */
