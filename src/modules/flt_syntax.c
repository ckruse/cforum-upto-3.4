/**
 * \file flt_syntax.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 *
 * This plugin implements a syntax highlighter
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
#include <time.h>

#include <string.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "readline.h"
#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "cfcgi.h"
#include "template.h"
#include "clientlib.h"
#include "charconvert.h"
#include "validate.h"
#include "htmllib.h"
/* }}} */

static u_char *flt_syntax_patterns_dir = NULL;

typedef struct {
  u_char *name;

  u_char **values;
  size_t len;
} flt_syntax_list_t;

typedef struct {
  int type;

  u_char **args;
  size_t len;
} flt_syntax_statement_t;

typedef struct {
  u_char *name;
  t_array statements;
} flt_syntax_block_t;

typedef struct {
  u_char *start;

  t_array lists;
  t_array blocks;
} flt_syntax_pattern_file_t;

static t_array flt_syntax_files;

#define FLT_SYNTAX_GLOBAL 0
#define FLT_SYNTAX_BLOCK  1

#define FLT_SYNTAX_EOF -1
#define FLT_SYNTAX_FAILURE -2

#define FLT_SYNTAX_TOK_STR 1
#define FLT_SYNTAX_TOK_KEY 2
#define FLT_SYNTAX_TOK_EQ  3

int flt_syntax_highlight(t_string *content,t_string *bco,const u_char *lang) {
  return 1;
}

/* {{{ flt_syntax_read_string */
int flt_syntax_read_string(const u_char *begin,u_char **pos,t_string *str) {
  register u_char *ptr;

  for(ptr=(u_char *)begin;*ptr;++ptr) {
    switch(*ptr) {
      case '\\':
        switch(*(ptr+1)) {
          case 'n':
            str_char_append(str,'\n');
            break;
          case 't':
            str_char_append(str,'\t');
            break;
          default:
            str_char_append(str,*ptr);
        }

      case '"':
        if(pos) *pos = ptr;
        return 0;

      default:
        str_char_append(str,*ptr);
    }
  }

  return FLT_SYNTAX_FAILURE;
}
/* }}} */

/* {{{ flt_syntax_next_token */
int flt_syntax_next_token(const u_char *line,u_char **pos,t_string *str) {
  register u_char *ptr;

  for(ptr=*pos?*pos:line;*ptr && isspace(*ptr);++ptr);

  switch(*ptr) {
    case '#':
      return FLT_SYNTAX_EOF;
    case '"':
      flt_syntax_read_string(ptr+1,&ptr,str);
      *pos = ptr+1;
      return FLT_SYNTAX_TOK_STR;

    case '=':
      return FLT_SYNTAX_TOK_EQ;

    default:
      for(;*ptr && !isspace(*ptr);++ptr) str_char_append(str,*ptr);
      return FLT_SYNTAX_TOK_KEY;
  }

  return FLT_SYNTAX_EOF;
}
/* }}} */

/* {{{ flt_syntax_read_list */
int flt_syntax_read_list(const u_char *pos,flt_syntax_list_t *list) {
  register u_char *ptr;
  st_string str;

  str_init(&str);

  for(ptr=(u_char *)pos;*ptr && *ptr != '"';++ptr) {
    switch(*ptr) {
      case '\\':
        switch(*(ptr+1)) {
          case 'n':
            str_char_append(&str,'\n');
            break;
          case 't':
            str_char_append(&str,'\t');
            break;
          default:
            str_char_append(&str,*ptr);
        }

      case ',':
        list->values = realloc(list->values,++list->len * sizeof(u_char **));
        list->values[list->len-1] = str.content;
        str_init(&str);

      default:
        str_char_append(str,*ptr);
    }
  }

  list->values = realloc(list->values,++list->len * sizeof(u_char **));
  list->values[list->len-1] = str.content;

  return 0;
}
/* }}} */

int flt_syntax_load(const u_char *path,const u_char *lang) {
  FILE *fd;
  ssize_t len;
  u_char *line = NULL,*tmp,*pos;
  size_t buflen = 0;
  int state = FLT_SYNTAX_GLOBAL;
  t_string str;

  flt_syntax_pattern_file file;
  flt_syntax_list_t list;
  flt_syntax_block_t block;
  flt_syntax_statement_t statement;

  if((fd = fopen(path,"r")) == NULL) return 1;

  file.start = NULL;
  array_init(&file.lists,sizeof(flt_syntax_list_t),NULL);
  array_init(&file.blocks,sizeof(flt_syntax_block_t),NULL);

  while((len = getline(&line,&buflen,fd)) > 0) {
    str_init(&str);

    pos  = NULL;
    type = flt_syntax_next_token(line,&pos,&str);

    if(state == FLT_SYNTAX_GLOBAL) {
      if(type != FLT_SYNTAX_TOK_KEY) return 1;
      if(cf_strcmp(str.content,"start") == 0) {
        str_cleanup(&str);

        type = flt_syntax_next_token(line,&pos,&str);
        if(type != FLT_SYNTAX_TOK_EQ) return 1;

        type = flt_syntax_next_token(line,&pos,&str);
        if(type != FLT_SYNTAX_TOK_STR) return 1;

        file.start = str.content;
        str_init(&str);
      }
      else if(cf_strcmp(str.content,"list") == 0) {
        memset(&list,0,sizeof(list));

        str_cleanup(&str);
        type = flt_syntax_next_token(line,&pos,&str);
        if(type != FLT_SYNTAX_TOK_STR) return 1;

        list.name = str.content;
        str_init(&str);

        type = flt_syntax_next_token(line,&pos,&str);
        if(type != FLT_SYNTAX_TOK_EQ) return 1;

        if(flt_syntax_read_list(pos,&list) != 0) return 1;

        array_push(&file.lists,&list);
      }
      else if(cf_strcmp(str.content,"block") == 0) {
        array_init(&block.statements,sizeof(flt_syntax_statement_t),NULL);
        str_cleanup(&str);

        type = flt_syntax_next_token(line,&pos,&str);
        if(type != FLT_SYNTAX_TOK_STR) return 1;

        block.name = str.content;
        str_init(&str);

        state = FLT_SYNTAX_BLOCK;
      }
      else return 1;
    }
    else {
      /* dumdidum */
    }

  }

  fclose(fd);

  return 1;
}


