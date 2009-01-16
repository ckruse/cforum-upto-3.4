/**
 * \file clientlib.h
 * \author Christian Kruse, <ckruse@wwwtech.de>
 * \brief client library functions
 *
 * This file contains some functions and datatypes used in client modus,
 * e.g. delete_subtree() or generate_tpl_name() or something like that
 */

/* {{{ Initial headers */
/*
 * $LastChangedDate$
 * $LastChangedRevision$
 * $LastChangedBy$
 *
 */
/* }}} */

#ifndef _CF_CLIENTLIB_H
#define _CF_CLIENTLIB_H

#define CF_MODE_THREADLIST (1<<0)
#define CF_MODE_THREADVIEW (1<<1)
#define CF_MODE_PRE        (1<<2)
#define CF_MODE_POST       (1<<3)
#define CF_MODE_XML        (1<<4)

/**
 * module api function.
 * \param param Expects a void pointer
 * \return Returns a void pointer
 */
typedef void *(*mod_api_t)(void *);

/** Struct for Module communication API entry */
typedef struct s_mod_api_ent {
  u_char *mod_name;          /**< Name of the module */
  u_char *unique_identifier; /**< unique identifier of the API entry */
  mod_api_t function;        /**< function pointer to the API function */
} mod_api_ent_t;


typedef struct s_flag {
  u_char *name;
  u_char *val;
} cf_post_flag_t;

typedef struct s_uri_flag {
  u_char *name;
  u_char *val;
  int encode;
} cf_uri_flag_t;

/** This struct is used to store and handle a posting */
typedef struct s_message {
  u_int64_t mid; /**< The message id */

  cf_list_head_t flags; /**< contains the flags of the posting */

  string_t author; /**< The author of the posting */
  string_t email; /**< The email address of the author */
  string_t hp; /**< The homepage URL of the poster */
  string_t img; /**< The image URL of the poster */

  string_t remote_addr; /**< The remote address of the poster */

  string_t subject; /**< The subject of the posting */
  string_t category; /**< The category of the posting */
  string_t content; /**< The content of the posting */

  time_t date; /**< The date this message was created */
  unsigned short level; /**< The indent level of the posting */

  u_int32_t votes_good; /**< The good votings */
  u_int32_t votes_bad; /**< The bad votings */

  short may_show; /**< The visibility flag for plugins to use to hide postings */
  short invisible; /**< The visibility flag (if 0, posting is deleted) */

  cf_tpl_variable_t hashvar;

  struct s_message *next; /**< The pointer to the next message in the chain */
  struct s_message *prev; /**< The pointer to the previous message in the chain */
} message_t;

typedef struct s_hierchical_node {
  message_t *msg;
  array_t childs;
} hierarchical_node_t;

/** This struct is used to store and handle whole threads */
typedef struct s_cl_thread {
  u_int64_t tid; /**< The thread id */
  u_int32_t msg_len; /**< The number of messages */

  hierarchical_node_t *ht; /**< Thread in hierarchical datatypes */

  message_t *messages; /**< Pointer to the first message (thread message) in the chain */
  message_t *last; /**< Pointer to the last message in the chain */
  message_t *threadmsg; /**< Pointer to the message the user wants to see */
  message_t *newest; /**< Pointer to the newest message in this thread */
} cl_thread_t;


typedef struct {
  u_char *posting_uri[2];

  u_char *pre_threadlist_tpl;
  u_char *threadlist_thread_tpl;
  u_char *post_threadlist_tpl;

  u_char *thread_tpl;
  u_char *thread_posting_tpl;
} cf_readmode_t;


typedef int (*cf_readmode_collector_t)(cf_hash_t *,configuration_t *,configuration_t *,cf_readmode_t *);

#ifndef CF_SHARED_MEM
typedef int (*sorting_handler_t)(cf_hash_t *head,configuration_t *dc,configuration_t *vc,int sock,rline_t *tsd,array_t *threads);
#else
typedef int (*sorting_handler_t)(cf_hash_t *head,configuration_t *dc,configuration_t *vc,void *ptr,array_t *threads);
#endif

