/**
 * \file configparser.h
 * \author Christian Kruse, <ckruse@wwwtech.de>
 * \brief configuration parser functions and datatypes
 *
 * This file contains the configuration parser used by this project
 */

/* {{{ Initial headers */
/*
 * $LastChangedDate$
 * $LastChangedRevision$
 * $LastChangedBy$
 *
 */
/* }}} */

#ifndef __CONFIGPARSER_H
#define __CONFIGPARSER_H

struct s_conf_opt;
struct s_configfile;

/** look at s_conf_opt */
typedef struct s_conf_opt t_conf_opt;

/** look at s_configfile */
typedef struct s_configfile t_configfile;

/**
 * This type of function is expected as a callback function
 * \param cfile The configuration file structure
 * \param entry The configuration option entry
 * \param context The context of this entry in the configfile
 * \param args The directive arguments
 * \param len The length of the arguments array
 * \return 0 on success, any other value for failure
 */
typedef int (*t_take)(t_configfile *cfile,t_conf_opt *entry,const u_char *context,u_char **args,size_t len);

/**
 * This type of function is expected as a standard callback function (if no configuration directive entry could be found)
 * \param cfile The configuration file structure
 * \param args The directive arguments
 * \param len The length of the directives array
 * \return 0 on success, any other value for failure
 */
typedef int (*t_take_default)(t_configfile *cfile,const u_char *context,u_char *name,u_char **args,size_t len);

#define CFG_OPT_NEEDED     (0x1<<0) /**< directive _must_ exist */
#define CFG_OPT_CONFIG     (0x1<<1) /**< directive may exist in fo_*.conf */
#define CFG_OPT_USER       (0x1<<2) /**< directive may exist in user config */
#define CFG_OPT_UNIQUE     (0x1<<3) /**< directive is unique, not multiple */
#define CFG_OPT_GLOBAL     (0x1<<4) /**< directive is global only, not usable in context */
#define CFG_OPT_LOCAL      (0x1<<5) /**< directive is context-local only, not usable in global */
#define CFG_OPT_NOOVERRIDE (0x1<<6)

#define CFG_OPT_SEEN       (0x1<<7) /**< flag internally used for marking as "seen" */

#define CFG_MODE_CONFIG CFG_OPT_CONFIG /**< We are in configuration mode */
#define CFG_MODE_USER   CFG_OPT_USER   /**< We are in user configuration mode */

/** This describes the structure of a config file */
struct s_conf_opt {
  u_char *name; /**< The name of the configuration directive */
  t_take callback; /**< The callback function */
  u_int32_t flags; /**< Flags for this entry */
  void *data; /**< User defined data */
};

/** The configuration file structure */
struct s_configfile {
  u_char *filename; /**< The filename */
  t_cf_hash *options; /**< The configuration options */
  t_cf_list_head options_list; /**< A list of all configuration option names, used for CFG_OPT_NEEDED */
};

/** A list of name-value-pairs */
typedef struct s_name_value {
  u_char *name; /**< The name of the directive */

  u_char **values; /**< The value array of this struct */
  size_t valnum; /**< The size of the values array */
} t_name_value;

/***** NEW config parser API *****/

/** A structure to save configuration data */
typedef struct s_configuration {
  t_cf_list_head forums;
  t_cf_tree global_directives;
} t_configuration;

typedef struct s_internal_config {
  u_char *name;
  t_cf_tree directives;
} t_internal_config;

/* module API */

/** Plugin API. This structure saves handler configurations */
typedef struct s_handler_config {
  int handler; /**< The handler hook */
  void *func; /**< A pointer to the handler function */
} t_handler_config;

/** This function type is used for cleaning up module specific data */
typedef void (*t_finish)(void);

/** This function type is used for cache revalidation */
#ifndef CF_SHARED_MEM
typedef int (*t_cache_revalidator)(t_cf_hash *,t_configuration *,t_configuration *,time_t,int);
#else
typedef int (*t_cache_revalidator)(t_cf_hash *,t_configuration *,t_configuration *,time_t,void *);
#endif

/** This function type is used for the HTTP header hook */
#ifndef CF_SHARED_MEM
typedef int (*t_header_hook)(t_cf_hash *,t_cf_hash *,t_configuration *,t_configuration *,int);
#else
typedef int (*t_header_hook)(t_cf_hash *,t_cf_hash *,t_configuration *,t_configuration *,void *);
#endif

/** This function type is used for the Last Modified header revalidation */
#ifndef CF_SHARED_MEM
typedef time_t (*t_last_modified)(t_cf_hash *,t_configuration *,t_configuration *,int);
#else
typedef time_t (*t_last_modified)(t_cf_hash *,t_configuration *,t_configuration *,void *);
#endif

