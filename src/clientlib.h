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

#ifndef CF_CLIENTLIB_H
#define CF_CLIENTLIB_H

/**
 * module api function.
 * \param param Expects a void pointer
 * \return Returns a void pointer
 */
typedef void *(*t_mod_api)(void *);

/** Struct for Module communication API entry */
typedef struct s_mod_api_ent {
  u_char *mod_name;          /**< Name of the module */
  u_char *unique_identifier; /**< unique identifier of the API entry */
  t_mod_api function;        /**< function pointer to the API function */
} t_mod_api_ent;


/** This struct is used to store and handle a posting */
typedef struct s_message {
  u_int64_t mid; /**< The message id */
  u_char *author; /**< The author of the posting */
  unsigned long author_len; /**< The length of the author string */

  u_char *subject; /**< The subject of the posting */
  unsigned long subject_len; /**< The length of the subject string */

  u_char *category; /**< The category of the posting */
  unsigned long category_len; /**< The length of the category string */

  u_char *content; /**< The content of the posting */
  unsigned long content_len; /**< The length of the content string */

  u_char *email; /**< The email-address of the poster */
  unsigned long email_len; /**< The length of the email string */

  u_char *hp; /**< The homepage URL of the poster */
  unsigned long hp_len; /**< The length of the homepage string */

  u_char *img; /**< The image URL of the poster */
  unsigned long img_len; /**< The length of the image string */

  time_t date; /**< The date this message was created */
  unsigned short level; /**< The indent level of the posting */
  u_int32_t votes_good; /**< The good votings */
  u_int32_t votes_bad; /**< The bad votings */
  
  short may_show; /**< The visibility flag for plugins to use to hide postings */
  short invisible; /**< The visibility flag (if 0, posting is deleted) */

  t_cf_template tpl; /**< The template object corresponding to this message */

  struct s_message *next; /**< The pointer to the next message in the chain */
  struct s_message *prev; /**< The pointer to the previous message in the chain */
} t_message;

/** This struct is used to store and handle whole threads */
typedef struct s_cl_thread {
  u_int64_t tid; /**< The thread id */
  u_int32_t msg_len; /**< The number of messages */

  t_message *messages; /**< Pointer to the first message (thread message) in the chain */
  t_message *last; /**< Pointer to the last message in the chain */
  t_message *threadmsg; /**< Pointer to the message the user wants to see */
} t_cl_thread;

/**
 * This function prototype pointer is used for the authorization and the
 * initialization plugins
 * \param hash The CGI object hash
 * \param dc The default configuration object
 * \param vc The fo_view configuration object
 * \return FLT_OK, FLT_DECLINE or FLT_EXIT. FLT_EXIT terminates the program, FLT_OK terminates the plugin handling in this state.
 */
typedef int (*t_filter_begin)(t_cf_hash *,t_configuration *,t_configuration *);

/**
 * This function prototype pointer is used for the connection plugins
 * \param hash The CGI object hash
 * \param dc The default configuration object
 * \param vc The fo_view configuration object
 * \param sock The connection handle or, if defined CF_SHARED_MEM, the pointer to the shared memory segment
 * \return FLT_OK, FLT_DECLINE or FLT_EXIT. FLT_EXIT terminates the program
 */
#ifndef CF_SHARED_MEM
typedef int (*t_filter_connect)(t_cf_hash *,t_configuration *,t_configuration *,int);
#else
typedef int (*t_filter_connect)(t_cf_hash *,t_configuration *,t_configuration *,void *);
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
typedef int (*t_filter_list)(t_cf_hash *,t_configuration *,t_configuration *,t_cl_thread *,int);

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
typedef int (*t_filter_list_posting)(t_cf_hash *,t_configuration *,t_configuration *,t_message *,u_int64_t,int);

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
typedef int (*t_filter_404_handler)(t_cf_hash *,t_configuration *,t_configuration *,u_int64_t,u_int64_t);

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
typedef int (*t_filter_posting)(t_cf_hash *,t_configuration *,t_configuration *,t_cl_thread *,t_cf_template *);

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
typedef int (*t_filter_init_view)(t_cf_hash *,t_configuration *,t_configuration *,t_cf_template *,t_cf_template *);

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
typedef int (*t_new_post_filter)(t_cf_hash *,t_configuration *,t_configuration *,t_message *,int,int);
#else
typedef int (*t_new_post_filter)(t_cf_hash *,t_configuration *,t_configuration *,t_message *,void *,int);
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
typedef int (*t_after_post_filter)(t_cf_hash *,t_configuration *,t_configuration *,t_message *,u_int64_t);