#ifdef CF_SHARED_MEM
typedef int (*thread_sorting_handler_t)(cf_hash_t *head,configuration_t *dc,configuration_t *vc,void *shm_ptr,cl_thread_t *thread);
#else
typedef int (*thread_sorting_handler_t)(cf_hash_t *head,configuration_t *dc,configuration_t *vc,int sock,rline_t *tsd,cl_thread_t *thread);
#endif

/**
 * This function prototype pointer is used for the authorization and the
 * initialization plugins
 * \param hash The CGI object hash
 * \param dc The default configuration object
 * \param vc The fo_view configuration object
 * \return FLT_OK, FLT_DECLINE or FLT_EXIT. FLT_EXIT terminates the program, FLT_OK terminates the plugin handling in this state.
 */
typedef int (*filter_begin_t)(cf_hash_t *,configuration_t *,configuration_t *);

/**
 * This function prototype pointer is used for the connection plugins
 * \param hash The CGI object hash
 * \param dc The default configuration object
 * \param vc The fo_view configuration object
 * \param sock The connection handle or, if defined CF_SHARED_MEM, the pointer to the shared memory segment
 * \return FLT_OK, FLT_DECLINE or FLT_EXIT. FLT_EXIT terminates the program
 */
#ifndef CF_SHARED_MEM
typedef int (*connect_filter_t)(cf_hash_t *,configuration_t *,configuration_t *,int);
#else
typedef int (*connect_filter_t)(cf_hash_t *,configuration_t *,configuration_t *,void *);
#endif

/**
 * This function prototype pointer is used for the VIEW_HANDLER plugins. It will be called once for
 * each thread.
 * \param hash The CGI object hash
 * \param dc The default configuration object
 * \param vc The fo_view configuration object
 * \param mode The mode argument
 * \return FLT_OK, FLT_DECLINE or FLT_EXIT. FLT_EXIT means, do not print this thread
 */
typedef int (*filter_list_t)(cf_hash_t *,configuration_t *,configuration_t *,cl_thread_t *,int);

/**
 * This function prototype pointer is used for the VIEW_LIST_HANDLER plugins. It will be called once for
 * each message.
 * \param hash The CGI object hash
 * \param dc The default configuration object
 * \param vc The fo_view configuration object
 * \param msg The message object
 * \param tid The thread id
 * \param mode The mode argument
 * \return FLT_OK, FLT_DECLINE or FLT_EXIT. FLT_EXIT means, do not print this message
 */
typedef int (*filter_list_posting_t)(cf_hash_t *,configuration_t *,configuration_t *,message_t *,u_int64_t,int);

/**
 * This function prototype pointer is used for the 404_HANDLER plugins. It will be called if a thread or
 * a message could not be found
 * \param hash The CGI object hash
 * \param dc The default configuration object
 * \param vc The fo_view configuration object
 * \param tid The thread id
 * \param mid The message id
 * \return FLT_OK, FLT_DECLINE or FLT_EXIT. FLT_EXIT means, do not print an error message
 */
typedef int (*filter_404_handler_t)(cf_hash_t *,configuration_t *,configuration_t *,u_int64_t,u_int64_t);

/**
 * This function prototype pointer is used for the POSTING_HANDLER plugins. It will be called when a user requests
 * a message of a thread.
 * \param hash The CGI object hash
 * \param dc The default configuration object
 * \param vc The fo_view configuration object
 * \param thread The thread object
 * \param template The template object
 * \return FLT_OK, FLT_DECLINE or FLT_EXIT. FLT_EXIT means, terminate the plugin handling
 */
typedef int (*filter_posting_t)(cf_hash_t *,configuration_t *,configuration_t *,cl_thread_t *,cf_template_t *);

/**
 * This function prototype pointer is used for the VIEW_INIT_HANDLER plugins. It will be called before the thread
 * list is generated.
 * \param hash The CGI object hash
 * \param dc The default configuration object
 * \param vc The fo_view configuration object
 * \param tpl_top The template object for the top of the page
 * \param tpl_end The template object for the end of the page
 * \return FLT_OK, FLT_DECLINE or FLT_EXIT, it doesn't matter in this case
 */
typedef int (*filter_init_view_t)(cf_hash_t *,configuration_t *,configuration_t *,cf_template_t *,cf_template_t *);

