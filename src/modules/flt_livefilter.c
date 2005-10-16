/**
 * \file flt_livefilter.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 *
 * Live category and poster filter
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
#include <sys/types.h>

#include "readline.h"
#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "cfcgi.h"
#include "template.h"
#include "clientlib.h"
#include "charconvert.h"

/* }}} */

static const u_char *symbols[] = {
  "TOK_EOS", "TOK_EQ", "TOK_NE",
  "TOK_OR", "TOK_AND", "TOK_ID",
  "TOK_STR", "TOK_CONTAINS", "TOK_LPAREN",
  "TOK_RPAREN"
};

#define flt_lf_get_type(x) (symbols[x])

#define PREC_ID    60
#define PREC_EQ    50 /* =, != */
#define PREC_AND   40
#define PREC_OR    30
#define PREC_PAREN 20

#define TOK_EOS          0x00
#define TOK_EQ           0x01
#define TOK_NE           0x02
#define TOK_OR           0x03
#define TOK_AND          0x04
#define TOK_ID           0x05
#define TOK_STR          0x06
#define TOK_CONTAINS     0x07
#define TOK_LPAREN       0x08
#define TOK_RPAREN       0x09
#define TOK_CONTAINS_NOT 0x0A

#define T_STRING 0x00
#define T_INT    0x01
#define T_DATE   0x02
#define T_BOOL   0x04

#define FLT_LF_FLAG_HOUR 0x1
#define FLT_LF_FLAG_MIN  0x2
#define FLT_LF_FLAG_SEC  0x4

typedef struct {
  int type,flags;
  void *val;
} flt_lf_result_t;

typedef struct s_node {
  int type,prec;
  u_char *content;

  struct s_node *left,*right,*parent,*argument;
} flt_lf_node_t;

static flt_lf_node_t *flt_lf_first = NULL;
static int flt_lf_active = 0;
static int flt_lf_overwrite = 0;

static cf_string_t flt_lf_str = { 0, 0, 128, NULL };

static int flt_lf_success = 1;

static u_char *flt_lf_fn = NULL;

/* {{{ flt_lf_case_strstr */
u_char *flt_lf_case_strstr(const u_char *haystack,const u_char *needle) {
  size_t len1 = strlen(haystack);
  size_t len2 = strlen(needle);
  size_t i;

  if(len1 < len2) return NULL;
  if(len1 == len2) return cf_strcasecmp(haystack,needle) == 0 ? (u_char *)haystack : NULL;

  for(i=0;i<=len1-len2;i++) {
    if(cf_strncasecmp(haystack+i,needle,len2) == 0) return (u_char *)(haystack+i);
  }

  return NULL;
}
/* }}} */

/* {{{ flt_lf_read_string */
int flt_lf_read_string(register u_char *ptr) {
  cf_str_init(&flt_lf_str);

  for(;*ptr;ptr++) {
    switch(*ptr) {
      case '\\':
        switch(*(++ptr)) {
          case 'n':
            cf_str_char_append(&flt_lf_str,'\n');
            break;
          case 't':
            cf_str_char_append(&flt_lf_str,'\t');
            break;
          case '"':
            cf_str_char_append(&flt_lf_str,'"');
            break;
          default:
            cf_str_char_append(&flt_lf_str,*ptr);
            break;
        }
      case '"':
        return 0;
      default:
        cf_str_char_append(&flt_lf_str,*ptr);
    }
  }

  return -1;
}
/* }}} */

