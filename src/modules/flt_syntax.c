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

#include <pcre.h>

#include <errno.h>

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
  pcre *re;
  pcre_extra *extra;
} flt_syntax_preg_t;

typedef struct {
  int type;

  u_char **args;
  size_t len;

  t_array pregs;
} flt_syntax_statement_t;

typedef struct {
  u_char *name;
  int le_behavior;
  t_array statement;
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

#define FLT_SYNTAX_STAY         0xF
#define FLT_SYNTAX_POP          0x10
#define FLT_SYNTAX_ONSTRING     0x11
#define FLT_SYNTAX_ONSTRINGLIST 0x12

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
            str_char_append(str,*(ptr+1));
        }
        ++ptr;
        break;

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
  u_char *ptr;

  for(ptr=(u_char *)(*pos?*pos:line);*ptr && isspace(*ptr);++ptr);
  if(!*ptr) return FLT_SYNTAX_EOF;

  switch(*ptr) {
    case '#':
      return FLT_SYNTAX_EOF;
    case '"':
      flt_syntax_read_string(ptr+1,&ptr,str);
      *pos = ptr+1;
      return FLT_SYNTAX_TOK_STR;

    case '=':
      *pos = ptr+1;
      return FLT_SYNTAX_TOK_EQ;

    default:
      for(;*ptr && !isspace(*ptr);++ptr) str_char_append(str,*ptr);
      *pos = ptr;
      return FLT_SYNTAX_TOK_KEY;
  }

  return FLT_SYNTAX_EOF;
}
/* }}} */

/* {{{ flt_syntax_read_list */
int flt_syntax_read_list(const u_char *pos,flt_syntax_list_t *list) {
  register u_char *ptr;
  t_string str;

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
        ++ptr;
        continue;

      case ',':
        list->values = fo_alloc(list->values,++list->len,sizeof(u_char **),FO_ALLOC_REALLOC);
        list->values[list->len-1] = str.content;
        str_init(&str);

      default:
        str_char_append(&str,*ptr);
    }
  }

  list->values = fo_alloc(list->values,++list->len,sizeof(u_char **),FO_ALLOC_REALLOC);
  list->values[list->len-1] = str.content;

  return 0;
}
/* }}} */