/**
 * This function prototype pointer is used for the NEW_POST_HANDLER plugins. It will be called before the
 * posting will be send to the server
 * \param hash The CGI object hash
 * \param dc The default configuration
 * \param pc The fo_post configuration
 * \param msg The new message
 * \param sock The server socket or the shm segment pointer
 * \param mode 0 on new thread, 1 on new answer
 * \return FLT_OK, FLT_DECLINE or FLT_EXIT. FLT_DECLINE and FLT_OK are functionless, FLT_EXIT means: do nothing else, just cleanup and exit
 */
#ifndef CF_SHARED_MEM
typedef int (*new_post_filter_t)(cf_hash_t *,configuration_t *,configuration_t *,message_t *,cl_thread_t *,int,int);
#else
typedef int (*new_post_filter_t)(cf_hash_t *,configuration_t *,configuration_t *,message_t *,cl_thread_t *,void *,int,int);
#endif

/**
 * This function prototype pointer is used for the AFTER_POST_HANDLER plugins. It will be called after a successful
 * transmission to the server
 * \param hash The CGI object hash
 * \param dc The default configuration
 * \param pc The fo_post configuration
 * \param msg The new message
 * \param tid The thread id
 * \return FLT_OK, FLT_DECLINE or FLT_EXIT. FLT_DECLINE and FLT_OK are functionless, FLT_EXIT means: run no more plugins
 */
#ifdef CF_SHARED_MEM
typedef int (*after_post_filter_t)(cf_hash_t *,configuration_t *,configuration_t *,message_t *,u_int64_t,int,void *);
#else
typedef int (*after_post_filter_t)(cf_hash_t *,configuration_t *,configuration_t *,message_t *,u_int64_t,int);
#endif

typedef int (*post_display_filter_t)(cf_hash_t *,configuration_t *,configuration_t *,cf_template_t *,message_t *);

typedef int (*filter_urlrewrite_t)(configuration_t *,configuration_t *,const u_char *,u_char **);

typedef int (*filter_perpost_var_t)(cf_hash_t *,configuration_t *,configuration_t *,cl_thread_t *,message_t *,cf_tpl_variable_t *);

/**
 * In this hash global values can be saved,
 * e.g. the username of logged in users
 */
extern cf_hash_t *GlobalValues;

/**
 * In this hash the module API entries will be saved
 */
extern cf_hash_t *APIEntries;


extern int shm_id;
extern void *shm_ptr;
extern int shm_lock_sem;


/**
 * contains error string in failure case
 */
extern u_char ErrorString[];

#ifdef CF_SHARED_MEM
/**
 * This function gets a pointer to the shared memory segment
 * \return Returns NULL if segment does not exist, otherwise it returns the pointer
 */
void *cf_get_shm_ptr(void);

/**
 * This function re-gets the pointer to the shared memory segment
 * \return Returns NULL if segment does not exist, otherwise it returns the pointer
 */
void *cf_reget_shm_ptr(void);
#endif


/**
 * This function tries to find the path to the user configuration file
 * \param uname The username
 * \return NULL If the configuration file of the user could not be found, the full path if it could be found
 */
u_char *cf_get_uconf_name(const u_char *uname);

/**
 * This function creates a socket handle an connects to the server
 * \return -1 on error, the socket on success
 */
int cf_socket_setup(void);



/**
 * This function generates a template name
 * \param buff The buffer in which the name should be saved
 * \param len The maximal length of the buffer
 * \param name The template name string
 */
void cf_gen_tpl_name(u_char *buff,size_t len,const u_char *name);

/**
 * This function sets a variable in a template.
 * The value will be converted to the output charset
 * \param tpl The template object
 * \param cs The charset configuration entry
 * \param vname The variable name
 * \param val The template variable value
 * \param len The length of the value
 * \param html 1 if output should be html escaped, 0 if not
 */
void cf_set_variable(cf_template_t *tpl,name_value_t *cs,u_char *vname,const u_char *val,size_t len,int html);

void cf_set_variable_hash(cf_tpl_variable_t *hash,name_value_t *cs,u_char *key,const u_char *val,size_t len,int html);


/**
 * This function spits out an error message by a error string
 * \param msg The error string
 * \param out A file handle to write to. If NULL, stdout will be used
 * \param rd The length of the error string
 */
