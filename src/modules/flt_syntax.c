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

  t_array list;
} flt_syntax_list_t;

typedef struct {
  pcre *re;
  pcre_extra *extra;
} flt_syntax_preg_t;

typedef struct {
  int type;
  int start;

  t_array args;
  t_array pregs;
} flt_syntax_statement_t;

typedef struct {
  u_char *name;
  int le_behavior;
  t_array statement;
} flt_syntax_block_t;

typedef struct {
  u_char *start;
  u_char *lang;

  t_array lists;
  t_array blocks;
} flt_syntax_pattern_file_t;

static t_array flt_syntax_files;
static int flt_syntax_active = 0;

#define FLT_SYNTAX_GLOBAL 0
#define FLT_SYNTAX_BLOCK  1

#define FLT_SYNTAX_EOF -1
#define FLT_SYNTAX_FAILURE -2

#define FLT_SYNTAX_TOK_STR 1
#define FLT_SYNTAX_TOK_KEY 2
#define FLT_SYNTAX_TOK_EQ  3

#define FLT_SYNTAX_STAY                   0xF
#define FLT_SYNTAX_POP                    0x10
#define FLT_SYNTAX_ONSTRING               0x11
#define FLT_SYNTAX_ONSTRINGLIST           0x12
#define FLT_SYNTAX_ONREGEXP               0x13
#define FLT_SYNTAX_ONREGEXP_BACKREF       0x14
#define FLT_SYNTAX_ONREGEXP_AFTER         0x15
#define FLT_SYNTAX_ONREGEXP_AFTER_BACKREF 0x16

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

  array_init(&list->list,sizeof(str),NULL);
  str_init(&str);

  for(ptr=(u_char *)pos;isspace(*ptr);++ptr);
  if(*ptr != '"') return 1;

  for(++ptr;*ptr && *ptr != '"';++ptr) {
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
            str_char_append(&str,*(ptr+1));
        }
        ++ptr;
        continue;

      case ',':
        if(!str.content) {
          fprintf(stderr,"Error reading list, str.content is NULL! ptr: %s\n",ptr);
          return -1;
        }
        array_push(&list->list,&str);
        str_init(&str);
        break;

      default:
        str_char_append(&str,*ptr);
    }
  }

  if(!str.content) {
    fprintf(stderr,"Error reading list, str.content is NULL at end of list!\n");
    return -1;
  }
  array_push(&list->list,&str);

  return 0;
}
/* }}} */

int flt_syntax_my_cmp(const t_string *a,const t_string *b) {
  return strcasecmp(a->content,b->content);
}

int flt_syntax_my_scmp(const u_char *a,const t_string *b) {
  return strcasecmp(a,b->content);
}