int flt_syntax_execute(t_configuration *fdc,t_configuration *fvc,const u_char *directive,const u_char **parameters,size_t plen,t_string *bco,t_string *bci,t_string *content,t_string *cite,const u_char *qchars,int sig) {
  t_string str;
  struct stat st;

  /* {{{ we don't know what language we got, so just put a <code> around it */
  if(plen != 2 || cf_strcmp(parameters[0],"lang") != 0) {
    str_chars_append(bco,"<code>",6);
    str_str_append(bco,content);
    str_chars_append(bco,"</code>",7);

    if(bci) {
      str_chars_append(bci,"[code]",6);
      str_str_append(bci,cite);
      str_chars_append(bci,"[/code]",7);
    }

    return FLT_OK;
  }
  /* }}} */

  /* we got a language, check if it exists */
  str_init(&str);
  str_char_set(&str,flt_syntax_patterns_dir,strlen(flt_syntax_patterns_dir));
  str_char_append(&str,'/');
  str_chars_append(&str,parameters[1],strlen(parameters[1]));
  str_chars_append(&str,".pat",4);

  /* {{{ language doesnt exist, put a <code> around it */
  if(stat(str.content,&st) == -1) {
    str_chars_append(bco,"<code>",6);
    str_str_append(bco,content);
    str_chars_append(bco,"</code>",7);

    if(bci) {
      str_chars_append(bci,"[code lang=",11);
      str_chars_append(bci,parameters[1],strlen(parameters[1]));
      str_char_append(bci,']');
      str_str_append(bci,cite);
      str_chars_append(bci,"[/code]",7);
    }

    return FLT_OK;
  }
  /* }}} */

  /* {{{ load syntax pattern file */
  if(flt_syntax_load(str.content,parameters[1]) != 0) {
    str_chars_append(bco,"<code>",6);
    str_str_append(bco,content);
    str_chars_append(bco,"</code>",7);

    if(bci) {
      str_chars_append(bci,"[code lang=",11);
      str_chars_append(bci,parameters[1],strlen(parameters[1]));
      str_char_append(bci,']');
      str_str_append(bci,cite);
      str_chars_append(bci,"[/code]",7);
    }

    return FLT_OK;
  }
  /* }}} */

  /* {{{ highlight content */
  if(flt_syntax_highlight(content,bco,parameters[1]) != 0) {
    str_chars_append(bco,"<code>",6);
    str_str_append(bco,content);
    str_chars_append(bco,"</code>",7);

    if(bci) {
      str_chars_append(bci,"[code lang=",11);
      str_chars_append(bci,parameters[1],strlen(parameters[1]));
      str_char_append(bci,']');
      str_str_append(bci,cite);
      str_chars_append(bci,"[/code]",7);
    }

    return FLT_OK;
  }
  /* }}} */

  if(bci) {
    str_chars_append(bci,"[code lang=",11);
    str_chars_append(bci,parameters[1],strlen(parameters[1]));
    str_char_append(bci,']');
    str_str_append(bci,cite);
    str_chars_append(bci,"[/code]",7);
  }

  return FLT_DECLINE;
}

int flt_syntax_init(t_cf_hash *cgi,t_configuration *dc,t_configuration *vc) {
  cf_html_register_directive("code",flt_syntax_execute,CF_HTML_DIR_TYPE_ARG|CF_HTML_DIR_TYPE_BLOCK);

  array_init(&flt_syntax_files,sizeof(flt_syntax_pattern_file),NULL);

  return FLT_DECLINE;
}


/* {{{ flt_syntax_handle */
int flt_syntax_handle(t_configfile *cfile,t_conf_opt *opt,const u_char *context,u_char **args,size_t argnum) {
  if(flt_syntax_patterns_dir != NULL) free(flt_syntax_patterns_dir);
  flt_syntax_patterns_dir = strdup(args[0]);

  return 0;
}
/* }}} */

void flt_syntax_cleanup(void) {
  if(flt_syntax_patterns_dir != NULL) free(flt_syntax_patterns_dir);
}

t_conf_opt flt_syntax_config[] = {
  { "PatternsDirectory", flt_syntax_handle, CFG_OPT_CONFIG|CFG_OPT_GLOBAL|CFG_OPT_NEEDED,  NULL },
  { NULL, NULL, 0, NULL }
};

t_handler_config flt_syntax_handlers[] = {
  { INIT_HANDLER, flt_syntax_init },
  { 0, NULL }
};

t_module_config flt_syntax = {
  flt_syntax_config,
  flt_syntax_handlers,
  NULL,
  NULL,
  NULL,
  flt_syntax_cleanup
};


/* eof */