/* {{{ flt_lf_scanner */
int flt_lf_scanner(u_char *str,u_char **pos) {
  register u_char *ptr = *pos;
  int ret;

  for(;*ptr;ptr++) {
    /* go to the next token */
    for(;*ptr && isspace(*ptr);ptr++);

    *pos = ptr;

    /* decide, what it is */
    switch(*ptr) {
      case '=':
        if(*(ptr + 1) == '~') {
          *pos = ptr + 2;
          cf_str_char_set(&flt_lf_str,ptr,2);
          return TOK_CONTAINS;
        }
        else {
          *pos = ptr + 1;
          cf_str_char_set(&flt_lf_str,ptr,1);
          return TOK_EQ;
        }
      case '(':
        cf_str_char_set(&flt_lf_str,ptr,1);
        *pos = ptr + 1;
        return TOK_LPAREN;
      case ')':
        cf_str_char_set(&flt_lf_str,ptr,1);
        *pos = ptr + 1;
        return TOK_RPAREN;
      case '!':
        cf_str_char_set(&flt_lf_str,ptr,2);
        *pos = ptr + 2;

        if(*(ptr + 1) == '~') {
          return TOK_CONTAINS_NOT;
        }
        else {
          return TOK_NE;
        }
      case '|':
        cf_str_char_set(&flt_lf_str,ptr,1);
        *pos = ptr + 1;
        return TOK_OR;
      case '&':
        cf_str_char_set(&flt_lf_str,ptr,1);
        *pos = ptr + 1;
        return TOK_AND;
      case '"':
        /* read string */
        if((ret = flt_lf_read_string(ptr+1)) != 0) return ret;

        /* safe new position */
        *pos = ptr + flt_lf_str.len + 2;

        /* return token */
        return TOK_STR;
      default:
        /*
         * a simple *ptr check is not enough at this point,
         * we can get ~= instead of =~ or ~! or something
         * like that; maybe an additional isalpha() check
         * is enough
         */
        if(*ptr && isalpha(*ptr)) {
          /* find end */
          for(;*ptr && isalnum(*ptr);ptr++);

          /* safe identifier */
          cf_str_char_set(&flt_lf_str,*pos,ptr - *pos);

          /* set position */
          *pos = ptr;

          /* return token */
          return TOK_ID;
        }
        else {
          flt_lf_success = 0;
          return TOK_EOS;
        }
    }
  }

  return TOK_EOS;
}
/* }}} */

/* {{{ flt_lf_insert_node */
flt_lf_node_t *flt_lf_insert_node(flt_lf_node_t *cur,flt_lf_node_t *tok,flt_lf_node_t *root) {
  flt_lf_node_t *n = cur;

  if(!cur) {
    if(root) root->argument = tok;
    else     flt_lf_first = tok;
    return tok;
  }

  if(cur->prec <= tok->prec) {
    tok->parent = cur;

    if(cur->left) {
      cur->right = tok;
    }
    else {
      cur->left = tok;
    }
  }
  else {
    /* we have to search the right position due to the precedence */
    while(cur && cur->prec > tok->prec) {
      n   = cur;
      cur = cur->parent;
    }

    if(!cur) {
      if(root) root->argument = tok;
      else     flt_lf_first = tok;
      tok->left         = n;
      tok->left->parent = tok;
    }
    else {
      tok->parent = cur;

      if(cur->right) {
        tok->left         = cur->right;
        tok->left->parent = tok;
        cur->right        = tok;
      }
      else {
        tok->left         = cur->left;
        tok->left->parent = tok;
        cur->left         = tok;
      }
    }
  }

  return tok;
}
/* }}} */

/* {{{ flt_lf_parse_string */
int flt_lf_parse_string(u_char *str,u_char **pos,cf_template_t *tpl,flt_lf_node_t *node,flt_lf_node_t *root_node,cf_configuration_t *dc) {
  int ret = 0;
  flt_lf_node_t *current = NULL;
  u_char *forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  cf_name_value_t *cs = cf_cfg_get_first_value(dc,forum_name,"ExternCharset");

  while((ret = flt_lf_scanner(str,pos)) > 0) {
    current       = cf_alloc(NULL,1,sizeof(*current),CF_ALLOC_CALLOC);
    current->type = ret;

    switch(ret) {
      case TOK_STR:
      case TOK_ID:
        current->prec = PREC_ID;

        if(cf_strcasecmp(flt_lf_str.content,"true") == 0 && ret == TOK_ID) {
          current->content = (u_char *)1;
          cf_str_cleanup(&flt_lf_str);
        }
        else if(cf_strcasecmp(flt_lf_str.content,"false") == 0 && ret == TOK_ID) {
          current->content = (u_char *)0;
          cf_str_cleanup(&flt_lf_str);
        }
        else {
          if(cf_strcmp(cs->values[0],"UTF-8")) {
            if((current->content = charset_convert(flt_lf_str.content,flt_lf_str.len,cs->values[0],"UTF-8",NULL)) == NULL) {
              current->content = flt_lf_str.content;
              cf_str_init(&flt_lf_str);
            }
            else {
              cf_str_cleanup(&flt_lf_str);
            }
          }
          else {
            current->content = flt_lf_str.content;
            cf_str_init(&flt_lf_str);
          }
        }

        break;
      case TOK_LPAREN:
        current->prec     = PREC_PAREN;
        current->argument = cf_alloc(NULL,1,sizeof(*current->argument),CF_ALLOC_CALLOC);
        if((ret = flt_lf_parse_string(str,pos,tpl,NULL,current,dc)) != 0) return ret;
        break;
      case TOK_RPAREN:
        free(current);

        if(root_node->prec != PREC_PAREN) {
          flt_lf_success = 0;
          return 1;
        }

        return 0;
        break;
      case TOK_CONTAINS_NOT:
      case TOK_CONTAINS:
      case TOK_NE:
      case TOK_EQ:
        current->prec = PREC_EQ;
        break;
      case TOK_OR:
        current->prec = PREC_OR;
        break;
      case TOK_AND:
        current->prec = PREC_AND;
        break;
    }

    node = flt_lf_insert_node(node,current,root_node);
  }

  return 0;
}
/* }}} */

