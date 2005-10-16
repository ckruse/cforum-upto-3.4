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

struct cf_conf_opt_s;
struct cf_configfile_s;

/** look at s_conf_opt */
typedef struct cf_conf_opt_s cf_conf_opt_t;

/** look at s_configfile */
typedef struct cf_configfile_s cf_configfile_t;

/**
 * This type of function is expected as a callback function
 * \param cfile The configuration file structure
 * \param entry The configuration option entry
 * \param context The context of this entry in the configfile
 * \param args The directive arguments
 * \param len The length of the arguments array
 * \return 0 on success, any other value for failure
 */
typedef int (*cf_take_t)(cf_configfile_t *cfile,cf_conf_opt_t *entry,const u_char *context,u_char **args,size_t len);

/**
 * This type of function is expected as a standard callback function (if no configuration directive entry could be found)
 * \param cfile The configuration file structure
 * \param args The directive arguments
 * \param len The length of the directives array
 * \return 0 on success, any other value for failure
 */
typedef int (*cf_take_default_t)(cf_configfile_t *cfile,const u_char *context,u_char *name,u_char **args,size_t len);

#define CF_CFG_OPT_NEEDED     (0x1<<0) /**< directive _must_ exist */
#define CF_CFG_OPT_CONFIG     (0x1<<1) /**< directive may exist in fo_*.conf */
#define CF_CFG_OPT_USER       (0x1<<2) /**< directive may exist in user config */
#define CF_CFG_OPT_UNIQUE     (0x1<<3) /**< directive is unique, not multiple */
#define CF_CFG_OPT_GLOBAL     (0x1<<4) /**< directive is global only, not usable in context */
#define CF_CFG_OPT_LOCAL      (0x1<<5) /**< directive is context-local only, not usable in global */
#define CF_CFG_OPT_NOOVERRIDE (0x1<<6)

#define CF_CFG_OPT_SEEN       (0x1<<7) /**< flag internally used for marking as "seen" */

#define CF_CFG_MODE_CONFIG CF_CFG_OPT_CONFIG /**< We are in configuration mode */
#define CF_CFG_MODE_USER   CF_CFG_OPT_USER   /**< We are in user configuration mode */
#define CF_CFG_MODE_NOLOAD (0x1<<8) /**< load no plugins */

/** This describes the structure of a config file */
struct cf_conf_opt_s {
  u_char *name; /**< The name of the configuration directive */
  cf_take_t callback; /**< The callback function */
  u_int32_t flags; /**< Flags for this entry */
  void *data; /**< User defined data */
};

/** The configuration file structure */
struct cf_configfile_s {
  u_char *filename; /**< The filename */
  cf_hash_t *options; /**< The configuration options */
  cf_list_head_t options_list; /**< A list of all configuration option names, used for CF_CFG_OPT_NEEDED */
};

/** A list of name-value-pairs */
typedef struct s_name_value {
  u_char *name; /**< The name of the directive */

  u_char **values; /**< The value array of this struct */
  size_t valnum; /**< The size of the values array */
} cf_name_value_t;

/***** NEW config parser API *****/

/** A structure to save configuration data */
typedef struct {
  cf_list_head_t forums;
  cf_tree_t global_directives;
} cf_configuration_t;

typedef struct {
  u_char *name;
  cf_tree_t directives;
} cf_internal_config_t;

/* module API */

/** Plugin API. This structure saves handler configurations */
typedef struct {
  int handler; /**< The handler hook */
  void *func; /**< A pointer to the handler function */
} cf_handler_config_t;

/** This function type is used for cleaning up module specific data */
typedef void (*cf_finish_t)(void);

/** This function type is used for the configuration init hook */
typedef int (*cf_config_init_hook_t)(cf_configfile_t *file);

/** This function type is used for cache revalidation */
#ifndef CF_SHARED_MEM
typedef int (*cf_cache_revalidator_t)(cf_hash_t *,cf_configuration_t *,cf_configuration_t *,time_t,int);
#else
typedef int (*cf_cache_revalidator_t)(cf_hash_t *,cf_configuration_t *,cf_configuration_t *,time_t,void *);
#endif

/** This function type is used for the HTTP header hook */
#ifndef CF_SHARED_MEM
typedef int (*cf_header_hook_t)(cf_hash_t *,cf_hash_t *,cf_configuration_t *,cf_configuration_t *,int);
#else
typedef int (*cf_header_hook_t)(cf_hash_t *,cf_hash_t *,cf_configuration_t *,cf_configuration_t *,void *);
#endif

/** This function type is used for the Last Modified header revalidation */
#ifndef CF_SHARED_MEM
typedef time_t (*cf_last_modified_t)(cf_hash_t *,cf_configuration_t *,cf_configuration_t *,int);
#else
typedef time_t (*cf_last_modified_t)(cf_hash_t *,cf_configuration_t *,cf_configuration_t *,void *);
#endif

