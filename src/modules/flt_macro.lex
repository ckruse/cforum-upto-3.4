/*
 * flt_macro.lex
 * A module for executing filter macros
 *
 * $Header: /home/users/cvs/selfforum/src/modules/flt_macro.lex,v 1.4 2003/10/20 08:57:39 ckruse Exp $
 * $Log: flt_macro.lex,v $
 * Revision 1.4  2003/10/20 08:57:39  ckruse
 * Heavy developement
 *
 * Revision 1.3  2003/09/07 14:49:07  ckruse
 * several bugfixes and performance enhancements
 *
 * Revision 1.2  2003/04/30 21:43:10  ckruse
 * developement in progress...
 *
 * Revision 1.1  2003/04/27 14:28:58  ckruse
 * ----------------------------------------------------------------------
 *
 * developement in progress...
 *
 *
 */
%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "defines.h"
#include "configparser.h"
#include "utils.h"
#include "hashlib.h"
#include "cfcgi.h"
#include "template.h"
#include "fo_view.h"
#include "clientlib.h"

typedef struct s_flt_macro_variable {
  char *type;
  t_cf_hash *locals;
} t_flt_macro_variable;

typedef struct s_flt_macro_node {
  int type;
  void *data;

  struct s_flt_macro_node *prev,*next;
} t_flt_macro_node;

typedef struct s_flt_macro_method {
  char *name;
  int state;
  t_flt_macro_node *nodes;
} t_flt_macro_method;

typedef struct s_flt_macro_static_method {
  char *name;
  t_cf_hash *locals;
  t_flt_macro_node *nodes;
} t_flt_macro_static_method;

typedef struct s_flt_macro_class {
  char *name;
  t_cf_hash *attributes,*methods;
} t_flt_macro_class;

typedef struct s_flt_macro_macro {
  char *filename;
  FILE *fd;

  unsigned int err;
  unsigned long lineno;

  t_flt_macro_node *main;
  t_cf_hash *globals,*classes;
} t_flt_macro_macro;


#define YY_DECL int flt_macro_lex(void)

/*
 * token constants
 */
#define FLT_MACRO_TOK_EOF         0x00
#define FLT_MACRO_TOK_INTEGER_OCT 0x01
#define FLT_MACRO_TOK_INTEGER_DEC 0x02
#define FLT_MACRO_TOK_INTEGER_HEX 0x03
#define FLT_MACRO_TOK_FLOAT       0x04
#define FLT_MACRO_TOK_KEYWORD     0x05
#define FLT_MACRO_TOK_OPERATOR    0x06
#define FLT_MACRO_TOK_VARIABLE    0x07
#define FLT_MACRO_TOK_IDENTIFIER  0x08
#define FLT_MACRO_TOK_STRING      0x09

/*
 * operator constants
 */


/*
 * keyword constants (for better performance)
 */
#define FLT_MACRO_KEYWORD_class     2391368147LL
#define FLT_MACRO_KEYWORD_method    474091084LL
#define FLT_MACRO_KEYWORD_static    522641180LL
#define FLT_MACRO_KEYWORD_inherits  897936922LL
#define FLT_MACRO_KEYWORD_super     3479853179LL
#define FLT_MACRO_KEYWORD_public    3204310542LL
#define FLT_MACRO_KEYWORD_protected 683295737LL
#define FLT_MACRO_KEYWORD_if        3334774442LL
#define FLT_MACRO_KEYWORD_else      3088025808LL
#define FLT_MACRO_KEYWORD_and       914857137LL
#define FLT_MACRO_KEYWORD_or        3690580840LL
#define FLT_MACRO_KEYWORD_xor       2560094461LL
#define FLT_MACRO_KEYWORD_while     1125028631LL
#define FLT_MACRO_KEYWORD_until     3538562994LL
#define FLT_MACRO_KEYWORD_for       4138991477LL
#define FLT_MACRO_KEYWORD_global    3991919145LL
#define FLT_MACRO_KEYWORD_local     2565606558LL
#define FLT_MACRO_KEYWORD_begin     2916300717LL
#define FLT_MACRO_KEYWORD_end       1152344558LL
#define FLT_MACRO_KEYWORD_next      3483119595LL
#define FLT_MACRO_KEYWORD_break     1903481056LL
#define FLT_MACRO_KEYWORD___LINE__  2619058067LL
#define FLT_MACRO_KEYWORD___FILE__  434676975LL