/* {{{ flt_lf_form */
int flt_lf_form(cf_hash_t *head,cf_configuration_t *dc,cf_configuration_t *vc,cf_template_t *begin,cf_template_t *end) {
  u_char *filter_str,*pos;

  if(head) {
    if((filter_str = cf_cgi_get(head,"lf")) != NULL) {
      cf_hash_set(GlobalValues,"openclose",9,"0",1);
      pos = filter_str;
      flt_lf_parse_string(filter_str,&pos,begin,NULL,NULL,dc);
      cf_tpl_setvalue(begin,"lf",TPL_VARIABLE_STRING,filter_str,strlen(filter_str));
    }
  }

  if(flt_lf_active) {
    cf_tpl_setvalue(begin,"livefilter",TPL_VARIABLE_STRING,"1",1);

    return FLT_OK;
  }

  return FLT_DECLINE;
}
/* }}} */


/* {{{ flt_lf_to_int */
int flt_lf_intval(const u_char *val) {
  register u_char *ptr;

  for(ptr=(u_char *)val;*ptr && !isdigit(*ptr);++ptr);

  if(*ptr) return strtol(ptr,NULL,10);
  return 0;
}
/* }}} */

/* {{{ flt_fl_to_string */
void flt_lf_to_string(flt_lf_result_t *v) {
  u_char buff[256];

  switch(v->type) {
    case T_BOOL:
      v->val = strdup(v->val ? "true" : "false");
      break;
    case T_STRING:
      break;
    case T_INT:
      snprintf(buff,256,"%d",(int)v->val);
      v->val = strdup(buff);
      break;
    case T_DATE:
      snprintf(buff,256,"%ld",(time_t)v->val);
      v->val = strdup(buff);
      break;
  }

  v->type = T_STRING;
}
/* }}} */

/* {{{ flt_lf_to_bool */
void flt_lf_to_bool(flt_lf_result_t *v) {
  u_char *tmp;

  switch(v->type) {
    case T_BOOL:
      break;
    case T_STRING:
      tmp = v->val;

      if(*((u_char *)v->val)) v->val = (void *)1;
      else v->val = (void *)0;

      free(tmp);
      break;
    case T_INT:
    case T_DATE:
      if(v->val) v->val = (void *)1;
      else v->val = (void *)0;
  }

  v->type = T_BOOL;
}
/* }}} */

/* {{{ flt_lf_to_int */
void flt_lf_to_int(flt_lf_result_t *v) {
  u_char *tmp;

  switch(v->type) {
    case T_STRING:
      tmp = v->val;
      v->val = (void *)flt_lf_intval(v->val);
      break;
    case T_BOOL:
    case T_DATE:
    case T_INT:
      break;
  }

  v->type = T_INT;
}
/* }}} */