void cf_error_message(const u_char *msg,FILE *out, ...);

/**
 * This function returns an error message
 * \param msg The error string
 * \param rd The length of the error string
 * \param len A reference to a size_t-variable. This variable will be set to the length of the error message.
 * \return NULL on error or a u_char pointer to the error message on success.
 * \attention You have to free() the returned u_char pointer!
 */
u_char *cf_get_error_message(const u_char *msg,size_t *len, ...);



/**
 * Returns first visible message in tree
 * \param msg The message list head
 * \return msg if no invisible message could be found, first visible message if a visible message could be found
 */
message_t *cf_msg_get_first_visible(message_t *msg);

hierarchical_node_t *cf_msg_ht_get_first_visible(hierarchical_node_t *msg);
void cf_msg_filter_invisible(hierarchical_node_t *msg,hierarchical_node_t *ary,int si);

/**
 * This function deletes a posting subtree
 * \param msg The message structure
 * \return Returns the next posting which has not been deleted
 */
message_t *cf_msg_delete_subtree(message_t *msg);

/**
 * This function search the first message in the next subtree
 * \param msg The message structure
 * \return Returns the pointer to the first message in the next subtree
 */
message_t *cf_msg_next_subtree(message_t *msg);

/**
 * This function search the first message in the previous subtree
 * \param msg The message structure
 * \return Returns the pointer to the first message in the previous subtree
 */
message_t *cf_msg_prev_subtree(message_t *msg);

/**
 * This function searches for the parent message of the given posting
 * \param tmsg The message
 * \return NULL if there is no parent posting, otherwise a pointer to the parent posting
 */
message_t *cf_msg_get_parent(message_t *tmsg);

/**
 * This function checks if a message has answers
 * \param msg The message structure
 * \returns
 */
int cf_msg_has_answers(message_t *msg);

/**
 * cleans up (== free()s needed memory) a message structure
 * \param msg The message to clean up
 */
void cf_cleanup_message(message_t *msg);

/**
 * cleans up (== free()s needed memory) a thread structure
 * \param thr The thread to clean up
 */
void cf_cleanup_thread(cl_thread_t *thr);

/**
 * Insert the postings to a hierarchy level. This function works recursively.
 * \param parent The parent hierarchical_node_t structure
 * \param msg The message pointer
 * \return The pointer to the next posting in a hierarchy level smaller or equal to ours.
 */
message_t *cf_msg_build_hierarchical_structure(hierarchical_node_t *parent,message_t *msg);

/**
 * This function serializes a hierarchical structure into a flat chain
 * \param thr The thread to linearize
 * \param h The hierarchical_node_t node
 * \return A posting in this or lower than this hierarchy level or NULL
 */
void cf_msg_linearize(cl_thread_t *thr,hierarchical_node_t *h);


/**
 * Run RM_COLLECTORS_HANDLER handlers.
 * \param head The CGI hash
 * \param rm_infos The readmode information struct
 * \return FLT_OK, FLT_EXIT or FLT_DECLINE
 */
int cf_run_readmode_collectors(cf_hash_t *head,configuration_t *vc,cf_readmode_t *rm_infos);

/**
 * Run VIEW_LIST_HANDLER handlers.
 * \param p The message pointer
 * \param head The CGI hash
 * \param tid The thread id
 * \param mode The mode which we run (0 == threadlist, 1 == thread view)
 * \return FLT_OK, FLT_EXIT or FLT_DECLINE
 */
int cf_run_view_list_handlers(message_t *p,cf_hash_t *head,u_int64_t tid,int mode);

/**
 * Runs VIEW_HANDLER handlers
 * \param thr The thread pointer
 * \param head The CGI hash
 * \param mode The mode which we run (0 == threadlist, 1 == thread view)
 * \return FLT_OK, FLT_EXIT or FLT_DECLINE
 */
int cf_run_view_handlers(cl_thread_t *thr,cf_hash_t *head,int mode);

/**
 * Runs POSTING_HANDLER handlers
 * \param head The CGI hash
 * \param thr The thread pointer
 * \param tpl The template pointer
 * \param vc The fo_view_config pointer
 * \return FLT_OK, FLT_EXIT or FLT_DECLINE
 */