/*
 * error constants
 */
#define FLT_MACRO_ERR -1
#define FLT_MACRO_ERR_FILENOTFOUND          -1
#define FLT_MACRO_ERR_UNRECOGNIZEDCHARACTER -2
#define FLT_MACRO_ERR_UNTERMINATEDCOMMENT   -3
#define FLT_MACRO_ERR_UNTERMINATEDSTRING    -4
#define FLT_MACRO_ERR_UNKNOWNKEYWORD        -5
#define FLT_MACRO_ERR_UNKNOWNOPERATOR       -6 


static char     *MacroFilesDirectory = NULL;
static long      lineno              = 0;
static t_string  string              = { 0, 0, NULL };


#ifdef _FLT_MACRO_TEST
static const char *get_token_name(int name) {
  static const char *tokens[] = {
    "FLT_MACRO_TOK_EOF",
    "FLT_MACRO_TOK_INTEGER_OCT",
    "FLT_MACRO_TOK_INTEGER_DEC",
    "FLT_MACRO_TOK_INTEGER_HEX",
    "FLT_MACRO_TOK_FLOAT",
    "FLT_MACRO_TOK_KEYWORD",
    "FLT_MACRO_TOK_OPERATOR",
    "FLT_MACRO_TOK_VARIABLE",
    "FLT_MACRO_TOK_IDENTIFIER",
    "FLT_MACRO_TOK_STRING"
  };

  return tokens[name];
}
#endif

static const char *flt_macro_get_error_message(int errnum) {
  static const char *messages[] = {
    "FLT_MACRO_ERR: unspecified error",
    "FLT_MACRO_ERR_FILENOTFOUND: could not open file, e.g. file not accesseable or not found",
    "FLT_MACRO_ERR_UNRECOGNIZEDCHARACTER: parser got an unrecognized character",
    "FLT_MACRO_ERR_UNTERMINATEDCOMMENT: got EOF before comment was closed",
    "FLT_MACRO_ERR_UNTERMINATEDSTRING: got EOF before string ended",
    "FLT_MACRO_ERR_UNKNOWNKEYWORD: treebuilder got an unknown keyword",
    "FLT_MACRO_ERR_UNKNOWNOPERATOR: treebuilder got an unknown operator"
  };

  return messages[abs(errnum)];
}

%}

/* states */
%x STRING COMMENT

/* options */
%option stack noyywrap

%%
<*>\n   {
  if(YYSTATE == STRING) {
    /* TODO: send error message (newline in string, unterminated string) */
  }

  ++lineno; /* count line numbers */
}

\" {
  if(string.content) free(string.content);
  str_init(&string);
  yy_push_state(STRING);
}

<STRING>{
  \\n                 str_char_append(&string,'\n');
  \\t                 str_char_append(&string,'\t');
  \\r                 str_char_append(&string,'\r');
  \\0[0-7]{1,3}       str_char_append(&string,(char)strtol(yytext,NULL,8));
  \\x[0-9A-Fa-f]{1,2} str_char_append(&string,(char)strtol(yytext,NULL,15));
  \\\"                str_char_append(&string,'"');
  \"                  {
    yy_pop_state();
    return FLT_MACRO_TOK_STRING;
  }
  <<EOF>> {
    return FLT_MACRO_ERR_UNTERMINATEDSTRING;
  }
  .                   str_char_append(&string,*yytext);
}

[0-9]+"."[0-9]+  return FLT_MACRO_TOK_FLOAT;
0[1-9]{2}        return FLT_MACRO_TOK_INTEGER_OCT;
[1-9][0-9]*      return FLT_MACRO_TOK_INTEGER_DEC;
0x[0-9A-Fa-f]{2} return FLT_MACRO_TOK_INTEGER_HEX;

#.*$  { // eat perl-like one line comments
}
"/*"  { // eat c-like comments
  register int c;

  for(;;) {
    while((c = input()) != '*' && c != EOF);    /* eat up text of comment */

    if(c == '*') {
      while((c = input()) == '*');
      if(c == '/') break;    /* found the end */
    }

    if(c == EOF) {
      return FLT_MACRO_ERR_UNTERMINATEDCOMMENT;
    }
  }
}