/* {{{ flt_lf_transform_date */
time_t flt_lf_transform_date(const u_char *datestr,flt_lf_result_t *v) {
  struct tm t;
  u_char *ptr,*before;
  u_char *str = cf_alloc(NULL,strlen(datestr)+1,1,CF_ALLOC_MALLOC);

  strcpy(str,datestr);
  ptr = before = str;

  memset(&t,0,sizeof(t));

  for(;*ptr && *ptr != '.';ptr++);
  if(*ptr == '.') {
    *ptr = '\0';
    t.tm_mday = atoi(before);
    *ptr = '.';
  }
  else {
    free(str);
    return (time_t)0;
  }

  for(before= ++ptr;*ptr && *ptr != '.';ptr++);
  if(*ptr == '.') {
    *ptr = '\0';
    t.tm_mon = atoi(before)-1;
    *ptr = '.';
  }
  else {
    free(str);
    return (time_t)0;
  }

  for(before= ++ptr;*ptr && !isspace(*ptr);ptr++);     /* search the '\0' or a whitespace; if a whitespace
                                                        * follows, there are also hours and mins and perhaps seconds */
  if(isspace(*ptr) || *ptr == '\0') {                  /* Is this a a valid entry? */
    t.tm_year = atoi(before) - 1900;                   /* tm_year contains the year - 1900 */
  }
  else {
    free(str);
    return (time_t)0;                                  /* not a valid entry */
  }

  if(*ptr == ' ') {                                    /* follows an hour and a minute? */
    for(;*ptr && *ptr == ' ';ptr++);                   /* skip trailing whitespaces */

    if(*ptr) {                                         /* have we got a string like "1.1.2001 "? */
      for(before=ptr;*ptr && *ptr != ':';++ptr);       /* search the next colon (Hours are seperated from minutes by
                                                        * colons */
      if(*ptr == ':' || *ptr == '\0') {
        v->flags = FLT_LF_FLAG_HOUR;
        if(*ptr != '\0') {
          *ptr = '\0';
          t.tm_hour = atoi(before);                      /* get the hour */
          *ptr = ':';
        }
        else t.tm_hour = atoi(before);
      }
      else {
        free(str);
        return (time_t)0;
      }

      if(*ptr) {
        for(before=++ptr;*ptr && *ptr != ':';ptr++);    /* search for the end of the string or another colon */

        if(*ptr == ':' || *ptr == '\0') {
          if(*ptr == ':') {
            v->flags |= FLT_LF_FLAG_MIN;
            *ptr = '\0';
            t.tm_min = atoi(before);                       /* get the minutes */
            *ptr = ':';
          }
          else {
            v->flags |= FLT_LF_FLAG_MIN;
            t.tm_min = atoi(before);
          }
        }
        else {
          free(str);
          return (time_t)0;
        }

        if(*ptr == ':') {                                /* seconds following */
          v->flags |= FLT_LF_FLAG_SEC;

          before= ptr + 1;                               /* after the seconds, there can only follow the end of
                                                        * the string
                                                        */
          if(!*ptr) return (time_t)0;
          t.tm_sec = atoi(before);
        }
      }
    }
  }

  /* we have to set daylight saving time flag */
  //t.tm_isdst = 1;

  free(str);
  return cf_timegm(&t);                                   /* finally, try to generate the timestamp */
}
/* }}} */

/* {{{ flt_lf_to_date */
void flt_lf_to_date(flt_lf_result_t *v) {
  u_char *tmp;

  switch(v->type) {
    case T_STRING:
      tmp = v->val;
      v->val = (void *)flt_lf_transform_date(v->val,v);
      free(tmp);
      break;
    case T_BOOL:
      v->val = 0;
      break;
    case T_INT:
    case T_DATE:
      break;
  }

  v->type = T_DATE;
}
/* }}} */

/* {{{ flt_lf_r2l */
void flt_lf_r2l(flt_lf_result_t *l,flt_lf_result_t *r) {
  switch(l->type) {
    case T_BOOL:
      flt_lf_to_bool(r);
      break;
    case T_STRING:
      flt_lf_to_string(r);
      break;
    case T_INT:
      flt_lf_to_int(r);
      break;
    case T_DATE:
      flt_lf_to_date(r);
      break;
  }
}
/* }}} */

/* {{{ flt_lf_is_true */
int flt_lf_is_true(flt_lf_result_t *v) {
  switch(v->type) {
    case T_STRING:
      return *((u_char *)v->val);
    default:
      return (int)v->val;
  }
}
/* }}} */