/* {{{ flt_syntax_load */
int flt_syntax_load(const u_char *path,const u_char *lang) {
  FILE *fd;
  ssize_t len;
  u_char *line = NULL,*pos,*error;
  size_t buflen = 0;
  int state = FLT_SYNTAX_GLOBAL,offset,type,tb = 0,lineno = 0;
  t_string str,*tmpstr;
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
  file.lang  = strdup(lang);
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
        array_sort(&list.list,flt_syntax_my_cmp);
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
        array_push(&file.blocks,&block);
      }

      /* {{{ onstring <zeichenkette> <neuer-block> <span-klasse> */
      else if(cf_strcmp(str.content,"onstring") == 0 || cf_strcmp(str.content,"onstring_start") == 0) {
        if(cf_strcmp(str.content,"onstring_start") == 0) statement.start = 1;
        else statement.start = 0;

        str_cleanup(&str);

        if((type = flt_syntax_next_token(line,&pos,&str)) != FLT_SYNTAX_TOK_STR) {
          fprintf(stderr,"after onstring we got no string at line %d!\n",lineno);
          return 1;
        }

        statement.type = FLT_SYNTAX_ONSTRING;
        array_init(&statement.args,sizeof(t_string),NULL);

        array_push(&statement.args,&str);
        str_init(&str);

        type = flt_syntax_next_token(line,&pos,&str);
        if(type == FLT_SYNTAX_TOK_KEY) {
          array_push(&statement.args,&str);
          tb = type;
        }
        else if(type != FLT_SYNTAX_TOK_STR) {
          fprintf(stderr,"after onstring we got no block name at line %d!\n",lineno);
          return 1;
        }
        else array_push(&statement.args,&str);

        str_init(&str);
        if((type = flt_syntax_next_token(line,&pos,&str)) != FLT_SYNTAX_TOK_STR) {
          tmpstr = array_element_at(&statement.args,1);
          if(tb != FLT_SYNTAX_TOK_KEY || cf_strcmp(tmpstr->content,"highlight") == 0) {
            fprintf(stderr,"after onstring we got no span class name at line %d!\n",lineno);
            return 1;
          }
        }

        if(str.len) array_push(&statement.args,&str);
        str_init(&str);
        array_push(&block.statement,&statement);
      }
      /* }}} */
      /* {{{ onstringlist <listen-name> <neuer-block> <span-klasse> */
      else if(cf_strcmp(str.content,"onstringlist") == 0 || cf_strcmp(str.content,"onstringlist_start") == 0) {
        if(cf_strcmp(str.content,"onstringlist_start") == 0) statement.start = 1;
        else statement.start = 0;

        str_cleanup(&str);

        statement.type = FLT_SYNTAX_ONSTRINGLIST;
        array_init(&statement.args,sizeof(t_string),NULL);

        if((type = flt_syntax_next_token(line,&pos,&str)) != FLT_SYNTAX_TOK_STR) {
          fprintf(stderr,"after onstringlist we got no list name at line %d!\n",lineno);
          return 1;
        }

        array_push(&statement.args,&str);
        str_init(&str);

        type = flt_syntax_next_token(line,&pos,&str);
        if(type == FLT_SYNTAX_TOK_KEY) {
          array_push(&statement.args,&str);
          tb = type;
        }
        else if(type != FLT_SYNTAX_TOK_STR) {
          fprintf(stderr,"after onstringlist we got no block name at line %d!\n",lineno);
          return 1;
        }
        else array_push(&statement.args,&str);

        str_init(&str);
        if((type = flt_syntax_next_token(line,&pos,&str)) != FLT_SYNTAX_TOK_STR) {
          tmpstr = array_element_at(&statement.args,1);
          if(tb != FLT_SYNTAX_TOK_KEY || cf_strcmp(tmpstr->content,"highlight") == 0) {
            fprintf(stderr,"after onstringlist we got no span class name at line %d!\n",lineno);
            return 1;
          }
        }

        if(str.len) array_push(&statement.args,&str);
        str_init(&str);
        array_push(&block.statement,&statement);
      }
      /* }}} */
      /* {{{ onregexp <regexp> <neuer-block> <span-klasse> */
      else if(cf_strcmp(str.content,"onregexp") == 0 || cf_strcmp(str.content,"onregexp_start") == 0) {
        if(cf_strcmp(str.content,"onregexp_start") == 0) statement.start = 1;
        else statement.start = 0;

        str_cleanup(&str);
        statement.type = FLT_SYNTAX_ONREGEXP;

        if((type = flt_syntax_next_token(line,&pos,&str)) != FLT_SYNTAX_TOK_STR) {
          fprintf(stderr,"after onregexp we got no regex at line %d!\n",lineno);
          return 1;
        }

        if((preg.re = pcre_compile((const char *)str.content,PCRE_CASELESS,(const char **)&error,&offset,NULL)) == NULL) {
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

        array_init(&statement.args,sizeof(t_string),NULL);

        type = flt_syntax_next_token(line,&pos,&str);
        if(type == FLT_SYNTAX_TOK_KEY) {
          array_push(&statement.args,&str);
          tb = type;
        }
        else if(type != FLT_SYNTAX_TOK_STR) {
          fprintf(stderr,"after onregexp we got no block name at line %d!\n",lineno);
          return 1;
        }
        else array_push(&statement.args,&str);

        str_init(&str);
        if((type = flt_syntax_next_token(line,&pos,&str)) != FLT_SYNTAX_TOK_STR) {
          tmpstr = array_element_at(&statement.args,0);
          if(tb != FLT_SYNTAX_TOK_KEY || cf_strcmp(tmpstr->content,"highlight") == 0) {
            fprintf(stderr,"after onregexp we got no span class name at line %d!\n",lineno);
            return 1;
          }
        }

        if(str.len) array_push(&statement.args,&str);

        str_init(&str);
        array_push(&block.statement,&statement);
      }
      /* }}} */
      /* {{{ onregexp_backref <pattern> <block> <backreference number> <span-klasse> */
      else if(cf_strcmp(str.content,"onregexp_backref") == 0 || cf_strcmp(str.content,"onregexp_backref_start") == 0) {
        if(cf_strcmp(str.content,"onregexp_backref_start") == 0) statement.start = 1;
        else statement.start = 0;

        str_cleanup(&str);
        statement.type = FLT_SYNTAX_ONREGEXP_BACKREF;

        if((type = flt_syntax_next_token(line,&pos,&str)) != FLT_SYNTAX_TOK_STR) {
          fprintf(stderr,"after onregex_backref we got no regex at line %d!\n",lineno);
          return 1;
        }

        if((preg.re = pcre_compile((const char *)str.content,PCRE_CASELESS,(const char **)&error,&offset,NULL)) == NULL) {
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

        array_init(&statement.args,sizeof(t_string),NULL);

        if((type = flt_syntax_next_token(line,&pos,&str)) != FLT_SYNTAX_TOK_STR) {
          fprintf(stderr,"after onregexp_backref we got no block name at line %d!\n",lineno);
          return 1;
        }
        array_push(&statement.args,&str),
        str_init(&str);

        if((type = flt_syntax_next_token(line,&pos,&str)) != FLT_SYNTAX_TOK_KEY) {
          fprintf(stderr,"after onregexp_after we got no reference number at line %d!\n",lineno);
          return 1;
        }
        array_push(&statement.args,&str);
        str_init(&str);

        if((type = flt_syntax_next_token(line,&pos,&str)) != FLT_SYNTAX_TOK_STR) {
          fprintf(stderr,"after onregexp_after we got no span class name at line %d!\n",lineno);
          return 1;
        }
        array_push(&statement.args,&str);
        str_init(&str);

        array_push(&block.statement,&statement);
      }
      /* }}} */
      /* {{{ onregexpafter <vorher-regexp> <regexp-zu-matchen> <neuer-block> <span-klasse> */
      else if(cf_strcmp(str.content,"onregexpafter") == 0 || cf_strcmp(str.content,"onregexpafter_start") == 0) {
        if(cf_strcmp(str.content,"onregexpafter_start") == 0) statement.start = 1;
        else statement.start = 0;

        str_cleanup(&str);
        statement.type = FLT_SYNTAX_ONREGEXP_AFTER;

        if((type = flt_syntax_next_token(line,&pos,&str)) != FLT_SYNTAX_TOK_STR) {
          fprintf(stderr,"after onregexpafter we got no regex at line %d!\n",lineno);
          return 1;
        }
        if((preg.re = pcre_compile((const char *)str.content,PCRE_CASELESS,(const char **)&error,&offset,NULL)) == NULL) {
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
        if((preg.re = pcre_compile((const char *)str.content,PCRE_CASELESS,(const char **)&error,&offset,NULL)) == NULL) {
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

        array_init(&statement.args,sizeof(t_string),NULL);

        type = flt_syntax_next_token(line,&pos,&str);
        if(type == FLT_SYNTAX_TOK_KEY) {
          array_push(&statement.args,&str);
          tb = type;
        }
        else if(type != FLT_SYNTAX_TOK_STR) {
          fprintf(stderr,"after onregexpafter we got no block name at line %d!\n",lineno);
          return 1;
        }
        else array_push(&statement.args,&str);

        str_init(&str);
        if((type = flt_syntax_next_token(line,&pos,&str)) != FLT_SYNTAX_TOK_STR) {
          tmpstr = array_element_at(&statement.args,1);
          if(tb != FLT_SYNTAX_TOK_KEY || cf_strcmp(tmpstr->content,"highlight") == 0) {
            fprintf(stderr,"after onregexpafter we got no span class name at line %d!\n",lineno);
            return 1;
          }
        }

        if(str.len) array_push(&statement.args,&str);
        str_init(&str);
        array_push(&block.statement,&statement);
      }
      /* }}} */
      /* {{{ onregexpafter_backref <vorher-regexp> <regexp-zu-matchen> <neuer-block> <backref-nummer> <span-klasse> */
      else if(cf_strcmp(str.content,"onregexpafter_backref") == 0 || cf_strcmp(str.content,"onregexpafter_backref_start") == 0) {
        if(cf_strcmp(str.content,"onregexpafter_backref_start") == 0) statement.start = 1;
        else statement.start = 0;

        str_cleanup(&str);
        statement.type = FLT_SYNTAX_ONREGEXP_AFTER_BACKREF;

        if((type = flt_syntax_next_token(line,&pos,&str)) != FLT_SYNTAX_TOK_STR) {
          fprintf(stderr,"after onregexpafter_backref we got no regex at line %d!\n",lineno);
          return 1;
        }
        if((preg.re = pcre_compile((const char *)str.content,PCRE_CASELESS,(const char **)&error,&offset,NULL)) == NULL) {
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
          fprintf(stderr,"after onregexpafter_backref we got no regex at line %d!\n",lineno);
          return 1;
        }
        if((preg.re = pcre_compile((const char *)str.content,PCRE_CASELESS,(const char **)&error,&offset,NULL)) == NULL) {
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

        array_init(&statement.args,sizeof(t_string),NULL);

        type = flt_syntax_next_token(line,&pos,&str);
        if(type != FLT_SYNTAX_TOK_STR) {
          fprintf(stderr,"after onregexpafter_backref we got no block name at line %d!\n",lineno);
          return 1;
        }

        array_push(&statement.args,&str);
        str_init(&str);

        
        if((type = flt_syntax_next_token(line,&pos,&str)) != FLT_SYNTAX_TOK_KEY) {
          fprintf(stderr,"after onregexpafter_backref we got no backref number at line %d!\n",lineno);
          return 1;
        }

        array_push(&statement.args,&str);
        str_init(&str);

        if((type = flt_syntax_next_token(line,&pos,&str)) != FLT_SYNTAX_TOK_STR) {
          fprintf(stderr,"after onregexpafter_backref we got no span class name at line %d!\n",lineno);
          return 1;
        }

        array_push(&statement.args,&str);
        str_init(&str);

        array_push(&block.statement,&statement);
      }
      /* }}} */
      else {
        fprintf(stderr,"unknown token in block state at line %d!\n",lineno);
        return 1;
      }
    }

  }

  fclose(fd);

  if(!file.start) {
    fprintf(stderr,"got no default block name!\n");
    return 1;
  }

  array_push(&flt_syntax_files,&file);

  return 0;
}
/* }}} */

/* {{{ flt_syntax_block_by_name */
flt_syntax_block_t *flt_syntax_block_by_name(flt_syntax_pattern_file_t *file,const u_char *name) {
  size_t i;
  flt_syntax_block_t *block;

  for(i=0;i<file->blocks.elements;++i) {
    block = array_element_at(&file->blocks,i);
    if(cf_strcmp(block->name,name) == 0) return block;
  }

  return NULL;
}
/* }}} */

/* {{{ flt_syntax_list_by_name */
flt_syntax_list_t *flt_syntax_list_by_name(flt_syntax_pattern_file_t *file,const u_char *name) {
  size_t i;
  flt_syntax_list_t *list;

  for(i=0;i<file->lists.elements;++i) {
    list = array_element_at(&file->lists,i);
    if(cf_strcmp(list->name,name) == 0) return list;
  }

  return NULL;
}
/* }}} */

/* {{{ flt_syntax_extra_arg */
t_string *flt_syntax_extra_arg(t_string *str,const u_char *extra_arg) {
  t_string *target = fo_alloc(NULL,1,sizeof(*target),FO_ALLOC_CALLOC);
  register u_char *ptr;

  for(ptr=str->content;*ptr;++ptr) {
    if(*ptr == '$' && *(ptr+1) == '$') {
      str_chars_append(target,extra_arg,strlen(extra_arg));
      ++ptr;
      continue;
    }

    str_char_append(target,*ptr);
  }

  return target;
}
/* }}} */

/* {{{ flt_syntax_get_token */
u_char *flt_syntax_get_token(const u_char *txt) {
  register u_char *ptr = NULL;
  u_char *ret = NULL;
  
  if(isalnum(*txt)) {
    for(ptr=(u_char *)txt;*ptr && (isalnum(*ptr) || *ptr == '_' || *ptr == '-');++ptr);
    ret = strndup(txt,ptr-txt);
  }
  else {
    if(!isspace(*txt)) ret = strndup(txt,1);
  }

  return ret;
}
/* }}} */

/* {{{ flt_syntax_check_for_prev */
int flt_syntax_check_for_prev(register u_char *ptr,const u_char *begin) {
  if(!isalpha(*ptr) && *ptr != '-' && *ptr != '_') return 0;

  if(ptr != begin) {
    --ptr;
    if(isalpha(*ptr) || *ptr == '-' || *ptr == '_') return -1;
  }

  return 0;
}
/* }}} */

/* {{{ flt_syntax_doit */
int flt_syntax_doit(flt_syntax_pattern_file_t *file,flt_syntax_block_t *block,u_char *text,size_t len,t_string *cnt,u_char **pos,const u_char *extra_arg,int xhtml,int *pops) {
  u_char *ptr,*tmpchar,*priv_extra,*begin;
  size_t i,x;
  flt_syntax_statement_t *statement;
  t_string *str,*tmpstr;
  int matched;
  flt_syntax_block_t *tmpblock;
  flt_syntax_list_t *tmplist;
  flt_syntax_preg_t *tmppreg;
  int stdvec[42],priv_pops = 0;
  int start = 1;

  if(!block) {
    if((block = flt_syntax_block_by_name(file,file->start)) == NULL) {
      fprintf(stderr,"could not get start block %s (language: %s)!\n",file->start,file->lang);
      return 1;
    }

    if(block->le_behavior != FLT_SYNTAX_STAY) {
      fprintf(stderr,"line end behavior of default block is not stay!\n");
      return 1;
    }

    if(cf_strncmp(text,"<br />",6) == 0) {
      text += 6;
      len  -= 6;
    }
    else if(cf_strncmp(text,"<br>",4) == 0) {
      text += 4;
      len  -= 4;
    }
  }

  begin = *pos ? *pos : text;

  for(ptr=begin;*ptr;++ptr) {
    if(block->le_behavior == FLT_SYNTAX_POP && cf_strncmp(ptr,"<br",3) == 0) {
      str_chars_append(cnt,"<br",3);
      if(xhtml) str_chars_append(cnt," />",3);
      else str_char_append(cnt,'>');
      *pos = ptr + (xhtml ? 6 : 4);
      return 0;
    }
    else if(cf_strncmp(ptr,"<code",5) == 0) {
      for(matched=0;*ptr;++ptr) {
        if(cf_strncmp(ptr,"<code",5) == 0) {
          ++matched;
          str_chars_append(cnt,"<code",5);
          ptr += 4;
        }
        else if(cf_strncmp(ptr,"</code>",7) == 0) {
          --matched;
          str_chars_append(cnt,"</code>",7);
          ptr += 6;
          if(matched == 0) break;
        }
        else str_char_append(cnt,*ptr);
      }

      continue;
    }

    for(i=0,matched=0;i<block->statement.elements && matched == 0;++i) {
      statement = array_element_at(&block->statement,i);

      if(statement->start && start == 0) continue;

      switch(statement->type) {
        case FLT_SYNTAX_ONSTRING:
          /* {{{ onstring */
          tmpstr = array_element_at(&statement->args,0);
          if(extra_arg) tmpstr = flt_syntax_extra_arg(tmpstr,extra_arg);

          if(cf_strncmp(tmpstr->content,ptr,tmpstr->len) == 0) {
            matched = 1;

            str = array_element_at(&statement->args,1);

            if(cf_strcmp(str->content,"highlight") == 0) {
              str = array_element_at(&statement->args,2);

              str_chars_append(cnt,"<span class=\"",13);
              str_str_append(cnt,str);
              str_chars_append(cnt,"\">",2);

              str_str_append(cnt,tmpstr);
              str_chars_append(cnt,"</span>",7);

              ptr += tmpstr->len - 1;
            }
            else if(cf_strcmp(str->content,"pop") == 0) {
              str = array_element_at(&statement->args,2);
              if(str && pops) *pops = atoi(str->content) - 1;

              *pos = ptr + tmpstr->len;
              str_str_append(cnt,tmpstr);
              if(extra_arg) {
                str_cleanup(tmpstr);
                free(tmpstr);
              }
              return 0;
            }
            else if(cf_strcmp(str->content,"stay") == 0) {
              str_str_append(cnt,tmpstr);
              ptr += tmpstr->len - 1;
            }
            else {
              str = array_element_at(&statement->args,2);

              str_chars_append(cnt,"<span class=\"",13);
              str_str_append(cnt,str);
              str_chars_append(cnt,"\">",2);

              str = array_element_at(&statement->args,1);
              if((tmpblock = flt_syntax_block_by_name(file,str->content)) == NULL) {
                fprintf(stderr,"Could not find block %s!\n",str->content);
                return 1;
              }

              str_str_append(cnt,tmpstr);

              tmpchar = ptr + tmpstr->len;
              priv_pops = 0;
              if(flt_syntax_doit(file,tmpblock,text,len,cnt,&tmpchar,NULL,xhtml,&priv_pops) != 0) return 1;
              ptr = tmpchar - 1;
              str_chars_append(cnt,"</span>",7);

              /* maybe we shall return more than one state */
              if(priv_pops > 0) {
                if(pops) *pops = priv_pops - 1;
                if(pos) *pos = ptr + 1;
                return 0;
              }
            }
          }

          if(extra_arg) {
            str_cleanup(tmpstr);
            free(tmpstr);
          }
          /* }}} */
          break;

        case FLT_SYNTAX_ONSTRINGLIST:
          /* {{{ onstringlist */
          if(flt_syntax_check_for_prev(ptr,begin) != 0) continue;

          str = array_element_at(&statement->args,0);
          if((tmplist = flt_syntax_list_by_name(file,str->content)) == NULL) {
            fprintf(stderr,"could not find list %s!\n",str->content);
            return 1;
          }

          tmpchar = flt_syntax_get_token(ptr);

          if(tmpchar && (tmpstr = array_bsearch(&tmplist->list,tmpchar,flt_syntax_my_scmp)) != NULL) {
            matched = 1;

            str = array_element_at(&statement->args,1);
            if(cf_strcmp(str->content,"highlight") == 0) {
              str = array_element_at(&statement->args,2);
              
              str_chars_append(cnt,"<span class=\"",13);
              str_str_append(cnt,str);
              str_chars_append(cnt,"\">",2);

              str_chars_append(cnt,tmpchar,tmpstr->len);
              str_chars_append(cnt,"</span>",7);

              ptr += tmpstr->len - 1;
            }
            else if(cf_strcmp(str->content,"pop") == 0) {
              str = array_element_at(&statement->args,2);
              if(str && pops) *pops = atoi(str->content) - 1;

              str = array_element_at(&statement->args,0);
              *pos = ptr + str->len;
              str_chars_append(cnt,tmpchar,tmpstr->len);
              return 0;
            }
            else if(cf_strcmp(str->content,"stay") == 0) {
              str = array_element_at(&statement->args,0);
              str_chars_append(cnt,tmpchar,tmpstr->len);
              ptr += tmpstr->len - 1;
            }
            else {
              str = array_element_at(&statement->args,2);

              str_chars_append(cnt,"<span class=\"",13);
              str_str_append(cnt,str);
              str_chars_append(cnt,"\">",2);
              
              str = array_element_at(&statement->args,1);
              if((tmpblock = flt_syntax_block_by_name(file,str->content)) == NULL) {
                fprintf(stderr,"Could not find block %s!\n",str->content);
                return 1;
              }

              str_chars_append(cnt,tmpchar,tmpstr->len);
              free(tmpchar);

              tmpchar = ptr + tmpstr->len;
              priv_pops = 0;
              if(flt_syntax_doit(file,tmpblock,text,len,cnt,&tmpchar,NULL,xhtml,&priv_pops) != 0) return 1;
              ptr = tmpchar - 1;
              str_chars_append(cnt,"</span>",7);

              if(priv_pops > 0) {
                if(pops) *pops = priv_pops - 1;
                if(pos) *pos = ptr + 1;
                return 0;
              }
            }
          }
          else {
            if(tmpchar) free(tmpchar);
          }
          /* }}} */
          break;

        case FLT_SYNTAX_ONREGEXP:
          /* {{{ onregexp */
          tmppreg = array_element_at(&statement->pregs,0);
          if(pcre_exec(tmppreg->re,tmppreg->extra,ptr,len-(text-ptr),0,0,stdvec,12) >= 0) {
            matched = 1;

            str = array_element_at(&statement->args,0);
            if(cf_strcmp(str->content,"highlight") == 0) {
              tmpchar = strndup(ptr+stdvec[0],stdvec[1]-stdvec[0]);

              str = array_element_at(&statement->args,1);
              str_chars_append(cnt,"<span class=\"",13);
              str_str_append(cnt,str);
              str_chars_append(cnt,"\">",2);

              str_chars_append(cnt,tmpchar,stdvec[1]-stdvec[0]);
              str_chars_append(cnt,"</span>",7);

              ptr += stdvec[1] - stdvec[0] - 1;
            }
            else if(cf_strcmp(str->content,"pop") == 0) {
              str = array_element_at(&statement->args,1);
              if(str && pops) *pops = atoi(str->content) - 1;

              str_chars_append(cnt,ptr+stdvec[0],stdvec[1]-stdvec[0]);
              *pos = ptr + (stdvec[1] - stdvec[0]);
              return 0;
            }
            else if(cf_strcmp(str->content,"stay") == 0) {
              str_chars_append(cnt,ptr+stdvec[0],stdvec[1]-stdvec[0]);
              ptr += stdvec[1] - stdvec[0];
            }
            else {
              str = array_element_at(&statement->args,1);

              str_chars_append(cnt,"<span class=\"",13);
              str_str_append(cnt,str);
              str_chars_append(cnt,"\">",2);

              str = array_element_at(&statement->args,0);
              if((tmpblock = flt_syntax_block_by_name(file,str->content)) == NULL) {
                fprintf(stderr,"Could not find block %s!\n",str->content);
                return 1;
              }

              str_chars_append(cnt,ptr+stdvec[0],stdvec[1]-stdvec[0]);
              tmpchar = ptr + (stdvec[1] - stdvec[0]);
              priv_pops = 0;
              if(flt_syntax_doit(file,tmpblock,text,len,cnt,&tmpchar,NULL,xhtml,&priv_pops) != 0) return 1;
              ptr = tmpchar - 1;
              str_chars_append(cnt,"</span>",7);

              if(priv_pops > 0) {
                if(pops) *pops = priv_pops - 1;
                if(pos) *pos = ptr + 1;
                return 0;
              }
            }
          }
          /* }}} */
          break;

        case FLT_SYNTAX_ONREGEXP_BACKREF:
          /* {{{ onregexp_backref */
          tmppreg = array_element_at(&statement->pregs,0);
          if(pcre_exec(tmppreg->re,tmppreg->extra,ptr,len-(text-ptr),0,0,stdvec,42) >= 0) {
            matched = 1;

            str = array_element_at(&statement->args,2);

            str_chars_append(cnt,"<span class=\"",13);
            str_str_append(cnt,str);
            str_chars_append(cnt,"\">",2);

            str = array_element_at(&statement->args,0);
            if((tmpblock = flt_syntax_block_by_name(file,str->content)) == NULL) {
              fprintf(stderr,"Could not find block %s!\n",str->content);
              return 1;
            }

            str = array_element_at(&statement->args,1);

            priv_extra = NULL;
            str_chars_append(cnt,ptr+stdvec[0],stdvec[1]-stdvec[0]);
            tmpchar = ptr + (stdvec[1] - stdvec[0]);

            if(pcre_get_substring((const char *)ptr,stdvec,42,atoi(str->content),(const char **)&priv_extra) < 0) break;
            priv_pops = 0;
            if(flt_syntax_doit(file,tmpblock,text,len,cnt,&tmpchar,priv_extra,xhtml,&priv_pops) != 0) return 1;

            pcre_free_substring(priv_extra);
            ptr = tmpchar - 1;
            str_chars_append(cnt,"</span>",7);

            if(priv_pops > 0) {
              if(pops) *pops = priv_pops - 1;
              if(pos) *pos = ptr + 1;
              return 0;
            }
          }
          /* }}} */
          break;
        case FLT_SYNTAX_ONREGEXP_AFTER:
          /* {{{ onregexpafter */
          tmppreg = array_element_at(&statement->pregs,0);
          if(pcre_exec(tmppreg->re,tmppreg->extra,ptr,len-(text-ptr),0,0,stdvec,42) >= 0) {
            x = stdvec[1] - stdvec[0];
            tmppreg = array_element_at(&statement->pregs,1);

            if(pcre_exec(tmppreg->re,tmppreg->extra,ptr+x,len-(text-ptr)-x,0,0,stdvec,42) >= 0) {
              matched = 1;
              str_chars_append(cnt,ptr,x);
              ptr += x;

              str = array_element_at(&statement->args,0);
              if(cf_strcmp(str->content,"highlight") == 0) {
                tmpchar = strndup(ptr+stdvec[0],stdvec[1]-stdvec[0]);

                str = array_element_at(&statement->args,1);
                str_chars_append(cnt,"<span class=\"",13);
                str_str_append(cnt,str);
                str_chars_append(cnt,"\">",2);

                str_chars_append(cnt,tmpchar,stdvec[1]-stdvec[0]);
                str_chars_append(cnt,"</span>",7);

                ptr += stdvec[1] - stdvec[0] - 1;
              }
              else if(cf_strcmp(str->content,"pop") == 0) {
                str = array_element_at(&statement->args,1);
                if(str && pops) *pops = atoi(str->content) - 1;

                str_chars_append(cnt,ptr+stdvec[0],stdvec[1]-stdvec[0]);
                *pos = ptr + (stdvec[1] - stdvec[0]);
                return 0;
              }
              else if(cf_strcmp(str->content,"stay") == 0) {
                str_chars_append(cnt,ptr+stdvec[0],stdvec[1]-stdvec[0]);
                ptr += stdvec[1] - stdvec[0];
              }
              else {
                str = array_element_at(&statement->args,1);

                str_chars_append(cnt,"<span class=\"",13);
                str_str_append(cnt,str);
                str_chars_append(cnt,"\">",2);

                str = array_element_at(&statement->args,0);
                if((tmpblock = flt_syntax_block_by_name(file,str->content)) == NULL) {
                  fprintf(stderr,"Could not find block %s!\n",str->content);
                  return 1;
                }

                str_chars_append(cnt,ptr+stdvec[0],stdvec[1]-stdvec[0]);
                tmpchar = ptr + (stdvec[1] - stdvec[0]);
                priv_pops = 0;
                if(flt_syntax_doit(file,tmpblock,text,len,cnt,&tmpchar,NULL,xhtml,&priv_pops) != 0) return 1;
                ptr = tmpchar - 1;
                str_chars_append(cnt,"</span>",7);

                if(priv_pops > 0) {
                  if(pops) *pops = priv_pops - 1;
                  if(pos) *pos = ptr + 1;
                  return 0;
                }
              }
            }
          }
          /* }}} */
          break;
        case FLT_SYNTAX_ONREGEXP_AFTER_BACKREF:
          /* {{{ onregexpafter_backref */
          tmppreg = array_element_at(&statement->pregs,0);
          if(pcre_exec(tmppreg->re,tmppreg->extra,ptr,len-(text-ptr),0,0,stdvec,42) >= 0) {
            x = stdvec[1] - stdvec[0];
            tmppreg = array_element_at(&statement->pregs,1);

            if(pcre_exec(tmppreg->re,tmppreg->extra,ptr+x,len-(text-ptr)-x,0,0,stdvec,42) >= 0) {
              matched = 1;
              str_chars_append(cnt,ptr,x);
              ptr += x;

              str = array_element_at(&statement->args,2);
              str_chars_append(cnt,"<span class=\"",13);
              str_str_append(cnt,str);
              str_chars_append(cnt,"\">",2);

              priv_extra = NULL;
              str_chars_append(cnt,ptr+stdvec[0],stdvec[1]-stdvec[0]);
              tmpchar = ptr + (stdvec[1] - stdvec[0]);

              str = array_element_at(&statement->args,0);
              if((tmpblock = flt_syntax_block_by_name(file,str->content)) == NULL) {
                fprintf(stderr,"Could not find block %s!\n",str->content);
                return 1;
              }

              str = array_element_at(&statement->args,1);
              if(pcre_get_substring((const char *)ptr,stdvec,42,atoi(str->content),(const char **)&priv_extra) < 0) break;
              priv_pops = 0;
              if(flt_syntax_doit(file,tmpblock,text,len,cnt,&tmpchar,priv_extra,xhtml,&priv_pops) != 0) return 1;

              pcre_free_substring(priv_extra);
              ptr = tmpchar - 1;
              str_chars_append(cnt,"</span>",7);

              if(priv_pops > 0) {
                if(pops) *pops = priv_pops - 1;
                if(pos) *pos = ptr + 1;
                return 0;
              }
            }
          }
          /* }}} */
          break;
      }
    }

    if(matched == 0) {
      if(*ptr == '&') {
        for(;*ptr != ';';++ptr) str_char_append(cnt,*ptr);
        str_char_append(cnt,*ptr);
      }
      else if(*ptr == '<') {
        for(;*ptr && *ptr != '>';++ptr) str_char_append(cnt,*ptr);
        str_char_append(cnt,*ptr);
      }
      else str_char_append(cnt,*ptr);
    }
    else start = 0;
  }

  if(pos) *pos = ptr;

  return 0;
}
/* }}} */

/* {{{ flt_syntax_highlight */
int flt_syntax_highlight(t_string *content,t_string *bco,const u_char *lang,const u_char *fname) {
  u_char *forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  flt_syntax_pattern_file_t *file;
  size_t i;
  int found = 0;
  t_string code;
  u_char *pos = NULL;
  t_name_value *xmlm = cfg_get_first_value(&fo_default_conf,forum_name,"XHTMLMode");

  for(i=0;i<flt_syntax_files.elements;++i) {
    file = array_element_at(&flt_syntax_files,i);

    if(cf_strcmp(file->lang,lang) == 0) {
      found = 1;
      break;
    }
  }

  if(!found) {
    if(flt_syntax_load(fname,lang) != 0) return 1;
    file = array_element_at(&flt_syntax_files,flt_syntax_files.elements-1);
  }

  str_init(&code);
  if(flt_syntax_doit(file,NULL,content->content,content->len,&code,&pos,NULL,cf_strcmp(xmlm->values[0],"yes") == 0,NULL) != 0) {
    str_cleanup(&code);
    return 1;
  }

  str_chars_append(bco,"<code title=\"",13);
  str_chars_append(bco,lang,strlen(lang));
  str_chars_append(bco,"\" class=\"",9);
  str_chars_append(bco,lang,strlen(lang));
  str_chars_append(bco,"\">",2);
  str_str_append(bco,&code);
  str_cleanup(&code);
  str_chars_append(bco,"</code>",7);

  return 0;
}
/* }}} */

/* {{{ flt_syntax_execute */
int flt_syntax_execute(t_configuration *fdc,t_configuration *fvc,t_cl_thread *thr,const u_char *directive,const u_char **parameters,size_t plen,t_string *bco,t_string *bci,t_string *content,t_string *cite,const u_char *qchars,int sig) {
  t_string str;
  struct stat st;
  u_char *lang,*ptr;

  /* {{{ we don't know what language we got, so just put a <code> around it */
  if(flt_syntax_active == 0 || plen != 2 || cf_strcmp(parameters[0],"lang") != 0) {
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

  str_init(&str);
  for(ptr=(u_char *)parameters[1];*ptr;++ptr) {
    if(isalnum(*ptr)) str_char_append(&str,tolower(*ptr));
  }
  lang = str.content;

  /* we got a language, check if it exists */
  str_init(&str);
  str_char_set(&str,flt_syntax_patterns_dir,strlen(flt_syntax_patterns_dir));
  str_char_append(&str,'/');
  str_chars_append(&str,lang,strlen(lang));
  str_chars_append(&str,".pat",4);

  /* {{{ language doesnt exist, put a <code> around it */
  if(stat(str.content,&st) == -1) {
    str_chars_append(bco,"<code title=\"",13);
    str_chars_append(bco,parameters[1],strlen(parameters[1]));
    str_chars_append(bco,"\" class=\"",9);
    str_chars_append(bco,parameters[1],strlen(parameters[1]));
    str_chars_append(bco,"\">",2);
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
  if(flt_syntax_highlight(content,bco,lang,str.content) != 0) {
    str_chars_append(bco,"<code title=\"",13);
    str_chars_append(bco,parameters[1],strlen(parameters[1]));
    str_chars_append(bco,"\" class=\"",9);
    str_chars_append(bco,parameters[1],strlen(parameters[1]));
    str_chars_append(bco,"\">",2);
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

  return FLT_OK;
}
/* }}} */

int flt_syntax_validate(t_configuration *fdc,t_configuration *fvc,const u_char *directive,const u_char **parameters,size_t plen,t_cf_tpl_variable *var) {
  u_char *err,*lang;
  register u_char *ptr;
  size_t len;

  struct stat st;

  t_string str;

  if(plen != 2 || cf_strcmp(parameters[0],"lang") != 0) {
    if((err = cf_get_error_message("E_CODE_NOLANG",&len)) != NULL) {
      cf_tpl_var_addvalue(var,TPL_VARIABLE_STRING,err,len);
      free(err);
      return FLT_ERROR;
    }
  }

  str_init(&str);
  for(ptr=(u_char *)parameters[1];*ptr;++ptr) {
    if(isalnum(*ptr)) str_char_append(&str,tolower(*ptr));
  }
  lang = str.content;

  /* we got a language, check if it exists */
  str_init(&str);
  str_char_set(&str,flt_syntax_patterns_dir,strlen(flt_syntax_patterns_dir));
  str_char_append(&str,'/');
  str_chars_append(&str,lang,strlen(lang));
  str_chars_append(&str,".pat",4);

  if(stat(str.content,&st) == -1) {
    str_cleanup(&str);

    if((err = cf_get_error_message("E_CODE_LANG",&len)) != NULL) {
      cf_tpl_var_addvalue(var,TPL_VARIABLE_STRING,err,len);
      free(err);
      return FLT_ERROR;
    }
  }

  str_cleanup(&str);

  return FLT_DECLINE;
}

/* {{{ flt_syntax_init */
int flt_syntax_init(t_cf_hash *cgi,t_configuration *dc,t_configuration *vc) {
  cf_html_register_directive("code",flt_syntax_execute,CF_HTML_DIR_TYPE_ARG|CF_HTML_DIR_TYPE_BLOCK);
  cf_html_register_validator("code",flt_syntax_validate,CF_HTML_DIR_TYPE_ARG|CF_HTML_DIR_TYPE_BLOCK);

  array_init(&flt_syntax_files,sizeof(flt_syntax_pattern_file_t),NULL);

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_syntax_handle */
int flt_syntax_handle(t_configfile *cfile,t_conf_opt *opt,const u_char *context,u_char **args,size_t argnum) {
  u_char *fn;

  if(cf_strcmp(opt->name,"ActivateSyntax") == 0) {
    fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
    if(cf_strcmp(fn,context) == 0) flt_syntax_active = cf_strcmp(args[0],"yes") == 0;
  }
  else {
    if(flt_syntax_patterns_dir != NULL) free(flt_syntax_patterns_dir);
    flt_syntax_patterns_dir = strdup(args[0]);
  }

  return 0;
}
/* }}} */

void flt_syntax_cleanup(void) {
  if(flt_syntax_patterns_dir != NULL) free(flt_syntax_patterns_dir);
}

t_conf_opt flt_syntax_config[] = {
  { "ActivateSyntax",    flt_syntax_handle, CFG_OPT_CONFIG|CFG_OPT_USER|CFG_OPT_LOCAL, NULL },
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
