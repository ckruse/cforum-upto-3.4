/**
 * \file fo_view.h
 * \author Christian Kruse, <ckruse@wwwtech.de>
 * \brief forum viewer program header file
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

#ifndef FO_VIEW_H
#define FO_VIEW_H


/* function forwards */

#ifndef CF_SHARED_MEM
/**
 * This function sends a posting to the user
 * \param head The CGI hash
 * \param sock The server socket
 * \param tid The thread id
 * \param mid The message id
 */
void send_posting(t_cf_hash *head,int sock,u_int64_t tid,u_int64_t mid);

/**
 * This function sends a thread list to the client
 * \param sock The server socket
 * \param head The CGI hash
 */
void send_threadlist(int sock,t_cf_hash *head);
#else
/**
 * This function sends a posting to the user
 * \param head The CGI hash
 * \param shm_ptr Pointer to the shared memory segment
 * \param tid The thread id
 * \param mid The message id
 */
void send_posting(t_cf_hash *head,void *shm_ptr,u_int64_t tid,u_int64_t mid);

/**
 * This function sends the threadlist to the client
 * \param shm_ptr Pointer to the shared memory segment
 * \param head The CGI hash
 */
void send_threadlist(void *shm_ptr,t_cf_hash *head);
#endif


/**
 * This function prints a thread structure
 * \param thread The thread structure
 * \param head The CGI hash
 */
void print_thread_structure(t_cl_thread *thread,t_cf_hash *head);


#endif

/* eof */