/* {{{ flt_lf_evaluate */
flt_lf_result_t *flt_lf_evaluate(flt_lf_node_t *n,message_t *msg,u_int64_t tid) {
  mod_api_t is_visited = cf_get_mod_api_ent("is_visited");
  flt_lf_result_t *result = cf_alloc(NULL,1,sizeof(*result),CF_ALLOC_CALLOC);
  flt_lf_result_t *l = NULL,*r = NULL,*tmp;
  struct tm tm,tm1;

  if(!n) return NULL;

  switch(n->type) {
    case TOK_CONTAINS:
      /* {{{ contains */
      l = flt_lf_evaluate(n->left,msg,tid);
      r = flt_lf_evaluate(n->right,msg,tid);

      flt_lf_to_string(l);
      flt_lf_to_string(r);

      result->type = T_BOOL;

      if(flt_lf_case_strstr(l->val,r->val)) result->val  = (void *)1;
      else result->val  = (void *)0;

      if(l->type == T_STRING) free(l->val);
      if(r->type == T_STRING) free(r->val);
      free(r);
      free(l);

      return result;
      /* }}} */

    case TOK_CONTAINS_NOT:
      /* {{{ contains not */
      l = flt_lf_evaluate(n->left,msg,tid);
      r = flt_lf_evaluate(n->right,msg,tid);

      flt_lf_to_string(l);
      flt_lf_to_string(r);

      result->type = T_BOOL;

      if(flt_lf_case_strstr(l->val,r->val)) result->val = (void *)0;
      else result->val = (void *)1;

      if(l->type == T_STRING) free(l->val);
      if(r->type == T_STRING) free(r->val);
      free(r);
      free(l);

      return result;
      /* }}} */

    case TOK_EQ:
      /* {{{ equal */
      l = flt_lf_evaluate(n->left,msg,tid);
      r = flt_lf_evaluate(n->right,msg,tid);

      flt_lf_r2l(l,r);
      result->type = T_BOOL;

      if(l->type == T_STRING) {
        if(cf_strcasecmp(l->val,r->val) == 0) result->val = (void *)1; /* true: strings are equal */
        else result->val = (void *)0;
      }
      else {
        if(l->type == T_DATE) {
          memset(&tm,0,sizeof(tm));
          memset(&tm1,0,sizeof(tm1));

          localtime_r((const time_t *)&l->val,&tm);
          gmtime_r((const time_t *)&r->val,&tm1);

          //if(tm1.tm_isdst) tm1.tm_hour -= 1;

          if(tm.tm_year == tm1.tm_year && tm.tm_mon == tm1.tm_mon && tm.tm_mday == tm1.tm_mday) {
            if(r->flags & FLT_LF_FLAG_HOUR) {
              if(tm.tm_hour == tm1.tm_hour) {
                if(r->flags & FLT_LF_FLAG_MIN) {
                  if(tm.tm_min == tm1.tm_min) {
                    if(r->flags & FLT_LF_FLAG_SEC) {
                      if(tm.tm_sec == tm1.tm_sec) result->val = (void *)1;
                      else result->val = (void *)0;
                    }
                    else result->val = (void *)1;
                  }
                  else result->val = (void *)0;
                }
                else result->val = (void *)1;
              }
              else result->val = (void *)0;
            }
            else result->val = (void *)1;
          }
          else result->val = (void *)0;
        }
        else {
          if(l->val == r->val) result->val = (void *)1;
          else result->val = (void *)0;
        }
      }

      if(l->type == T_STRING) free(l->val);
      if(r->type == T_STRING) free(r->val);
      free(r);
      free(l);

      return result;
      /* }}} */

    case TOK_NE:
      /* {{{ not equal */
      l = flt_lf_evaluate(n->left,msg,tid);
      r = flt_lf_evaluate(n->right,msg,tid);

      if(l->type == T_STRING) {
        if(cf_strcasecmp(l->val,r->val) == 0) result->val = (void *)0; /* false: strings are equal */
        else result->val = (void *)1;
      }
      else {
        if(l->val == r->val) result->val = (void *)0;
        else result->val = (void *)1;
      }

      flt_lf_r2l(l,r);
      result->type = T_BOOL;

      if(l->type == T_STRING) free(l->val);
      if(r->type == T_STRING) free(r->val);
      free(r);
      free(l);

      return result;
      /* }}} */

    case TOK_OR:
      /* {{{ or */
      result->type = T_BOOL;
      l = flt_lf_evaluate(n->left,msg,tid);
      if(flt_lf_is_true(l)) result->val = (void *)1;

      if(!result->val) {
        r = flt_lf_evaluate(n->right,msg,tid);
        if(flt_lf_is_true(r)) result->val = (void *)1;
      }

      if(l->type == T_STRING) free(l->val);
      if(r && r->type == T_STRING) free(r->val);
      free(r);
      free(l);

      return result;
      /* }}} */

    case TOK_AND:
      /* {{{ and */
      l = flt_lf_evaluate(n->left,msg,tid);
      r = flt_lf_evaluate(n->right,msg,tid);

      flt_lf_to_bool(l);
      flt_lf_to_bool(r);

      result->type = T_BOOL;

      if(!l->val || !r->val) result->val = (void *)0;
      else result->val = (void *)1;

      free(l);
      free(r);

      return result;
      /* }}} */

    case TOK_ID:
      /* {{{ id */
      if(n->content > (u_char *)1) {
        switch(*n->content) {
          case 'a':
            result->type = T_STRING;
            result->val  = strdup(msg->author.content);
            return result;
          case 's':
            result->type = T_STRING;
            result->val  = strdup(msg->subject.content);
            return result;
          case 'c':
            result->type = T_STRING;
            result->val  = msg->category.len ? strdup(msg->category.content) : strdup("");
            return result;
          case 'd':
            result->type = T_DATE;
            result->val  = (void *)msg->date;
            return result;
          case 'l':
            result->type = T_INT;
            result->val = (void *)msg->level;
            return result;
          case 'v':
            result->type = T_BOOL;
            if(cf_strcasecmp(n->content,"visited") == 0) {
              if(is_visited) result->val = (void *)(is_visited(&(msg->mid)) == NULL ? 0 : 1);
              else result->val = (void *)0;
            }
            else result->val = (void *)(msg->may_show == 0 ? 0 : 1);
            return result;
        }
      }
      else {
        result->type = T_BOOL;
        result->val  = (void *)(n->content == (u_char *)1 ? 1 : 0);
        return result;
      }
      /* }}} */

    case TOK_STR:
      /* {{{ string */
      result->type = T_STRING;
      result->val  = strdup(n->content);
      return result;
      /* }}} */

    case TOK_LPAREN:
      result->type = T_BOOL;
      if(n->argument) {
        tmp = flt_lf_evaluate(n->argument,msg,tid);
        flt_lf_to_bool(tmp);
        result->val = tmp->val;
        free(tmp);
      }
      else result->val = (void *)0;
      return result;
  }

  return NULL;
}
/* }}} */