class|method|static|inherits|super|public|protected { /* the OO keywords */
  return FLT_MACRO_TOK_KEYWORD;
}

if|else|and|or|xor|while|until|for|global|local|begin|end|next|break|__LINE__|__FILE__ { /* the language keywords */
  return FLT_MACRO_TOK_KEYWORD;
}

->|!=|==|"**"|"+"|-|"*"|"/"|%|!|"["|"]"|"("|")"|,|; { /* operators */
  return FLT_MACRO_TOK_OPERATOR;
}

"$"[a-zA-Z][a-zA-Z0-9]* { /* variables */
  return FLT_MACRO_TOK_VARIABLE;
}

[a-zA-Z][a-zA-Z0-9]* { /* identifier */
  return FLT_MACRO_TOK_IDENTIFIER;
}

[ \n\t]+  /* eat up whitespaces */

. { /* hu? What's up? */
  return FLT_MACRO_ERR_UNRECOGNIZEDCHARACTER;
}

<<EOF>> return FLT_MACRO_TOK_EOF;

%%

/******************************************************
 * the tree builder
 * It builds the syntax tree with the results of our scanner (flt_macro_lex).
 */
int flt_macro_tree_builder(t_flt_macro_macro *macro) {
#ifdef _FLT_MACRO_TEST
  static const char cn[] = "flt_macro_tree_builder";
#endif

  int ret;
  uint64 key;
  t_flt_macro_node *node;

  yyin = macro->fd = fopen(macro->filename,"r");

  if(!macro->fd) {
    macro->err    = FLT_MACRO_ERR_FILENOTFOUND;
    macro->lineno = 0;

    return FLT_MACRO_ERR;
  }

  lineno = 0;
  node   = calloc(1,sizeof(t_flt_macro_node));

  do {
    ret = flt_macro_lex();

#ifdef _FLT_MACRO_TEST
    if(ret >= FLT_MACRO_TOK_EOF) fprintf(stderr,"%s: got token %s\n",cn,get_token_name(ret));
    else                         fprintf(stderr,"%s: %s\n",cn,flt_macro_get_error_message(ret));
#endif

    switch(ret) {
      case FLT_MACRO_TOK_STRING:
        break;
      case FLT_MACRO_TOK_INTEGER_OCT:
        break;
      case FLT_MACRO_TOK_INTEGER_DEC:
        break;
      case FLT_MACRO_TOK_INTEGER_HEX:
        break;
      case FLT_MACRO_TOK_FLOAT:
        break;
      case FLT_MACRO_TOK_KEYWORD:
        key = lookup(yytext,yyleng,0);
        printf("keyword: '%s'; keylen: %d; hashsum: %lld\n",yytext,yyleng,key);

        switch(key) {
          case FLT_MACRO_KEYWORD_class:
            break;
          case FLT_MACRO_KEYWORD_method:
            break;
          case FLT_MACRO_KEYWORD_static:
            break;
          case FLT_MACRO_KEYWORD_inherits:
            break;
          case FLT_MACRO_KEYWORD_super:
            break;
          case FLT_MACRO_KEYWORD_public:
            break;
          case FLT_MACRO_KEYWORD_protected:
            break;
          case FLT_MACRO_KEYWORD_if:
            break;
          case FLT_MACRO_KEYWORD_else:
            break;
          case FLT_MACRO_KEYWORD_and:
            break;
          case FLT_MACRO_KEYWORD_or:
            break;
          case FLT_MACRO_KEYWORD_xor:
            break;
          case FLT_MACRO_KEYWORD_while:
            break;
          case FLT_MACRO_KEYWORD_until:
            break;
          case FLT_MACRO_KEYWORD_for:
            break;
          case FLT_MACRO_KEYWORD_global:
            break;
          case FLT_MACRO_KEYWORD_local:
            break;
          case FLT_MACRO_KEYWORD_begin:
            break;
          case FLT_MACRO_KEYWORD_end:
            break;
          case FLT_MACRO_KEYWORD_next:
            break;
          case FLT_MACRO_KEYWORD_break:
            break;
          case FLT_MACRO_KEYWORD___LINE__:
            break;
          case FLT_MACRO_KEYWORD___FILE__:
            break;
          default:
            return FLT_MACRO_ERR_UNKNOWNKEYWORD;
        }
        break;

      case FLT_MACRO_TOK_OPERATOR:
        printf("got an operator (%s)\n",yytext);
#if 0
        if(yyleng == 1) {
          switch(*yytext) {
            case '+':
              node->type = FLT_MACRO_OP_PLUS;
              break;
            case '-':
              node->type = FLT_MACRO_OP_MINUS;
              break;
            case '*':
              node->type = FLT_MACRO_OP_MULTI;
              break;
            case '/':
              node->type = FLT_MACRO_OP_DIVIDE;
              break;
            case '%':
              node->type = FLT_MACRO_OP_MODULO;
              break;
            case '!':
              node->type = FLT_MACRO_OP_NOT;
              break;
            case '[':
              node->type = FLT_MACRO_OP_SQBRACK;
              break;
            case ']':
              break;
            case '(':
              node->type = FLT_MACRO_OP_BRACK;
              break;
            case ')':
              break;
            case ',':
              node->type = FLT_MACRO_COMMA;
              break;
            case ';':
              if(node->type) { /* ignore empty statements */
                node->next       = calloc(1,sizeof(t_flt_macro_node));
                node->next->prev = node;
                node             = node->next;
              }
              break;
            default:
              return FLT_MACRO_ERR_UNKNOWNOPERATOR;
          }
        }
        else {
          switch(*yytext) {
          case '-': /* -> */
            node->type = FLT_MACRO_OP_DEREF;
            break;
          case '!': /* != */
            node->type = FLT_MACRO_UNEQUAL;
            break;
          case '=': /* == */
            node->type = FLT_MACRO_EQUAL;
            break;
          case '*': /* ** */
            node->type = FLT_MACRO_POWER;
            break;
          default:
            return FLT_MACRO_ERR_UNKNOWNOPERATOR;
          }
        }
#endif
        break;
      case FLT_MACRO_TOK_VARIABLE:
        break;
      case FLT_MACRO_TOK_IDENTIFIER:
        break;
    }

  } while(ret > FLT_MACRO_TOK_EOF);

  return ret;
}


