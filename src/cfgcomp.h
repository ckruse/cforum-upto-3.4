/**
 * \file configlexer.c
 * \author Christian Kruse, <cjk@wwwtech.de>
 *
 * This file contains the configuration parser interface
 */

#ifndef _CF_CONFIG_H
#define _CF_CONFIG_H

/* {{{ parser constants */
#define CF_NO_TOK       0x0
#define CF_TOK_DQ       0x1
#define CF_TOK_SQ       0x2
#define CF_TOK_LPAREN   0x3
#define CF_TOK_RPAREN   0x4
#define CF_TOK_DOT      0x5
#define CF_TOK_PLUS     0x6
#define CF_TOK_MINUS    0x7
#define CF_TOK_DIV      0x8
#define CF_TOK_MULT     0x9
#define CF_TOK_SEMI     0xA
#define CF_TOK_PERC     0xB
#define CF_TOK_EQ       0xC
#define CF_TOK_SET      0xD
#define CF_TOK_NOTEQ    0xE
#define CF_TOK_NOT      0xF
#define CF_TOK_LTEQ     0x10
#define CF_TOK_LT       0x11
#define CF_TOK_GTEQ     0x12
#define CF_TOK_GT       0x13
#define CF_TOK_NUM      0x14
#define CF_TOK_IF       0x15
#define CF_TOK_ELSEIF   0x16
#define CF_TOK_ELSE     0x17
#define CF_TOK_WITH     0x18
#define CF_TOK_END      0x19
#define CF_TOK_AND      0x1A
#define CF_TOK_OR       0x1B
#define CF_TOK_IDENT    0x1C
#define CF_TOK_DOLLAR   0x1D
#define CF_TOK_EOF      0x1E
#define CF_TOK_STRING   0x1F
#define CF_TOK_COMMA    0x20
#define CF_TOK_ARRAY    0x21
#define CF_TOK_STMT     0x22
#define CF_TOK_LOAD     0x23
#define CF_TOK_RBRACKET 0x24
#define CF_TOK_LBRACKET 0x25
#define CF_TOK_FID      0x26
#define CF_TOK_COMMARPAREN 0x27
#define CF_TOK_NEXTTOK  0x28

#define CF_TOK_IFELSEELSEIF 0x29
/* }}} */

/* {{{ return values (parser and lexer) */
#define CF_RETVAL_OK               0
#define CF_RETVAL_NOSUCHFILE      -1
#define CF_RETVAL_FILENOTREADABLE -2
#define CF_RETVAL_PARSEERROR      -3
/* }}} */

/* {{{ precedences */
#define CF_PREC_ATOM        100
#define CF_PREC_PT           90
#define CF_PREC_BR           83
#define CF_PREC_NOT          82
#define CF_PREC_EQ           80
#define CF_PREC_LTGT         70
#define CF_PREC_PLUSMINUS    60
#define CF_PREC_DIVMUL       50
#define CF_PREC_AND          40
#define CF_PREC_OR           30
#define CF_PREC_SET          10
/* }}} */

/* {{{ parser type constants */
#define CF_TYPE_INT 0x1
#define CF_TYPE_STR 0x2
#define CF_TYPE_ID  0x3
/* }}} */

/* {{{ ASM command constants */
#define CF_ASM_MODULE    0x1
#define CF_ASM_SET       0x2
#define CF_ASM_UNSET     0x4
#define CF_ASM_LOAD      0x5

#define CF_ASM_CPY       0x6
#define CF_ASM_EQ        0x7
#define CF_ASM_NE        0x8
#define CF_ASM_LT        0x9
#define CF_ASM_LTEQ      0xA
#define CF_ASM_GT        0xB
#define CF_ASM_GTEQ      0xC
#define CF_ASM_ADD       0xD
#define CF_ASM_SUB       0xE
#define CF_ASM_DIV       0xF
#define CF_ASM_MUL       0x10
#define CF_ASM_JMP       0x11
#define CF_ASM_JMPIF     0x12
#define CF_ASM_JMPIFNOT  0x13
#define CF_ASM_AND       0x14
#define CF_ASM_OR        0x15
#define CF_ASM_NEG       0x16
#define CF_ASM_ARRAY     0x17
#define CF_ASM_ARRAYSUBS 0x18
#define CF_ASM_ARRAYPUSH 0x19
/* }}} */

/* {{{ ASM arg types */
#define CF_ASM_ARG_REG 0x1
#define CF_ASM_ARG_NUM 0x2
#define CF_ASM_ARG_STR 0x3
#define CF_ASM_ARG_CFG 0x4
#define CF_ASM_ARG_ARY 0x5
/* }}} */

/* {{{ assembler type constants */
#define CF_ASM_T_STR  0x1
#define CF_ASM_T_ATOM 0x2
#define CF_ASM_T_LBL  0x3
/* }}} */

/* {{{ datatypes */
typedef struct {
  int type;
  u_char *val;
} cf_cfg_asm_tok_t;

typedef struct {
  u_char *name;
  u_int32_t lbl_off;
  u_int32_t *repl_offs;
  int rlen;
} cf_asm_replacements_t;

typedef struct cf_cfg_value_s {
  int type,ival;
  u_char *sval;
} cf_cfg_value_t;


typedef struct cf_cfg_trees_s cf_cfg_trees_t;

typedef struct cf_cfg_token_s {
  int type,prec,line;
  cf_cfg_value_t *data;

  cf_cfg_trees_t *arguments;
  size_t arglen;

  struct cf_cfg_token_s *left,*right,*parent;
} cf_cfg_token_t;

struct cf_cfg_trees_s {
  int type,arglen;
  void *data;