/* {{{ flt_lf_filter */
int flt_lf_filter(cf_hash_t *head,cf_configuration_t *dc,cf_configuration_t *vc,message_t *msg,u_int64_t tid,int mode) {
  if(mode & CF_MODE_THREADVIEW) return FLT_DECLINE;
  flt_lf_result_t *res;

  if(flt_lf_first && flt_lf_success) {
    res = flt_lf_evaluate(flt_lf_first,msg,tid);
    flt_lf_to_bool(res);

    if(res->val == NULL) msg->may_show = 0;
    else {
      if(flt_lf_overwrite) msg->may_show = 1;
    }

    free(res);

    return FLT_OK;
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_lf_handle_command */
int flt_lf_handle_command(cf_configfile_t *cf,cf_conf_opt_t *opt,const u_char *context,u_char **args,size_t argnum) {
  if(!flt_lf_fn) flt_lf_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(flt_lf_fn,context) != 0) return 0;

  if(cf_strcmp(opt->name,"ActivateLiveFilter") == 0) flt_lf_active = cf_strcmp(args[0],"yes") == 0;
  else flt_lf_overwrite = cf_strcmp(args[0],"yes") == 0;

  return 0;
}
/* }}} */

/* {{{ flt_lf_cleanup */
void flt_lf_cleanup(void) {
  /* \todo Implement cleanup code (I'm a bad guy) */
}
/* }}} */

cf_conf_opt_t config[] = {
  { "ActivateLiveFilter",  flt_lf_handle_command, CF_CFG_OPT_CONFIG|CF_CFG_OPT_USER|CF_CFG_OPT_LOCAL, NULL },
  { "LiveFilterOverwrite", flt_lf_handle_command, CF_CFG_OPT_CONFIG|CF_CFG_OPT_USER|CF_CFG_OPT_LOCAL, NULL },
  { NULL, NULL, 0, NULL }
};

cf_handler_config_t handlers[] = {
  { VIEW_INIT_HANDLER, flt_lf_form },
  { VIEW_LIST_HANDLER, flt_lf_filter },
  { 0, NULL }
};

cf_module_config_t flt_livefilter = {
  MODULE_MAGIC_COOKIE,
  config,
  handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  flt_lf_cleanup
};

/* eof */