int flt_macro_handle_command(t_configfile *cfile,t_conf_opt *opt,char **args,int argnum) {
  if(args[0]) {
    if(MacroFilesDirectory) free(MacroFilesDirectory);
    MacroFilesDirectory = strdup(args[0]);
  }

  return 0;
}

void flt_macro_cleanup(void) {
  if(MacroFilesDirectory) free(MacroFilesDirectory);
}

t_conf_opt flt_macro_config[] = {
  { "MacroFilesDirectory",  flt_macro_handle_command, NULL },
  { NULL, NULL, NULL }
};

t_handler_config flt_macro_handlers[] = {
  /*  { VIEW_INIT_HANDLER, execute_filter           },
  { POSTING_HANDLER,   flt_basic_handle_posting },
  { VIEW_LIST_HANDLER, flt_basic_set_target     },*/
  { 0, NULL }
};

t_module_config flt_basic = {
  flt_macro_config,
  flt_macro_handlers,
  flt_macro_cleanup
};

#ifdef _FLT_MACRO_TEST
int  main(int argc,const char *argv[],const char *envp[]) {
  int ret;
  t_flt_macro_macro macro;

  memset(&macro,0,sizeof(t_flt_macro_macro));

  if(argc >= 2) {
    int i;
    for(i=1;i<argc;i++) {
      //while((ret = flt_macro_lex()) != FLT_MACRO_TOK_EOF) printf("got token '%s' (%d)\n",get_token_name(ret),ret);
      macro.filename = (char *)argv[i];
      printf("return value: %d\n",flt_macro_tree_builder(&macro));
    }
  }
  else {
    yyin = stdin;
    while((ret = flt_macro_lex()) != FLT_MACRO_TOK_EOF) printf("got token '%s' (%d)\n",get_token_name(ret),ret);
  }

  return EXIT_SUCCESS;
}
#endif

/* eof */