/* {{{ flt_syntax_load */
int flt_syntax_load(const u_char *path,const u_char *lang) {
  FILE *fd;
  ssize_t len;
  u_char *line = NULL,*tmp,*pos,*error;
  size_t buflen = 0;
  int state = FLT_SYNTAX_GLOBAL,offset,type,tb,lineno = 0;
  t_string str;
  flt_syntax_preg_t preg;

  flt_syntax_pattern_file_t file;
  flt_syntax_list_t list;
  flt_syntax_block_t block;
  flt_syntax_statement_t statement;

  if((fd = fopen(path,"r")) == NULL) {
    fprintf(stderr,"I/O error opening file %s: %s\n",path,strerror(errno));
    return 1;
  }

  file.start = NULL;
  array_init(&file.lists,sizeof(flt_syntax_list_t),NULL);
  array_init(&file.blocks,sizeof(flt_syntax_block_t),NULL);

  while((len = getline((char **)&line,&buflen,fd)) > 0) {
    lineno++;
    str_init(&str);

    pos  = NULL;
    type = flt_syntax_next_token(line,&pos,&str);

    if(state == FLT_SYNTAX_GLOBAL) {
      if(type != FLT_SYNTAX_TOK_KEY) {
        if(type == FLT_SYNTAX_EOF) continue;

        fprintf(stderr,"unknown token at line %d!\n",lineno);
        return 1;
      }
      /* {{{ start = "key" */
      if(cf_strcmp(str.content,"start") == 0) {
        str_cleanup(&str);

        if((type = flt_syntax_next_token(line,&pos,&str)) != FLT_SYNTAX_TOK_EQ) {
          fprintf(stderr,"after start we got no eqal at line %d!\n",lineno);
          return 1;
        }
        if((type = flt_syntax_next_token(line,&pos,&str)) != FLT_SYNTAX_TOK_STR) {
          fprintf(stderr,"after start we got no string at line %d!\n",lineno);
          return 1;
        }

        file.start = str.content;
        str_init(&str);
      }
      /* }}} */
      /* {{{ list "name" = "value,value" */
      else if(cf_strcmp(str.content,"list") == 0) {
        memset(&list,0,sizeof(list));

        str_cleanup(&str);
        if((type = flt_syntax_next_token(line,&pos,&str)) != FLT_SYNTAX_TOK_STR) {
          fprintf(stderr,"after list we got no list name at line %d!\n",lineno);
          return 1;
        }

        list.name = str.content;
        str_init(&str);

        if((type = flt_syntax_next_token(line,&pos,&str)) != FLT_SYNTAX_TOK_EQ) {
          fprintf(stderr,"after list we got no eqal at line %d!\n",lineno);
          return 1;
        }
        if(flt_syntax_read_list(pos,&list) != 0) {
          fprintf(stderr,"syntax error in list %s at line %d!\n",list.name,lineno);
          return 1;
        }

        /* we sort them to be able to search faster in it */
        qsort(list.values,list.len,sizeof(*list.values),cf_strcmp);
        array_push(&file.lists,&list);
      }
      /* }}} */
      /* {{{ block "name" */
      else if(cf_strcmp(str.content,"block") == 0) {
        array_init(&block.statement,sizeof(flt_syntax_statement_t),NULL);
        str_cleanup(&str);

        if((type = flt_syntax_next_token(line,&pos,&str)) != FLT_SYNTAX_TOK_STR) {
          fprintf(stderr,"after block we got no block name at line %d!\n",lineno);
          return 1;
        }

        block.name = str.content;
        str_init(&str);

        state = FLT_SYNTAX_BLOCK;
      }
      /* }}} */
      else {
        fprintf(stderr,"unknown token in global state at line %d!\n",lineno);
        return 1;
      }
    }
    else {
      if(type != FLT_SYNTAX_TOK_KEY) {
        if(type == FLT_SYNTAX_EOF) continue;
        fprintf(stderr,"unknown token at line %d!\n",lineno);
        return 1;
      }
      memset(&statement,0,sizeof(statement));

      if(cf_strcmp(str.content,"lineend") == 0) {
        str_cleanup(&str);

        if((type = flt_syntax_next_token(line,&pos,&str)) != FLT_SYNTAX_TOK_KEY) {
          fprintf(stderr,"after lineend we got no keyword at line %d!\n",lineno);
          return 1;
        }
        block.le_behavior = cf_strcmp(str.content,"stay") == 0 ? FLT_SYNTAX_STAY : FLT_SYNTAX_POP;
      }
      else if(cf_strcmp(str.content,"end") == 0) {
        str_cleanup(&str);
        state = FLT_SYNTAX_GLOBAL;
      }

      /* {{{ onstring <zeichenkette> <neuer-block> <span-klasse> */
      else if(cf_strcmp(str.content,"onstring") == 0) {
        str_cleanup(&str);

        if((type = flt_syntax_next_token(line,&pos,&str)) != FLT_SYNTAX_TOK_STR) {
          fprintf(stderr,"after onstring we got no string at line %d!\n",lineno);
          return 1;
        }

        statement.type = FLT_SYNTAX_ONSTRING;
        statement.args = fo_alloc(NULL,3,sizeof(u_char **),FO_ALLOC_MALLOC);

        statement.args[0] = str.content;
        str_init(&str);

        type = flt_syntax_next_token(line,&pos,&str);
        if(type == FLT_SYNTAX_TOK_KEY) {
          statement.args[1] = str.content;
          statement.len = 2;
          tb = type;
        }
        else if(type != FLT_SYNTAX_TOK_STR) {
          fprintf(stderr,"after onstring we got no block name at line %d!\n",lineno);
          return 1;
        }
        else {
          statement.args[1] = str.content;
          str_init(&str);
        }

        if((type = flt_syntax_next_token(line,&pos,&str)) != FLT_SYNTAX_TOK_STR) {
          if(tb != FLT_SYNTAX_TOK_KEY || cf_strcmp(statement.args[1],"highlight") == 0) {
            fprintf(stderr,"after onstring we got no span class name at line %d!\n",lineno);
            return 1;
          }
        }
        else statement.args[2] = str.content;

        array_push(&block.statement,&statement);
      }
      /* }}} */
      /* {{{ onstringlist <listen-name> <neuer-block> <span-klasse> */
      else if(cf_strcmp(str.content,"onstringlist") == 0) {
        str_cleanup(&str);

        statement.type = FLT_SYNTAX_ONSTRINGLIST;
        statement.args = fo_alloc(NULL,3,sizeof(u_char **),FO_ALLOC_MALLOC);
        statement.len  = 3;

        if((type = flt_syntax_next_token(line,&pos,&str)) != FLT_SYNTAX_TOK_STR) {
          fprintf(stderr,"after onstringlist we got no list name at line %d!\n",lineno);
          return 1;
        }
        statement.args[0] = str.content;
        str_init(&str);

        type = flt_syntax_next_token(line,&pos,&str);
        if(type == FLT_SYNTAX_TOK_KEY) {
          statement.args[1] = str.content;
          statement.len = 2;
          tb = type;
        }
        else if(type != FLT_SYNTAX_TOK_STR) {
          fprintf(stderr,"after onstringlist we got no block name at line %d!\n",lineno);
          return 1;
        }
        else {
          statement.args[1] = str.content;
          str_init(&str);
        }

        if((type = flt_syntax_next_token(line,&pos,&str)) != FLT_SYNTAX_TOK_STR) {
          if(tb != FLT_SYNTAX_TOK_KEY || cf_strcmp(statement.args[1],"highlight") == 0) {
            fprintf(stderr,"after onstringlist we got no span class name at line %d!\n",lineno);
            return 1;
          }
        }
        else {
          statement.args[2] = str.content;
          str_init(&str);
        }

        array_push(&block.statement,&statement);
      }
      /* }}} */
      /* {{{ onregexp <regexp> <neuer-block> <span-klasse> */
      else if(cf_strcmp(str.content,"onregexp") == 0) {
        str_cleanup(&str);

        if((type = flt_syntax_next_token(line,&pos,&str)) != FLT_SYNTAX_TOK_STR) {
          fprintf(stderr,"after onregexp we got no regex at line %d!\n",lineno);
          return 1;
        }

        if((preg.re = pcre_compile((const char *)str.content,0,(const char **)&error,&offset,NULL)) == NULL) {
          fprintf(stderr,"regex error in pattern '%s': %s (Offset %d) at line %d\n",str.content,error,offset,lineno);
          return 1;
        }
        if((preg.extra = pcre_study(preg.re,0,(const char **)&error)) == NULL) {
          if(error) {
            fprintf(stderr,"regex error in pattern '%s': %s at line %d\n",str.content,error,lineno);
            return 1;
          }
        }

        str_cleanup(&str);
        array_init(&statement.pregs,sizeof(preg),NULL);
        array_push(&statement.pregs,&preg);

        statement.args = fo_alloc(NULL,2,sizeof(u_char **),FO_ALLOC_MALLOC);
        statement.len = 2;

        type = flt_syntax_next_token(line,&pos,&str);
        if(type == FLT_SYNTAX_TOK_KEY) {
          statement.args[0] = str.content;
          statement.len = 2;
          tb = type;
        }
        else if(type != FLT_SYNTAX_TOK_STR) {
          fprintf(stderr,"after onregexp we got no block name at line %d!\n",lineno);
          return 1;
        }
        else {
          statement.args[0] = str.content;
          str_init(&str);
        }

        if((type = flt_syntax_next_token(line,&pos,&str)) != FLT_SYNTAX_TOK_STR) {
          if(tb != FLT_SYNTAX_TOK_KEY || cf_strcmp(statement.args[1],"highlight") == 0) {
            fprintf(stderr,"after onregexp we got no span class name at line %d!\n",lineno);
            return 1;
          }
        }
        else {
          statement.args[1] = str.content;
          str_init(&str);
        }
      }
      /* }}} */
      /* {{{ onregexp_backref <pattern> <block> <backreference number> <span-klasse> */
      else if(cf_strcmp(str.content,"onregexp_backref") == 0) {
        str_cleanup(&str);

        if((type = flt_syntax_next_token(line,&pos,&str)) != FLT_SYNTAX_TOK_STR) {
          fprintf(stderr,"after onregex_backref we got no regex at line %d!\n",lineno);
          return 1;
        }

        if((preg.re = pcre_compile((const char *)str.content,0,(const char **)&error,&offset,NULL)) == NULL) {
          fprintf(stderr,"error in patter '%s': %s (Offset %d) at line %d\n",str.content,error,offset,lineno);
          return 1;
        }
        if((preg.extra = pcre_study(preg.re,0,(const char **)&error)) == NULL) {
          if(error) {
            fprintf(stderr,"error in pattern '%s': %s at line %d\n",str.content,error,lineno);
            return 1;
          }
        }

        str_cleanup(&str);
        array_init(&statement.pregs,sizeof(preg),NULL);
        array_push(&statement.pregs,&preg);

        statement.args = fo_alloc(NULL,3,sizeof(u_char **),FO_ALLOC_MALLOC);
        statement.len = 3;

        if((type = flt_syntax_next_token(line,&pos,&str)) != FLT_SYNTAX_TOK_STR) {
          fprintf(stderr,"after onregexp_backref we got no block name at line %d!\n",lineno);
          return 1;
        }
        statement.args[0] = str.content;
        str_init(&str);

        if((type = flt_syntax_next_token(line,&pos,&str)) != FLT_SYNTAX_TOK_KEY) {
          fprintf(stderr,"after onregexp_after we got no reference number at line %d!\n",lineno);
          return 1;
        }
        statement.args[1] = str.content;
        str_init(&str);

        if((type = flt_syntax_next_token(line,&pos,&str)) != FLT_SYNTAX_TOK_STR) {
          fprintf(stderr,"after onregexp_after we got no span class name at line %d!\n",lineno);
          return 1;
        }
        statement.args[2] = str.content;
        str_init(&str);
      }
      /* }}} */
      /* {{{ syntax onregexpafter <vorher-regexp> <regexp-zu-matchen> <neuer-block> <span-klasse> */
      else if(cf_strcmp(str.content,"onregexpafter") == 0) {
        str_cleanup(&str);

        if((type = flt_syntax_next_token(line,&pos,&str)) != FLT_SYNTAX_TOK_STR) {
          fprintf(stderr,"after onregexpafter we got no regex at line %d!\n",lineno);
          return 1;
        }
        if((preg.re = pcre_compile((const char *)str.content,0,(const char **)&error,&offset,NULL)) == NULL) {
          fprintf(stderr,"error in pattern '%s': %s (offset %d) at line %d\n",str.content,error,offset,lineno);
          return 1;
        }
        if((preg.extra = pcre_study(preg.re,0,(const char **)&error)) == NULL) {
          if(error) {
            fprintf(stderr,"error in pattern '%s': %s at line %d\n",str.content,error,lineno);
            return 1;
          }
        }

        str_cleanup(&str);
        array_init(&statement.pregs,sizeof(preg),NULL);
        array_push(&statement.pregs,&preg);

        if((type = flt_syntax_next_token(line,&pos,&str)) != FLT_SYNTAX_TOK_STR) {
          fprintf(stderr,"after onregexpafter we got no regex at line %d!\n",lineno);
          return 1;
        }
        if((preg.re = pcre_compile((const char *)str.content,0,(const char **)&error,&offset,NULL)) == NULL) {
          fprintf(stderr,"error in pattern '%s': %s (offset %d) at line %d\n",str.content,error,offset,lineno);
          return 1;
        }
        if((preg.extra = pcre_study(preg.re,0,(const char **)&error)) == NULL) {
          if(error) {
            fprintf(stderr,"error in pattern '%s': %s at line %d\n",str.content,error,lineno);
            return 1;
          }
        }

        str_cleanup(&str);
        array_push(&statement.pregs,&preg);

        statement.args = fo_alloc(NULL,2,sizeof(u_char **),FO_ALLOC_MALLOC);
        statement.len = 2;

        type = flt_syntax_next_token(line,&pos,&str);
        if(type == FLT_SYNTAX_TOK_KEY) {
          statement.args[0] = str.content;
          statement.len = 2;
          tb = type;
        }
        else if(type != FLT_SYNTAX_TOK_STR) {
          fprintf(stderr,"after onregexpafter we got no block name at line %d!\n",lineno);
          return 1;
        }
        else {
          statement.args[0] = str.content;
          str_init(&str);
        }

        if((type = flt_syntax_next_token(line,&pos,&str)) != FLT_SYNTAX_TOK_STR) {
          if(tb != FLT_SYNTAX_TOK_KEY || cf_strcmp(statement.args[1],"highlight") == 0) {
            fprintf(stderr,"after onregexpafter we got no span class name at line %d!\n",lineno);
            return 1;
          }
        }
        else {
          statement.args[1] = str.content;
          str_init(&str);
        }
      }
      /* }}} */
      else {
        fprintf(stderr,"unknown token in block state at line %d!\n",lineno);
        return 1;
      }
    }

  }

  fclose(fd);

  return 0;
}
/* }}} */


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

  array_init(&flt_syntax_files,sizeof(flt_syntax_pattern_file_t),NULL);

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