/**
 * In this hash global values can be saved,
 * e.g. the username of logged in users
 */
extern t_cf_hash *GlobalValues;

/**
 * In this hash the module API entries will be saved
 */
extern t_cf_hash *APIEntries;

/**
 * contains error string in failure case
 */
extern u_char ErrorString[];

/**
 * This function tries to find the path to the user configuration file
 * \param uname The username
 * \return NULL If the configuration file of the user could not be found, the full path if it could be found
 */
u_char *get_uconf_name(const u_char *uname);

/**
 * This function creates a socket handle an connects to the server
 * \return -1 on error, the socket on success
 */
int set_us_up_the_socket(void);

/**
 * This function spits out an error message by a error string
 * \param msg The error string
 * \param out A file handle to write to. If NULL, stdout will be used
 * \param rd The length of the error string
 */
void str_error_message(const u_char *msg,FILE *out, ...);

/**
 * This function returns an error message
 * \param msg The error string
 * \param rd The length of the error string
 * \param len A reference to a size_t-variable. This variable will be set to the length of the error message.
 * \return NULL on error or a u_char pointer to the error message on success.
 * \attention You have to free() the returned u_char pointer!
 */
u_char *get_error_message(const u_char *msg,size_t *len, ...);

/**
 * This function creates a date string. It's more general than get_time(),
 * actually get_time() is a wrapper around it
 * \param fmt Date format
 * \param locale The date locale
 * \param len In this variable the length of the string will be stored
 * \param date The date itself
 * \return NULL on failure, the date string on success
 */
u_char *general_get_time(u_char *fmt,u_char *locale,int *len,time_t *date);

/**
 * This function creates a date string
 * \param cfg The configuration structure
 * \param symbol The configuration symbol. E.g. "DateFormatThreadView"
 * \param len In this variable the length of the string will be stored
 * \param date The date
 * \return Returns NULL on failure or the string on success
 */
u_char *get_time(t_configuration *cfg,const u_char *symbol,int *len,time_t *date);

/**
 * This function generates a link to a thread
 * \param link The link string (if NULL, PostingURL or UPostingURL will be used)
 * \param tid The thread id
 * \param mid The message id
 * \return Returns NULL on failure or the link string on success
 * \attention You have to free() the returned pointer!
 */
u_char *get_link(const u_char *link,u_int64_t tid,u_int64_t mid);

/**
 * This function generates a link to a thread and appends some parameters (as query string)
 * \param link The link string
 * \param tid The thread id
 * \param mid The message id
 * \param l A reference, the new length will be stored in it (if NULL, it'll be ignored)
 * \return Returns the link string
 * \attention You have to free() the returned pointer!
 */
u_char *advanced_get_link(const u_char *link,u_int64_t tid,u_int64_t mid,const u_char *parameters,size_t plen,size_t *l);

/**
 * This function checks if a message has answers
 * \param msg The message structure
 * \returns
 */
int has_answers(t_message *msg);

/**
 * This function runs VIEW_HANDLER plugins on a
 * completely read thread.
 * \param thr Thread structure
 * \param head The CGI hash
 * \param mode 0 if in thread list, 1 if in thread view
 * \return Returns the return value of the last plugin
 */
int handle_thread(t_cl_thread *thr,t_cf_hash *head,int mode);

/**
 * This function runs VIEW_LIST_HANDLER plugins
 * \param p The posting structure
 * \param head The CGI hash
 * \param tid The ID of the thread
 * \param mode 0 if in thread list, 1 if in thread view
 * \return Returns the return value of the last plugin
 */
int handle_thread_list_posting(t_message *p,t_cf_hash *head,u_int64_t tid,int mode);

/**
 * This function deletes a posting subtree
 * \param msg The message structure
 * \return Returns the next posting which has not been deleted
 */
t_message *delete_subtree(t_message *msg);

/**
 * This function search the first message in the next subtree
 * \param msg The message structure
 * \return Returns the pointer to the first message in the next subtree
 */
