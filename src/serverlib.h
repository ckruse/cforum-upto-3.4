/**
 * \file serverlib.h
 * \author Christian Kruse, <ckruse@wwwtech.de>
 * \brief Server library functions and datatypes
 *
 * This file contains the functions used by the server
 * program and the server plugins, so it contains the
 * server library.
 */

/* {{{ Initial comments */
/*
 * $LastChangedDate$
 * $LastChangedRevision$
 * $LastChangedBy$
 *
 */
/* }}} */

#ifndef __SERVERLIB_H
#define __SERVERLIB_H

#ifndef DOXYGEN
/* forward type to avoid a warning */
struct sockaddr_un;
#endif

/**
 * Function to create socket and bind it to the specified
 * port
 * \param addr A unix domain socket structure
 * \returns The socket on success or -1 on failure
 */
int cf_set_us_up_the_socket(struct sockaddr_un *addr);

/**
 * Worker function for the CFTP
 * \param arg The socket as an integer variable castet to (void *)
 * \return Returns NULL
 */
void *cf_worker(void *arg);

/**
 * Handler function to handle client sockets. This function
 * calls the specified worker function
 * \param sockfd The socket file descriptor of the connection
 */
void cf_handle_request(int sockfd);


/**
 * This function sends a "live" thread list
 * \param sockfd The socket file descriptor
 * \param del True-false-switch. If false value, deleted threads are not submitted. If true value, deleted threads will be submitted
 */
void cf_send_thread_list(int sockfd,int del);

/**
 * This function generates the cache for the thread list
 * \param arg Dummy argument
 * \return Returns always NULL
 */
void *cf_generate_cache(void *arg);

/**
 * This function generates a stringified thread list for
 * caching
 * \param str A t_string reference. Destination for the stringified list
 * \param del If true, deleted threads will be inserted, too. If false, they won't.
 */
void cf_generate_list(t_string *str,int del);

/**
 * This function sends a posting to the client
 * \param sock The socket file descriptor
 * \param tid The thread id
 * \param mid The message id
 * \param invisible If true, invisible messages will be sent, to. If false, the won't.
 */
void cf_send_posting(int sock,u_int64_t tid,u_int64_t mid,int invisible);

/**
 * This function gets a thread from the thread list
 * \param tid The thread list
 * \returns If the thread could be found, the thread, otherwise NULL.
 */
t_thread *cf_get_thread(u_int64_t tid);

/**
 * This function returns a posting from a thread
 * \param t Thread structure
 * \param mid The message id
 * \returns The posting if it could be found, otherwise NULL
 */
t_posting *cf_get_posting(t_thread *t,u_int64_t mid);

/**
 * This function generates the refreshed shared memory
 * segment
 */
void cf_generate_shared_memory();

/**
 * This function reads a posting in CFTP from the socket into
 * the referenced posting structure.
 * \param p The referenced posting structure
 * \param sock The client socket
 * \param tsd The caching structure
 * \returns 0 on success, -1 on failure
 */
int cf_read_posting(t_posting *p,int sock,rline_t *tsd);

/**
 * This function makes a log entry into the log file
 * \param mode The mode to log (LOG_ERR, LOG_DBG or LOG_STD)
 * \param file The file name (will be shortened to the last piece of the file name, e.g. /home/ck.c to ck.c)
 * \param format The format string
 * \param line The line number
 */
void cf_log(int mode,const u_char *file,int line,const u_char *format,...);

/**
 * This function adds a client to the client queque.
 * \param connfd The connection file descriptor
 * \param handler The handler function
 * \returns -1 on failure, 0 on success
 */
int cf_push_client(int connfd,t_worker handler);

/**
 * This function adds a server socket to the server sockets array
 * \param sockfd The socket file descriptor
 * \param addr The address structure
 * \param size The size of the structure
 * \param handler Pointer to the handler function
 * \return Returns 0 on success, -1 on failure
 */
int cf_push_server(int sockfd,struct sockaddr *addr,int size,t_worker handler);

/**
 * This function registeres a CFTP handler
 * \param handler_hook The token which identifies the handler
 * \param handler The handler function
 * \returns 0 on success, -1 on failure
 */
int cf_register_protocol_handler(u_char *handler_hook,t_server_protocol_handler handler);

/**
 * This function registeres a thread in the global threads hash
 * \param t The thread structure
 */
void cf_register_thread(t_thread *t);

/**
 * This function unregisteres a thread int he global threads hash
 * \param t The thread structure
 */
void cf_unregister_thread(t_thread *t);

/**
 * This function tokenizes a CFTP line
 * \param line The line string
 * \param tokens A reference to a u_char ** pointer. Contains the array of tokens after function call
 * \return Returns 0 on failure, number of tokens on success
 */
int cf_tokenize(u_char *line,u_char ***tokens);

#endif

/* eof */
