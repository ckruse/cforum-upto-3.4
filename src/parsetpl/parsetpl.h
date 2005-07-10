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
  t_string *data;
} t_token;

struct s_function;

typedef struct s_context {
  t_string   output;
  t_string   output_mem;
  t_array    foreach_var_stack;
  t_array    if_level_stack;
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
} t_context;

typedef struct s_function {
  t_string name;
  t_array  params;
  t_context *ctx;
} t_function;



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
extern t_cf_hash *defined_functions;
extern t_array *defined_function_list;
extern t_context *current_context;
extern t_string string;
extern t_string content;
extern t_string content_backup;
extern t_string current_file;
extern t_context global_context;

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

int peek_next_nws_type(t_array *data);
int peek_for_hash(t_array *data);

int dereference_variable(t_string *out_str,t_token *var,t_array *data,t_string *c_var);
int dereference_iterator(t_string *out_str,t_token *var,t_array *data,t_string *c_var);

void write_parser_functions_def(FILE *ofp, t_string *func_name, t_context *ctx, t_array *params);
void write_parser_functions(FILE *ofp, t_string *func_name, t_context *ctx, t_array *params);

int process_array_assignment(t_array *data,t_string *tmp);
int process_variable_assignment_tag(t_token *variable,t_array *data);
int process_variable_print_tag(t_token *variable,t_array *data);
int process_iterator_print_tag (t_token *iterator,t_array *data);
int process_include_tag(t_string *file);
int process_foreach_tag(t_array *data);
int process_if_tag(t_array *data, int is_elseif);
int process_func_tag(t_array *data);
int process_func_call_tag(t_array *data);
int process_tag(t_array *data);

void init_context(t_context *context);
void destroy_context(t_context *context);
void init_function(t_function *func);
void destroy_function(void *arg);
void destroy_token(void *t);
void append_escaped_string(t_string *dest,t_string *src);

#endif

/* eof */
