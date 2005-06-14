%{
/**
 * \file flt_scoring.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 *
 * This plugin implements usenet like scoring
 */

/* {{{ Initial comments */
/*
 * $LastChangedDate$
 * $LastChangedRevision$
 * $LastChangedBy$
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

static t_string flt_scoring_str        = { 0, 0, 0, NULL };
static int      flt_scoring_number     = 0;

static t_array flt_scoring_ary         = { 0, 0, 0, NULL, NULL };
static int     flt_scoring_hide_score  = 0;
static int     flt_scoring_hide_score_set = 0;
static int     flt_scoring_min_val     = 0;
static int     flt_scoring_max_val     = 255;
static u_char  flt_scoring_min_col[3]  = { 0, 0, 0 };
static u_char  flt_scoring_max_col[3]  = { 255, 0, 0 };
static u_char  flt_scoring_norm_col[3] = { 127, 0, 0 };
static int     flt_scoring_ign         = 0;

static u_char *flt_scoring_fn = NULL;

#define FLT_SCORING_SUBJECT 1
#define FLT_SCORING_AUTHOR  2
#define FLT_SCORING_CAT     4

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

^[[:space:]]*Score:[[:space:]]*[+-]?[0-9]+\n?  {
  u_char *ptr;
  for(ptr=yytext;!isdigit(*ptr) && *ptr != '-';ptr++);
  flt_scoring_number = atoi(ptr);
  return FLT_SCORING_SCORE;
}

^[[:space:]]*Field:[[:space:]]*[a-zA-Z]+\n? {
  u_char *ptr;
  size_t len;
  for(ptr=yytext;isspace(*ptr);ptr++);
  ptr += 6;
  for(;isspace(*ptr);ptr++);

  len = strlen(ptr);
  if(ptr[len-1] == '\n') len -= 1;

  str_char_set(&flt_scoring_str,ptr,len);
  return FLT_SCORING_FIELD;
}

^[[:space:]]*Regex:[[:space:]]*[^\n]+\n? {
  u_char *ptr;
  size_t len;
  for(ptr=yytext;isspace(*ptr);ptr++);
  ptr += 6;
  for(;isspace(*ptr);ptr++);

  len = strlen(ptr);
  if(ptr[len-1] == '\n') len -= 1;

  str_char_set(&flt_scoring_str,ptr,len);
  return FLT_SCORING_REGEX;
}

.

<<EOF>> return FLT_SCORING_EOF;

%%

/* }}} */

/* {{{ flt_scoring_calc_col */
size_t flt_scoring_calc_col(u_char buff[],int score) {
  float percentage = 0;
  int finish = (flt_scoring_max_val + flt_scoring_min_val) / 2;
  u_char scol[3];
  u_char *col = scol;
  int i;

  /* we got a positive scoring */
  if(score > finish) {
    /* calculate the matching percentage */
    if(score >= flt_scoring_max_val) percentage = 1.0;
    else percentage = (float)score / (float)flt_scoring_max_val;

    /* calculate the color (percentage * difference) */
    for(i=0;i<3;i++) scol[i] = (u_char)(flt_scoring_norm_col[i] + (float)(flt_scoring_max_col[i] - flt_scoring_norm_col[i]) * percentage);
  }
  /* we got a negative scoring */
  else if(score < finish) {
    /* calculate the matching percentage */
    if(score <= flt_scoring_min_val) percentage = 1.0;
    else percentage = (float)score / (float)flt_scoring_min_val;

    /* calculate the color (percentage * difference) */
    for(i=0;i<3;i++) scol[i] = (u_char)(flt_scoring_norm_col[i] - (float)(flt_scoring_norm_col[i] - flt_scoring_min_col[i]) * percentage);
  }
  /* we got a neutral scoring */
  else col = flt_scoring_norm_col;

  return snprintf(buff,8,"#%02x%02x%02x",col[0],col[1],col[2]);
}
/* }}} */

/* {{{ flt_scoring_execute */
int flt_scoring_execute(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_message *msg,u_int64_t tid,int mode) {
  int res = 0;
  size_t i;
  struct s_scoring_filter *flt;
  int score = 0;
  size_t len;
  u_char buff[10];

  if(mode & CF_MODE_THREADVIEW) return FLT_DECLINE;

  if(flt_scoring_ary.elements) {
    for(i=0;i<flt_scoring_ary.elements;i++) {
      flt = array_element_at(&flt_scoring_ary,i);

      switch(flt->field) {
        case FLT_SCORING_SUBJECT:
	  res = pcre_exec(flt->regex, flt->regex_extra, msg->subject.content, msg->subject.len, 0, 0, NULL,0);
	  break;
        case FLT_SCORING_AUTHOR:
	  res = pcre_exec(flt->regex,flt->regex_extra,msg->author.content,msg->author.len,0,0,NULL,0);
	  break;
        case FLT_SCORING_CAT:
	  if(msg->category.len) res = pcre_exec(flt->regex,flt->regex_extra,msg->category.content,msg->category.len,0,0,NULL,0);
	  break;
        default:
	  res = -1;
      }

      if(res >= 0) score += flt->score;
    }

    /* Does the user want to ignore non-matched postings? */
    if(score == 0 && flt_scoring_ign) return FLT_OK;

    /* has the posting to be deleted? */
    if(flt_scoring_hide_score_set && score <= flt_scoring_hide_score) {
      msg->may_show = 0;
    }
    else {
      /* no, so go and calculate the color */
      len = flt_scoring_calc_col(buff,score);
      cf_tpl_hashvar_setvalue(&msg->hashvar,"flt_scoring_color",TPL_VARIABLE_STRING,buff,len);
    }

    return FLT_OK;
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_scoring_parse */
int flt_scoring_parse(t_configfile *cf,t_conf_opt *opt,const u_char *context,u_char **args,size_t argnum) {
  u_char *error,*ptr;
  struct s_scoring_filter filter;
  int err_offset,ret;
  YY_BUFFER_STATE yybuff;

  if(flt_scoring_fn == NULL) flt_scoring_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(flt_scoring_fn,context) != 0) return 0;

  array_init(&flt_scoring_ary,sizeof(filter),NULL);
  yybuff = yy_scan_string(args[0]);

  flt_scoring_number = 1;
  str_init(&flt_scoring_str);

  do {
    ret = flt_scoringlex();
    
    switch(ret) {
      case FLT_SCORING_FIELD:
        if(toupper(*flt_scoring_str.content) == 'S')      filter.field = FLT_SCORING_SUBJECT;
        else if(toupper(*flt_scoring_str.content) == 'A') filter.field = FLT_SCORING_AUTHOR;
        else                                              filter.field = FLT_SCORING_CAT;
        break;
      case FLT_SCORING_REGEX:
        if((filter.regex = pcre_compile(flt_scoring_str.content,PCRE_UTF8|PCRE_CASELESS,(const char **)&error,&err_offset,NULL)) == NULL) {
          fprintf(stderr,"flt_scoring: regex error in regex '%s': %s\n",flt_scoring_str.content,error);
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
int flt_scoring_cols(t_configfile *cf,t_conf_opt *opt,const u_char *context,u_char **args,size_t argnum) {
  u_char *arg;
  size_t len;
  u_char *ptr, *col;
  u_char tmp[3];
  int i;

  if(flt_scoring_fn == NULL) flt_scoring_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(flt_scoring_fn,context) != 0) return 0;

  arg = args[0];
  len = strlen(args[0]);

  if(cf_strcmp(opt->name,"ScoringMinColor") == 0) col = flt_scoring_min_col;
  else if(cf_strcmp(opt->name,"ScoringNormalColor") == 0) col = flt_scoring_norm_col;
  else col = flt_scoring_max_col;

  tmp[2] = '\0';

  if(*arg == '#') {
    arg++;
    len--;
  }

  switch(len) {
    case 3:
      for(i=0,ptr=arg;*ptr;ptr++,i++) {
        tmp[0] = *ptr;
        tmp[1] = *(ptr+1);
        col[i] = (u_char)strtol(tmp,NULL,16);
      }
      break;
    case 6:
      for(i=0,ptr=arg;*ptr;ptr+=2,i++) {
        tmp[0] = *ptr;
        tmp[1] = *(ptr+1);
        col[i] = (u_char)strtol(tmp,NULL,16);
      }
      break;
    default:
      return -10;
  }

  return 0;
}
/* }}} */

/* {{{ flt_scoring_hide */
int flt_scoring_hide(t_configfile *cf,t_conf_opt *opt,const u_char *context,u_char **args,size_t argnum) {
  if(flt_scoring_fn == NULL) flt_scoring_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(flt_scoring_fn,context) != 0) return 0;

  flt_scoring_hide_score = atoi(args[0]);
  flt_scoring_hide_score_set = 1;
  return 0;
}
/* }}} */

/* {{{ flt_scoring_vals */
int flt_scoring_vals(t_configfile *cf,t_conf_opt *opt,const u_char *context,u_char **args,size_t argnum) {
  if(flt_scoring_fn == NULL) flt_scoring_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(flt_scoring_fn,context) != 0) return 0;

  if(cf_strcmp(opt->name,"ScoringMaxValue") == 0) flt_scoring_max_val = strtol(args[0],NULL,10);
  else flt_scoring_min_val = strtol(args[0],NULL,10);

  return 0;
}
/* }}} */

/* {{{ flt_scoring_ignore */
int flt_scoring_ignore(t_configfile *cf,t_conf_opt *opt,const u_char *context,u_char **args,size_t argnum) {
  if(flt_scoring_fn == NULL) flt_scoring_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(flt_scoring_fn,context) != 0) return 0;

  flt_scoring_ign = cf_strcmp(args[0],"yes") == 0;
  return 0;
}
/* }}} */

/* {{{ flt_scoring_finish */
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
/* }}} */

t_conf_opt flt_scoring_config[] = {
  { "ScoringFilter",          flt_scoring_parse,  CFG_OPT_CONFIG|CFG_OPT_USER, NULL },
  { "ScoringHideScore",       flt_scoring_hide,   CFG_OPT_CONFIG|CFG_OPT_USER, NULL },
  { "ScoringMaxValue",        flt_scoring_vals,   CFG_OPT_CONFIG|CFG_OPT_USER, NULL },
  { "ScoringMinValue",        flt_scoring_vals,   CFG_OPT_CONFIG|CFG_OPT_USER, NULL },
  { "ScoringMaxColor",        flt_scoring_cols,   CFG_OPT_CONFIG|CFG_OPT_USER, NULL },
  { "ScoringNormalColor",     flt_scoring_cols,   CFG_OPT_CONFIG|CFG_OPT_USER, NULL },
  { "ScoringMinColor",        flt_scoring_cols,   CFG_OPT_CONFIG|CFG_OPT_USER, NULL },
  { "ScoringIgnoreNoneMatch", flt_scoring_ignore, CFG_OPT_CONFIG|CFG_OPT_USER, NULL },
  { NULL, NULL, 0, NULL }
};

t_handler_config flt_scoring_handlers[] = {
  { VIEW_LIST_HANDLER,    flt_scoring_execute },
  { 0, NULL }
};

t_module_config flt_scoring = {
  MODULE_MAGIC_COOKIE,
  flt_scoring_config,
  flt_scoring_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  flt_scoring_finish
};


/* eof */
