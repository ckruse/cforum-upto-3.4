%{
/**
 * \file flt_scoring.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 *
 * This plugin implements usenet like scoring
 */

/* {{{ Initial comments */
/*
 * $LastChangedDate: 2004-06-09 15:55:53 +0200 (Wed, 09 Jun 2004) $
 * $LastChangedRevision: 106 $
 * $LastChangedBy: cseiler $
 *
 */
/* }}} */

/* {{{ Includes */
#include "config.h"
#include "defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

#include <db.h>
#include <pcre.h>

#include "readline.h"
#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "cfcgi.h"
#include "template.h"
#include "clientlib.h"
/* }}} */

struct s_scoring_filter {
  int field;
  int score;
  pcre *regex;
  pcre_extra *regex_extra;
};

static t_array flt_scoring_ary     = { 0, 0, 0, NULL, NULL };
static t_string flt_scoring_str    = { 0, 0, NULL };
static int      flt_scoring_number = 0;

#define FLT_SCORING_SUBJECT 1
#define FLT_SCORING_AUTHOR  2

#define FLT_SCORING_SCORE 0x1
#define FLT_SCORING_FIELD 0x2
#define FLT_SCORING_REGEX 0x4
#define FLT_SCORING_EOF   0x8

/*
 * [score] fieldname: regex
 * # comment
 */
%}

%option noyywrap

/* {{{ lex patterns */
%%
\n           /* newlines are ok */

^[[:space:]]*Score:[[:space:]]*10\n  {
  u_char *ptr;
  for(ptr=yytext;!isdigit(*ptr);ptr++);
  flt_scoring_number = atoi(ptr);
  return FLT_SCORING_SCORE;
}

^[[:space:]]*Field:[[:space:]]*[a-zA-Z]+\n {
  u_char *ptr;
  for(ptr=yytext;isspace(*ptr);ptr++);
  ptr += 6;
  for(;isspace(*ptr);ptr++);
  str_char_set(&flt_scoring_str,ptr,strlen(ptr)-1);
  return FLT_SCORING_FIELD;
}

^[[:space:]]*Regex:[[:space:]]*[^\n]+\n {
  u_char *ptr;
  for(ptr=yytext;isspace(*ptr);ptr++);
  ptr += 6;
  for(;isspace(*ptr);ptr++);
  str_char_set(&flt_scoring_str,ptr,strlen(ptr)-1);
  return FLT_SCORING_REGEX;
}

.

<<EOF>> return FLT_SCORING_EOF;

%%

/* }}} */

int flt_scoring_execute(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_message *msg,u_int64_t tid,int mode) {
  return 0;
}

/* {{{ flt_scoring_parse */
int flt_scoring_parse(t_configfile *cf,t_conf_opt *opt,u_char **args,int argnum) {
  u_char *error,*ptr;
  struct s_scoring_filter filter;
  int err_offset,ret;
  YY_BUFFER_STATE yybuff;

  array_init(&flt_scoring_ary,sizeof(filter),NULL);
  yybuff = yy_scan_string(args[0]);

  flt_scoring_number = 1;
  str_init(&flt_scoring_str);

  do {
    ret = flt_scoringlex();
    
    switch(ret) {
      case FLT_SCORING_FIELD:
        if(toupper(*flt_scoring_str.content) == 'S') filter.field = FLT_SCORING_SUBJECT;
        else                                         filter.field = FLT_SCORING_AUTHOR;
        break;
      case FLT_SCORING_REGEX:
        if((filter.regex = pcre_compile(flt_scoring_str.content,0,(const char **)&error,&err_offset,NULL)) == NULL) {
          fprintf(stderr,"regex error in regex '%s': %s\n",flt_scoring_str.content,error);
          return -10;
        }
        filter.regex_extra = pcre_study(filter.regex, 0, (const char **)&error);

        array_push(&flt_scoring_ary,&filter);
        break;
      case FLT_SCORING_SCORE:
        filter.score       = flt_scoring_number;
        flt_scoring_number = 1;
        break;
    }
  } while(ret != FLT_SCORING_EOF);

  yy_delete_buffer(yybuff);
  str_cleanup(&flt_scoring_str);

  return 0;
}
/* }}} */

t_conf_opt flt_scoring_config[] = {
  { "ScoringFilter", flt_scoring_parse, NULL },
  { NULL, NULL, NULL }
};

t_handler_config flt_scoring_handlers[] = {
  { VIEW_LIST_HANDLER,    flt_scoring_execute },
  { 0, NULL }
};

t_module_config flt_scoring = {
  flt_scoring_config,
  flt_scoring_handlers,
  NULL,
  NULL,
  NULL,
  NULL
};


/* eof */