int cf_run_posting_handlers(cf_hash_t *head,cl_thread_t *thr,cf_template_t *tpl,configuration_t *vc);

/**
 * Runs the 404 handlers
 * \param head The CGI hash
 * \param tid The Thread-ID
 * \param mid The Message-ID
 * \return FLT_OK, FLT_EXIT or FLT_DECLINE
 */
int cf_run_404_handlers(cf_hash_t *head,u_int64_t tid,u_int64_t mid);

/**
 * Runs the INIT_HANDLER handlers
 * \param head The CGI hash
 * \return FLT_OK, FLT_EXIT or FLT_DECLINE
 */
int cf_run_init_handlers(cf_hash_t *head);

/**
 * Runs the authentification handlers
 * \param head The CGI hash
 * \return FLT_OK, FLT_EXIT or FLT_DECLINE
 */
int cf_run_auth_handlers(cf_hash_t *head);

#ifdef CF_SHARED_MEM
/**
 * Runs the connection init handlers
 * \param head The CGI hash
 * \param sock The pointer to the shared memory segment
 * \return FLT_OK, FLT_EXIT or FLT_DECLINE
 */
int cf_run_connect_init_handlers(cf_hash_t *head,void *sock);
#else
/**
 * Runs the connection init handlers
 * \param head The CGI hash
 * \param sock The socket to the server
 * \return FLT_OK, FLT_EXIT or FLT_DECLINE
 */
int cf_run_connect_init_handlers(cf_hash_t *head,int sock);
#endif

/**
 * Runs the VIEW_INIT_HANDLER handlers
 * \param head The CGI hash
 * \param tpl_begin The forum header template
 * \param tpl_end The forum footer template
 * \return FLT_OK, FLT_DECLINE or FLT_EXIT
 */
int cf_run_view_init_handlers(cf_hash_t *head,cf_template_t *tpl_begin,cf_template_t *tpl_end);

#ifdef CF_SHARED_MEM
int cf_run_sorting_handlers(cf_hash_t *head,void *ptr,array_t *threads);
#else
int cf_run_sorting_handlers(cf_hash_t *head,int sock,rline_t *tsd,array_t *threads);
#endif

#ifdef CF_SHARED_MEM
int cf_run_thread_sorting_handlers(cf_hash_t *head,void *shm_ptr,cl_thread_t *thread);
#else
int cf_run_thread_sorting_handlers(cf_hash_t *head,int sock,rline_t *tsd,cl_thread_t *thread);
#endif

#ifdef CF_SHARED_MEM
void cf_run_after_post_handlers(cf_hash_t *head,message_t *p,u_int64_t tid,void *shm,int sock);
#else
void cf_run_after_post_handlers(cf_hash_t *head,message_t *p,u_int64_t tid,int sock);
#endif

#ifdef CF_SHARED_MEM
int cf_run_post_filters(cf_hash_t *head,message_t *p,cl_thread_t *thr,void *ptr,int sock);
#else
int cf_run_post_filters(cf_hash_t *head,message_t *p,cl_thread_t *thr,int sock);
#endif

/**
 * Runs the posting display plugins
 * \param head The CGI hash
 * \param tpl The template
 * \param p NULL or the posting
 */
int cf_run_post_display_handlers(cf_hash_t *head,cf_template_t *tpl,message_t *p);

int cf_run_perpost_var_handlers(cf_hash_t *head,cl_thread_t *thread,message_t *msg,cf_tpl_variable_t *hash);


void cf_add_static_uri_flag(const u_char *name,const u_char *value,int encode);
void cf_remove_static_uri_flag(const u_char *name);

/**
 * This function generates a link to a thread
 * \param link The link string (if NULL, PostingURL or UPostingURL will be used)
 * \param forum_name The name of the forum context. May be NULL if parameter link is given
 * \param tid The thread id
 * \param mid The message id
 * \return Returns NULL on failure or the link string on success
 * \attention You have to free() the returned pointer!
 */
u_char *cf_get_link(const u_char *link,u_int64_t tid,u_int64_t mid);

/**
 * This function generates a link to a thread and appends some parameters (as query string)
 * \param link The link string
 * \param tid The thread id
 * \param mid The message id
 * \param anchor The anchor part of the URI, may be NULL
 * \param plen The number of URI arguments to append. The number of arguments to the function must be plen * 2!
 * \param l A reference, the new length will be stored in it (if NULL, it'll be ignored)
 * \return Returns the link string
 * \attention You have to free() the returned pointer!
 */
