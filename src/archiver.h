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
 * Reads the <Date longSec="epochseconds"/> element and returns them.
 * \param n The \<Header\> element
 * \return 0 on error, the epoch secons on success
 */
time_t cf_get_time(GdomeNode *n);

/**
 * This function creates the path segments if necessary. Beginning from the start on every / it stops and calls
 * mkdir() with the string to the actual position
 * \param path The full path
 */
void cf_make_path(u_char *path);

/**
 * This function archives a list of threads to their right position in the archive.
 * \param to_archive The thread list of to archive threads
 * \param len The length of the list
 */
void cf_archive_threads(t_thread **to_archive,int len);

/**
 * This function runs the archiver and writes the actual threadlist to disk. It gets
 * exclusive access to the postings, so don't call this function *too* often!
 */
void cf_run_archiver_and_write_to_disk(void);

/**
 * This function archives a specific thread.
 * \param sockfd The client socket from which the request came
 * \param tid The thread id of the thread to archive
 */
void cf_archive_thread(int sockfd,u_int64_t tid);

/**
 * This function deletes an archived thread file
 * \param t The thread structure
 */
void cf_delete_threadfile(t_thread *t);

#endif
/* eof */