  cf_cfg_token_t *tree;

  struct cf_cfg_trees_s **arguments;
  struct cf_cfg_trees_s *next,*prev;
};

typedef struct {
  u_char *content,*pos;
  int line;

  int numtok;
  u_char *stok;

  u_char *modpath;

  cf_cfg_trees_t *trees;
} cf_cfg_stream_t;

typedef struct {
  char registers;
  int lbls_used;
} cf_cfg_vmstate_t;

typedef struct cf_cfg_config_value_s cf_cfg_config_value_t;
typedef struct cf_cfg_vm_val_s {
  int type;
  int32_t i32val;
  u_char *cval;
  u_char bval;
  cf_cfg_config_value_t *cfgval;

  struct cf_cfg_vm_val_s *ary;
  size_t alen,pos;
} cf_cfg_vm_val_t;

typedef struct {
  u_char instruction;
  size_t argcount;

  cf_cfg_vm_val_t *args;
} cf_cfg_vm_command_t;

typedef struct {
  cf_cfg_vm_val_t registers[256];
  u_char *content,*pos;
  size_t len;
} cf_cfg_vm_t;


/* configuration values */
struct cf_cfg_config_value_s {
  u_char type;
  int32_t ival;
  u_char *sval,*name;

  struct cf_cfg_config_value_s *avals;
  size_t alen;
};

typedef struct cf_cfg_config_s {
  cf_array_t modules[MOD_MAX+1]; /**< Array containing all modules */

  u_char *name;
  cf_array_t nmspcs;

  cf_tree_t args;

} cf_cfg_config_t;
/* }}} */

/* {{{ module API */
/** Plugin API. This structure saves handler configurations */
typedef struct {
  int handler; /**< The handler hook */
  void *func; /**< A pointer to the handler function */
} cf_handler_config_t;

/** This function type is used for cleaning up module specific data */
typedef void (*cf_finish_t)(void);

/** This function type is used for the configuration init hook */
typedef int (*cf_config_init_hook_t)(cf_cfg_config_t *file);

/** This function type is used for cache revalidation */
#ifndef CF_SHARED_MEM
typedef int (*cf_cache_revalidator_t)(cf_hash_t *,cf_cfg_config_t *,time_t,int);
#else
typedef int (*cf_cache_revalidator_t)(cf_hash_t *,cf_cfg_config_t *,time_t,void *);
#endif

/** This function type is used for the HTTP header hook */
#ifndef CF_SHARED_MEM
typedef int (*cf_header_hook_t)(cf_hash_t *,cf_hash_t *,cf_cfg_config_t *,int);
#else
typedef int (*cf_header_hook_t)(cf_hash_t *,cf_hash_t *,cf_cfg_config_t *,void *);
#endif

/** This function type is used for the Last Modified header revalidation */
#ifndef CF_SHARED_MEM
typedef time_t (*cf_last_modified_t)(cf_hash_t *,cf_cfg_config_t *,int);
#else
typedef time_t (*cf_last_modified_t)(cf_hash_t *,cf_cfg_config_t *,void *);
#endif

/** This structure saves a configuration of a module */
typedef struct {
  u_int32_t module_magic_cookie;
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
/* }}} */

/* {{{ prototypes */
int cf_cfg_lexer(cf_cfg_stream_t *stream,int changestate);
u_char *cf_dbg_get_token(int ttype);

int cf_cfg_parser(cf_cfg_stream_t *stream,cf_cfg_trees_t *exec,cf_cfg_token_t *cur,int level,int retat);
int cf_cfg_codegenerator(cf_cfg_stream_t *stream,cf_cfg_trees_t *trees,cf_cfg_vmstate_t *state,cf_string_t *str,int regarg);
int cf_cfg_assemble(const u_char *filename,cf_string_t *str);
int cf_cfg_cfgcomp_compile_if_needed(const u_char *filename);
int cf_cfg_vm_start(cf_cfg_vm_t *me,cf_cfg_config_t *cfg);

void cf_cfg_parser_destroy_tokens(cf_cfg_token_t *tok);
void cf_cfg_parser_destroy_stream(cf_cfg_stream_t *stream);
void cf_cfg_parser_destroy_trees(cf_cfg_trees_t *tree);

void cf_cfg_init_cfg(cf_cfg_config_t *cfg);
void cf_cfg_config_destroy(cf_cfg_config_t *cfg);
int cf_cfg_cmp(cf_tree_dataset_t *dt,cf_tree_dataset_t *dt1);
/* }}} */


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
 * destructor function for the modules array
 * \param mod The module structure
 */
void cf_cfg_destroy_module(cf_module_t *mod);


/**
 * This function returns the configuration entry named by name
 * \param cfg The configuration file structure
 * \param name The configuration entry name
 * \return NULL if not found, the cf_cfg_config_value_t structure on success
 */
cf_cfg_config_value_t *cf_cfg_get_value(cf_cfg_config_t *cfg,const u_char *name);

/**
 * This function returns the configuration entry named by name parameter of forum specified by forum parameter
 * \param cfg The configuration file structure
 * \param forum The forum name
 * \param name The configuration entry name
 * \param global specifies if a config value may be read from the global namespace
 * \return NULL if not found, the cf_cfg_config_value_t structure on success
 */
cf_cfg_config_value_t *cf_cfg_get_value_w_nam(cf_cfg_config_t *cfg,const u_char *forum,const u_char *name,int global);

int cf_cfg_read_conffile(cf_cfg_config_t *cfg,const u_char *fname);
int cf_cfg_get_conf(cf_cfg_config_t *cfg,const u_char **which, size_t llen);

#endif
/* eof */