u_char *cf_advanced_get_link(const u_char *link,u_int64_t tid,u_int64_t mid,u_char *anchor,size_t plen,size_t *l,...);

/**
 * This function creates a date string. It's more general than get_time(),
 * actually get_time() is a wrapper around it
 * \param fmt Date format
 * \param locale The date locale
 * \param len In this variable the length of the string will be stored
 * \param date The date itself
 * \return NULL on failure, the date string on success
 */
u_char *cf_general_get_time(u_char *fmt,u_char *locale,size_t *len,time_t *date);



/**
 * This function gets a message from the server
 * \param sock The socket
 * \param tsd The readline buffer
 * \param thr The thread structure
 * \param tid The thread id
 * \param mid The message id
 * \param del Boolean. If CF_KILL_DELETED, deleted messages will be killed. If CF_KEEP_DELETED, deleted messages will not be killed
 * \return 0 on success, -1 on failure
 */
int cf_get_message_through_sock(int sock,rline_t *tsd,cl_thread_t *thr,u_int64_t tid,u_int64_t mid,int del);

/**
 * This function reads the next thread from a socket
 * \param sock The socket
 * \param tsd The readline buffer
 * \param thr The thread structure
 */
int cf_get_next_thread_through_sock(int sock,rline_t *tsd,cl_thread_t *thr);

#ifndef CF_SHARED_MEM
/**
 * Reads a complete threadlist into an array
 *
 * \param ary Reference to an array object (will be initialized in this function!
 * \param sock The socket to the server
 * \param tsd The readline structure
 * \return -1 on failure, 0 on success
 */
int cf_get_threadlist(array_t *ary,int sock,rline_t *tsd);
#else
/**
 * Reads a complete threadlist into an array
 *
 * \param ary Reference to an array object (will be initialized in this function!
 * \param ptr Pointer to the shared memory
 * \return -1 on failure, 0 on success
 */
int cf_get_threadlist(array_t *ary,void *ptr);
#endif

#ifdef CF_SHARED_MEM
/**
 * This function gets a message from the server
 * \param shm_ptr The pointer to the shared memory segment
 * \param thr The thread structure
 * \param tid The thread id
 * \param mid The message id
 * \param del Boolean. If CF_KILL_DELETED, deleted messages will be killed. If CF_KEEP_DELETED, deleted messages will not be killed
 * \return 0 on success, -1 on failure
 */
int cf_get_message_through_shm(void *shm_ptr,cl_thread_t *thr,u_int64_t tid,u_int64_t mid,int del);

/**
 * This function reads the next thread from the shared memory segment
 * \param shm_ptr The pointer to the shared memory segment
 * \param thr The thread structure
 * \return NULL on failure, the modified pointer on success
 */
void *cf_get_next_thread_through_shm(void *shm_ptr,cl_thread_t *thr);
#endif


/**
 * This Function gets a posting flag by its name.
 * \param flags The flags list head
 * \param name The name of the flag
 * \return NULL or the flag if found.
 */
cf_post_flag_t *cf_flag_by_name(cf_list_head_t *flags,const u_char *name);


/**
 * This function registeres a module API entry
 * \param mod_name The name of the module which registeres
 * \param unique_identifier A unique string to identify this API hook
 * \param func A pointer to the API function
 * \return Returns 0 on success and -1 on failure (e.g. doubly entries)
 */
int cf_register_mod_api_ent(const u_char *mod_name,const u_char *unique_identifier,mod_api_t func);

/**
 * This function deletes a module API hook
 * \param unid The unique identifier of the API hook
 * \return 0 on success, -1 on failure (e.g. not found)
 */
int cf_unregister_mod_api_ent(const u_char *unid);

/**
 * This function returns a pointer to an API hook function
 * \param unid The unique id of the API hook
 * \return The pointer on success, NULL on failure
 */
mod_api_t cf_get_mod_api_ent(const u_char *unid);



/**
 * client library initialization
 */
void cf_init(void);

/**
 * client library cleanup
 */
void cf_fini(void);


#endif

/* eof */