/** This structure saves a configuration of a module */
typedef struct s_module_config {
  t_conf_opt *cfgopts; /**< The configuration directive options of this module */
  t_handler_config *handlers; /**< The handler configuration */
  t_cache_revalidator revalidator; /**< The cache revalidation hook */
  t_last_modified last_modified; /**< The last modified validation hook */
  t_header_hook header_hook; /**< Hook for http headers */
  t_finish finish; /**< The cleanup function handler */
} t_module_config;

/** This structure saves the information of all modules in a linked list */
typedef struct s_module {
  void *module; /**< The pointer to the module object */
  t_handler_config *handler; /**< The handlers of the actual module */
  t_module_config *cfg; /**< The module configuration */
} t_module;

extern t_configuration fo_default_conf; /**< The configuration data of the default configuration */
extern t_configuration fo_server_conf; /**< The configuration data of the server */
extern t_configuration fo_view_conf; /**< The configuration data of the forum viewer */
extern t_configuration fo_arcview_conf; /**< The configuration data of the archive viewer */
extern t_configuration fo_post_conf; /**< The configuration data of the archive viewer */
extern t_configuration fo_vote_conf; /**< The configuration data of the voting program */

extern t_conf_opt default_options[]; /**< The default configuration options */
extern t_conf_opt fo_view_options[]; /**< The client configuration options */
extern t_conf_opt fo_post_options[]; /**< The posting configuration options */
extern t_conf_opt fo_server_options[]; /**< The server configuration options */
extern t_conf_opt fo_arcview_options[]; /**< The archiv viewer configuration options */
extern t_conf_opt fo_vote_options[]; /**< The voting program configuration options */

extern t_array Modules[]; /**< The modules array */

/***** NEW config parser API ****/


/* functions */

/**
 * This function expects a list of config names (e.g. fo_default, fo_view, fo_post)
 * and its length. It uses the CF_CONF_DIR environment variable to decide where
 * the config files can be found. It returns successfully an array, when all
 * wanted files could be found. It returns NULL if one of them could not be found.
 * \param which The list of wanted configuration files
 * \param llen The length of the list
 * \return An array containing the full path to the config files in the order given by the list
 */
t_array *get_conf_file(const u_char **which,size_t llen);

/**
 * This function parses a configuration file.
 * \param conf The configuration file structure
 * \param deflt The default callback function
 * \return 0 on success, a value unequal 0 on failure
 */
int read_config(t_configfile *conf,t_take_default deflt,int mode);

/**
 * This function initializes a configuration file structure
 * \param conf The configuration file structure
 * \param filename The configuration file filename
 */
void cfg_init_file(t_configfile *conf,u_char *filename);

/**
 * This function registeres configuration options in the configuration file structure
 * \param conf The configuration file structure
 * \param opts The configuration options
 */
int cfg_register_options(t_configfile *conf,t_conf_opt *opts);

/**
 * This function cleans up a configuration file structure
 * \param conf The configuration file structure
 */
void cfg_cleanup_file(t_configfile *conf);

/**
 * This function cleans up the modules structure
 * \param modules The modules array
 */
void cleanup_modules(t_array *modules);

/**
 * This function handles a configuration entry.
 * \param cfile The configuration file structure
 * \param opt The configuration option entry
 * \param context The context of the entry in the file
 * \param args The argument list
 * \param argnum The length of the argument list
 * \return 0 on success, any other value on error
 */
int handle_command(t_configfile *cfile,t_conf_opt *opt,const u_char *context,u_char **args,size_t argnum);

/**
 * This function adds a plugin into the program space
 * \param cfile The configuration file structure
 * \param path The path to the module
 * \param name The name of the module
 * \return 0 on success, any other value on error
 */
int add_module(t_configfile *cfile,const u_char *path,const u_char *name);

/**
 * This function cleans up a configuration file structure
 * \param cfg The configuration file structure
 */
void cfg_cleanup(t_configuration *cfg);

/**
 * This function returns a configuration entry list
 * \param cfg The configuration file structure
 * \param name The configuration entry name
 * \return NULL if not found, the t_cf_list_head structure on success
 */
t_cf_list_head *cfg_get_value(t_configuration *cfg,const u_char *context,const u_char *name);

/**
 * This function returns the first configuration entry of a configuration
 * entry list
 * \param cfg The configuration file structure
 * \param name The configuration entry name
 * \return NULL if not found, the t_name_value structure on success
 */
t_name_value *cfg_get_first_value(t_configuration *cfg,const u_char *context,const u_char *name);

/**
 * destructor function for the modules array
 * \param element An array element
 */
void cfg_destroy_module(void *element);

/**
 * configuration initialization function
 */
void cfg_init(void);

void cf_destroy_modules_array(void *arg);

#endif

/* eof */