/** This structure saves a configuration of a module */
typedef struct {
  u_int32_t module_magic_cookie;
  cf_conf_opt_t *cfgopts; /**< The configuration directive options of this module */
  cf_handler_config_t *handlers; /**< The handler configuration */
  cf_config_init_hook_t config_init;
  cf_cache_revalidator_t revalidator; /**< The cache revalidation hook */
  cf_last_modified_t last_modified; /**< The last modified validation hook */
  cf_header_hook_t header_hook; /**< Hook for http headers */
  cf_finish_t finish; /**< The cleanup function handler */
} cf_module_config_t;

/** This structure saves the information of all modules in a linked list */
typedef struct s_module {
  void *module; /**< The pointer to the module object */
  cf_handler_config_t *handler; /**< The handlers of the actual module */
  cf_module_config_t *cfg; /**< The module configuration */
} cf_module_t;

extern cf_configuration_t fo_default_conf; /**< The configuration data of the default configuration */
extern cf_configuration_t fo_server_conf; /**< The configuration data of the server */
extern cf_configuration_t fo_view_conf; /**< The configuration data of the forum viewer */
extern cf_configuration_t fo_arcview_conf; /**< The configuration data of the archive viewer */
extern cf_configuration_t fo_post_conf; /**< The configuration data of the archive viewer */
extern cf_configuration_t fo_vote_conf; /**< The configuration data of the voting program */
extern cf_configuration_t fo_feeds_conf; /**< The configuration data of the feeds program */
extern cf_configuration_t fo_userconf_conf; /**< The configuration data of the userconfig program */

extern cf_conf_opt_t default_options[]; /**< The default configuration options */
extern cf_conf_opt_t fo_view_options[]; /**< The client configuration options */
extern cf_conf_opt_t fo_post_options[]; /**< The posting configuration options */
extern cf_conf_opt_t fo_server_options[]; /**< The server configuration options */
extern cf_conf_opt_t fo_arcview_options[]; /**< The archiv viewer configuration options */
extern cf_conf_opt_t fo_vote_options[]; /**< The voting program configuration options */
extern cf_conf_opt_t fo_feeds_options[];  /**< The feeds program configuration options */
extern cf_conf_opt_t fo_userconf_options[]; /**< The userconf program configuration options */

extern cf_array_t Modules[]; /**< The modules array */

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
cf_array_t *cf_get_conf_file(const u_char **which,size_t llen);

/**
 * This function parses a configuration file.
 * \param conf The configuration file structure
 * \param deflt The default callback function
 * \return 0 on success, a value unequal 0 on failure
 */
int cf_read_config(cf_configfile_t *conf,cf_take_default_t deflt,int mode);

/**
 * This function initializes a configuration file structure
 * \param conf The configuration file structure
 * \param filename The configuration file filename
 */
void cf_cfg_init_file(cf_configfile_t *conf,u_char *filename);

/**
 * This function registeres configuration options in the configuration file structure
 * \param conf The configuration file structure
 * \param opts The configuration options
 */
int cf_cfg_register_options(cf_configfile_t *conf,cf_conf_opt_t *opts);

/**
 * This function cleans up a configuration file structure
 * \param conf The configuration file structure
 */
void cf_cfg_cleanup_file(cf_configfile_t *conf);

/**
 * This function cleans up the modules structure
 * \param modules The modules array
 */
void cf_cleanup_modules(cf_array_t *modules);

/**
 * This function handles a configuration entry.
 * \param cfile The configuration file structure
 * \param opt The configuration option entry
 * \param context The context of the entry in the file
 * \param args The argument list
 * \param argnum The length of the argument list
 * \return 0 on success, any other value on error
 */
int cf_handle_command(cf_configfile_t *cfile,cf_conf_opt_t *opt,const u_char *context,u_char **args,size_t argnum);

/**
 * This function adds a plugin into the program space
 * \param cfile The configuration file structure
 * \param path The path to the module
 * \param name The name of the module
 * \return 0 on success, any other value on error
 */
int cf_add_module(cf_configfile_t *cfile,const u_char *path,const u_char *name);

/**
 * This function cleans up a configuration file structure
 * \param cfg The configuration file structure
 */
void cf_cfg_cleanup(cf_configuration_t *cfg);

/**
 * This function returns a configuration entry list
 * \param cfg The configuration file structure
 * \param name The configuration entry name
 * \return NULL if not found, the cf_list_head_t structure on success
 */
cf_list_head_t *cf_cfg_get_value(cf_configuration_t *cfg,const u_char *context,const u_char *name);

/**
 * This function returns the first configuration entry of a configuration
 * entry list
 * \param cfg The configuration file structure
 * \param name The configuration entry name
 * \return NULL if not found, the cf_name_value_t structure on success
 */
cf_name_value_t *cf_cfg_get_first_value(cf_configuration_t *cfg,const u_char *context,const u_char *name);

/**
 * destructor function for the modules array
 * \param element An array element
 */
void cf_cfg_destroy_module(void *element);

/**
 * configuration initialization function
 */
void cf_cfg_init(void);

void cf_destroy_modules_array(void *arg);

void cf_cfg_destroy(void);

#endif

/* eof */
