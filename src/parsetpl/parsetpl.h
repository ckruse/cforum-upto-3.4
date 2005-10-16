/**
 * \file parsetpl.h
 * \author Christian Seiler, <self@christian-seiler.de>
 *
 * Header definitions for the template parser
 */

#ifndef _PARSETPL_H_
#define _PARSETPL_H_

#define CF_PARSER_VER "1.1"

typedef struct s_token {
  int type;
  cf_string_t *data;
} token_t;

struct s_function;

typedef struct s_context {
  cf_string_t   output;
  cf_string_t   output_mem;
  cf_array_t    foreach_var_stack;
  cf_array_t    if_level_stack;
  long       n_assign_vars;
  long       n_cur_assign_vars;
  long       n_foreach_vars;
  long       n_cur_foreach_vars;
  long       n_if_vars;
  long       n_cur_if_vars;
  long       n_if_iters;
  long       n_cur_if_iters;
  long       n_call_vars;
  long       n_cur_call_vars;
  long       n_call_iters;
  long       n_cur_call_iters;
  int        uses_include;
  int        uses_print;
  int        uses_iter_print;
  int        uses_clonevar;
  int        uses_loopassign;
  int        uses_tmpstring;
  int        iws;
  int        nle;

  struct s_function *function;
} context_t;

typedef struct s_function {
  cf_string_t name;
  cf_array_t  params;
  context_t *ctx;
} function_t;



#define YY_DECL int parsetpl_lex(void)

/*
 * token constants
 */
#define PARSETPL_TOK_EOF                 0x00
#define PARSETPL_TOK_TAGSTART            0x01
#define PARSETPL_TOK_TAGEND              0x02
#define PARSETPL_TOK_WHITESPACE          0x03
#define PARSETPL_TOK_VARIABLE            0x04
#define PARSETPL_TOK_ASSIGNMENT          0x05
#define PARSETPL_TOK_STRING              0x06
#define PARSETPL_TOK_INTEGER             0x07
#define PARSETPL_TOK_ARRAYSTART          0x08
#define PARSETPL_TOK_ARRAYSEP            0x09
#define PARSETPL_TOK_ARRAYEND            0x10
#define PARSETPL_TOK_IF                  0x11
#define PARSETPL_TOK_COMPARE             0x12
#define PARSETPL_TOK_NOT                 0x13
#define PARSETPL_TOK_ELSIF               0x14
#define PARSETPL_TOK_ELSE                0x15
#define PARSETPL_TOK_ENDIF               0x16
#define PARSETPL_TOK_INCLUDE             0x17
#define PARSETPL_TOK_FOREACH             0x18
#define PARSETPL_TOK_AS                  0x19
#define PARSETPL_TOK_ENDFOREACH          0x20
#define PARSETPL_TOK_MODIFIER_ESCAPE     0x21
#define PARSETPL_TOK_LOOPVAR             0x22
#define PARSETPL_TOK_HASHASSIGNMENT      0x23
#define PARSETPL_TOK_CONCAT              0x24
#define PARSETPL_TOK_IWS_START           0x25
#define PARSETPL_TOK_IWS_END             0x26
#define PARSETPL_TOK_NLE_START           0x27
#define PARSETPL_TOK_NLE_END             0x28
#define PARSETPL_TOK_FUNC                0x29
#define PARSETPL_TOK_FUNC_END            0x30
#define PARSETPL_TOK_FUNC_CALL           0x31
#define PARSETPL_TOK_PARAMS_START        0x32
#define PARSETPL_TOK_PARAMS_END          0x33

#define PARSETPL_INCLUDE_EXT     ".html"

#define PARSETPL_ERR -1
#define PARSETPL_ERR_FILENOTFOUND          -1
#define PARSETPL_ERR_UNRECOGNIZEDCHARACTER -2
#define PARSETPL_ERR_UNTERMINATEDSTRING    -3 
#define PARSETPL_ERR_UNTERMINATEDTAG       -4 
#define PARSETPL_ERR_INVALIDTAG            -5
#define PARSETPL_ERR_NOTINLOOP             -6

extern long lineno;
extern cf_hash_t *defined_functions;
extern cf_array_t *defined_function_list;
extern context_t *current_context;
extern cf_string_t string;
extern cf_string_t content;
extern cf_string_t content_backup;
extern cf_string_t current_file;
extern context_t global_context;

#ifndef parsetplleng
#define yyleng parsetplleng
#endif

#ifndef parsetplin
#define yyin parsetplin
#endif

#ifndef parsetplout
#define yyout parsetplout
#endif

#ifndef parsetpltext
#define yytext parsetpltext
#endif

extern FILE *yyin;
extern FILE *yyout;
extern char *yytext;
extern int yyleng;

int parse_file(const u_char *filename);

int peek_next_nws_type(cf_array_t *data);
int peek_for_hash(cf_array_t *data);

int dereference_variable(cf_string_t *out_str,token_t *var,cf_array_t *data,cf_string_t *c_var);
int dereference_iterator(cf_string_t *out_str,token_t *var,cf_array_t *data,cf_string_t *c_var);

void write_parser_functions_def(FILE *ofp, cf_string_t *func_name, context_t *ctx, cf_array_t *params);
void write_parser_functions(FILE *ofp, cf_string_t *func_name, context_t *ctx, cf_array_t *params);

int process_array_assignment(cf_array_t *data,cf_string_t *tmp);
int process_variable_assignment_tag(token_t *variable,cf_array_t *data);
int process_variable_print_tag(token_t *variable,cf_array_t *data);
int process_iterator_print_tag (token_t *iterator,cf_array_t *data);
int process_include_tag(cf_string_t *file);
int process_foreach_tag(cf_array_t *data);
int process_if_tag(cf_array_t *data, int is_elseif);
int process_func_tag(cf_array_t *data);
int process_func_call_tag(cf_array_t *data);
int process_tag(cf_array_t *data);

void init_context(context_t *context);
void destroy_context(context_t *context);
void init_function(function_t *func);
void destroy_function(void *arg);
void destroy_token(void *t);
void append_escaped_string(cf_string_t *dest,cf_string_t *src);

#endif

/* eof */