t_message *next_subtree(t_message *msg);

/**
 * This function search the first message in the previous subtree
 * \param msg The message structure
 * \return Returns the pointer to the first message in the previous subtree
 */
t_message *prev_subtree(t_message *msg);

/**
 * This function searches for the parent message of the given posting
 * \param tmsg The message
 * \return NULL if there is no parent posting, otherwise a pointer to the parent posting
 */
t_message *parent_message(t_message *tmsg);

/**
 * This function generates a template name
 * \param buff The buffer in which the name should be saved
 * \param len The maximal length of the buffer
 * \param v The configuration entry of the template
 */
void generate_tpl_name(u_char buff[],int len,t_name_value *v);

/**
 * This function generates a template name
 * \param buff The buffer in which the name should be saved
 * \param len The maximal length of the buffer
 * \param name The template name string
 */
void gen_tpl_name(u_char buff[],int len,const u_char *name);

#ifdef CF_SHARED_MEM
/**
 * This function gets a pointer to the shared memory segment
 * \return Returns NULL if segment does not exist, otherwise it returns the pointer
 */
void *get_shm_ptr(void);

/**
 * This function re-gets the pointer to the shared memory segment
 * \return Returns NULL if segment does not exist, otherwise it returns the pointer
 */
void *reget_shm_ptr();
#endif

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
void cf_set_variable(t_cf_template *tpl,t_name_value *cs,u_char *vname,const u_char *val,size_t len,int html);

/**
 * This function frees the complete thread structure
 * \param thr The thread structure
 */
void cleanup_struct(t_cl_thread *thr);

/**
 * This function runs POSTING_HANDLER plugins
 * \param head The CGI hash
 * \param thr The thread structure
 * \param tpl The template structure
 * \param vc The alternative configuration file
 * \return The last return value of the filter
 */
int handle_posting_filters(t_cf_hash *head,t_cl_thread *thr,t_cf_template *tpl,t_configuration *vc);

/**
 * This function gets a message from the server
 * \param sock The socket
 * \param tsd The readline buffer
 * \param thr The thread structure
 * \param tplname The template name
 * \param tid The thread id
 * \param mid The message id
 * \param del Boolean. If CF_KILL_DELETED, deleted messages will be killed. If CF_KEEP_DELETED, deleted messages will not be killed
 * \return 0 on success, -1 on failure
 */
int cf_get_message_through_sock(int sock,rline_t *tsd,t_cl_thread *thr,const u_char *tplname,u_int64_t tid,u_int64_t mid,int del);

/**
 * This function reads the next thread from a socket
 * \param sock The socket
 * \param tsd The readline buffer
 * \param thr The thread structure
 * \param tplname The template name
 */
int cf_get_next_thread_through_sock(int sock,rline_t *tsd,t_cl_thread *thr,const u_char *tplname);

#ifdef CF_SHARED_MEM
/**
 * This function gets a message from the server
 * \param shm_ptr The pointer to the shared memory segment
 * \param thr The thread structure
 * \param tplname The template name
 * \param tid The thread id
 * \param mid The message id
 * \param del Boolean. If CF_KILL_DELETED, deleted messages will be killed. If CF_KEEP_DELETED, deleted messages will not be killed
 * \return 0 on success, -1 on failure
 */
int cf_get_message_through_shm(void *shm_ptr,t_cl_thread *thr,const u_char *tplname,u_int64_t tid,u_int64_t mid,int del);

/**
 * This function reads the next thread from the shared memory segment
 * \param shm_ptr The pointer to the shared memory segment
 * \param thr The thread structure
 * \param tplname The path to the template
 * \return NULL on failure, the modified pointer on success
 */
void *cf_get_next_thread_through_shm(void *shm_ptr,t_cl_thread *thr,const u_char *tplname);
#endif

/**
 * This function registeres a module API entry
 * \param mod_name The name of the module which registeres
 * \param unique_identifier A unique string to identify this API hook
 * \param func A pointer to the API function
 * \return Returns 0 on success and -1 on failure (e.g. doubly entries)
 */
int cf_register_mod_api_ent(const u_char *mod_name,const u_char *unique_identifier,t_mod_api func);

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
t_mod_api cf_get_mod_api_ent(const u_char *unid);


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
