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

static t_array flt_scoring_ary         = { 0, 0, 0, NULL, NULL };
static t_string flt_scoring_str        = { 0, 0, NULL };
static int      flt_scoring_number     = 0;
static int      flt_scoring_hide_score = 0;

static u_char flt_scoring_base_color[3] = { 127, 0, 0 };


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

^[[:space:]]*Score:[[:space:]]*[-0-9]+\n  {
  u_char *ptr;
  for(ptr=yytext;!isdigit(*ptr) && *ptr != '-';ptr++);
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

/* {{{ flt_scoring_execute */
int flt_scoring_execute(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_message *msg,u_int64_t tid,int mode) {
  int res;
  size_t i;
  struct s_scoring_filter *flt;
  int score = 0;
  size_t len;
  u_char buff[10];

  if(mode == 0) {
    if(flt_scoring_ary.elements) {
      for(i=0;i<flt_scoring_ary.elements;i++) {
        flt = array_element_at(&flt_scoring_ary,i);

        switch(flt->field) {
          case FLT_SCORING_SUBJECT:
            res = pcre_exec(flt->regex, flt->regex_extra, msg->subject, msg->subject_len, 0, 0, NULL,0);
            break;
          case FLT_SCORING_AUTHOR:
            res = pcre_exec(flt->regex,flt->regex_extra,msg->author,msg->author_len,0,0,NULL,0);
            break;
          default:
            res = -1;
        }

        if(res >= 0) score += flt->score;
      }

      /* calculate color from score */
      if(score) {
        if(flt_scoring_hide_score && score <= flt_scoring_hide_score) {
          msg->may_show = 0;
        }
        else {
          len = snprintf(buff,8,"#%02x%02x%02x",flt_scoring_base_color[0]+score,flt_scoring_base_color[1],flt_scoring_base_color[2]);
          tpl_cf_setvar(&msg->tpl,"flt_scoring_color",buff,len,0);
        }
      }

      return FLT_OK;
    }
  }

  return 0;
}
/* }}} */

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
        if((filter.regex = pcre_compile(flt_scoring_str.content,PCRE_UTF8,(const char **)&error,&err_offset,NULL)) == NULL) {
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

/* {{{ flt_scoring_cols */
int flt_scoring_cols(t_configfile *cf,t_conf_opt *opt,u_char **args,int argnum) {
  size_t len = strlen(args[0]);
  u_char *ptr, *col;
  u_char tmp[3];
  int i;

  col = flt_scoring_base_color;

  tmp[2] = '\0';

  switch(len) {
    case 3:
      for(i=0,ptr=args[0];*ptr;ptr++,i++) {
        tmp[0] = *ptr;
        tmp[1] = *ptr;
        col[i] = strtol(tmp,NULL,16);
      }
      break;
    case 6:
      for(i=0,ptr=args[0];*ptr;ptr+=2,i++) {
        tmp[0] = *ptr;
        tmp[1] = *(ptr+1);
        col[i] = strtol(tmp,NULL,16);
      }
      break;
    default:
      return -10;
  }

  return 0;
}
/* }}} */

int flt_scoring_hide(t_configfile *cf,t_conf_opt *opt,u_char **args,int argnum) {
  flt_scoring_hide_score = atoi(args[0]);
}

void flt_scoring_finish(void) {
  size_t i = 0;
  struct s_scoring_filter *f;

  for(i=0;i<flt_scoring_ary.elements;i++) {
    f = array_element_at(&flt_scoring_ary,i);
    pcre_free(f->regex_extra);
    pcre_free(f->regex);
  }

  array_destroy(&flt_scoring_ary);
}

t_conf_opt flt_scoring_config[] = {
  { "ScoringFilter",     flt_scoring_parse, NULL },
  { "ScoringStartColor", flt_scoring_cols,  NULL },
  { "ScoringHideScore",  flt_scoring_hide,  NULL },
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
  flt_scoring_finish
};


/* eof */
