/**
 * \file flt_urlrewrite.c
 * \author Christian Seiler, <christian.seiler@selfhtml.org>
 *
 * rewrite urls based on configuration file parameters
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
#include <pcre.h>
#include <sys/types.h>

#include "readline.h"
#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "cfcgi.h"
#include "template.h"
#include "clientlib.h"
/* }}} */

#define DEFAULT_URLREWRITE_SIZE  5

#define MACRO_NODE_OPERATOR      0
#define MACRO_NODE_VALUE         1

#define MACRO_OP_NOT             0
#define MACRO_OP_AND             1
#define MACRO_OP_OR              2

typedef struct s_flt_urlrewrite_macro_node {
   int type;
   union {
     void *ptr_data;
     int int_data;
   } data;

   struct s_flt_urlrewrite_macro_node *left,
                                      *right,
                                      *parent;

} flt_urlrewrite_macro_node_t;

typedef struct s_flt_urlrewrite_rule {
  pcre *regexp;
  pcre_extra *regexp_extra;
  flt_urlrewrite_macro_node_t *macro_tree;
  const u_char *replacement;
  unsigned long int match_count;
  int *match_arr;
} flt_urlrewrite_rule_t;

array_t *flt_urlrewrite_rules = NULL;

static u_char *flt_urlrewrite_fname = NULL;

int flt_urlrewrite_is_macro_true(flt_urlrewrite_macro_node_t *tree) {
  string_t *data;
  int op_type;
  int *bool_value;

  if(!tree)
    return 0;

  switch(tree->type) {
    case MACRO_NODE_VALUE:
      data = (string_t *)tree->data.ptr_data;
      if(!data->len)
        return 0;
      if(!cf_strcmp(data->content, "true")) {
        return 1;
      } else if(!cf_strcmp(data->content, "logged_in")) {
        return cf_hash_get(GlobalValues,"UserName",8) ? 1 : 0;
      } else {
        bool_value = (int *)cf_hash_get(GlobalValues,data->content,data->len);
        if (!bool_value) {
          return 0;
        } else {
          return *bool_value ? 1 : 0;
        }
      }
      return 0;
      break;
    case MACRO_NODE_OPERATOR: {
      op_type = tree->data.int_data;
      switch(op_type) {
        case MACRO_OP_NOT:
          return !flt_urlrewrite_is_macro_true(tree->right);
        case MACRO_OP_AND:
          return flt_urlrewrite_is_macro_true(tree->left) && flt_urlrewrite_is_macro_true(tree->right);
        case MACRO_OP_OR:
          return flt_urlrewrite_is_macro_true(tree->left) || flt_urlrewrite_is_macro_true(tree->right);
        default:
          return 0;
      }
      break;
    default:
      return 0;
    }
  }
}

void flt_urlrewrite_free_macro_tree(flt_urlrewrite_macro_node_t *tree) {
  if(!tree)
    return;
  if(tree->left)
    flt_urlrewrite_free_macro_tree(tree->left);
  if(tree->right)
    flt_urlrewrite_free_macro_tree(tree->right);
  switch(tree->type) {
    case MACRO_NODE_VALUE:
      str_cleanup((string_t *)tree->data.ptr_data);
      free(tree->data.ptr_data);
      break;
    default:
      break;
  }
  free(tree);
}

flt_urlrewrite_macro_node_t *flt_urlrewrite_parse_macro(const u_char *str) {
  flt_urlrewrite_macro_node_t *cur_node;
  string_t left_side;
  int n_braces;
  int had_zero_brace;
  u_char op;
  u_char *buf;
  u_char *nstr;
  u_char *ptr;

  n_braces = 0;
  had_zero_brace = 0;

  // see if we can strip of uneeded braces at beggining / end of string
  for(ptr = (u_char *)str; *ptr; ptr++) {
    if(*ptr == ')') {
      if(n_braces == 1 && *(ptr+1))
        had_zero_brace = 1;
      n_braces--;
      if(n_braces < 0) { // too many ")"
        return NULL;
      }
    } else if(*ptr == '(') {
      n_braces++;
    }
  }

  if(n_braces != 0) { // too many "("
    return NULL;
  }

  // duplicate so that we can modify it
  nstr = strdup(str);

  // if these braces are just outside, like(a & b) => strip them
  if(!had_zero_brace && nstr[0] == '(' && nstr[strlen(nstr)-1] == ')') {
    nstr[strlen(nstr)-1] = 0;
  }

  cur_node = (flt_urlrewrite_macro_node_t *)fo_alloc(NULL,1,sizeof(flt_urlrewrite_macro_node_t),FO_ALLOC_MALLOC);
  if(!cur_node) {
    free(nstr);
    return NULL;
  }

  str_init(&left_side);

  n_braces = 0;

  if(!had_zero_brace && nstr[0] == '(' && nstr[strlen(nstr)-1] == ')') {
    ptr = nstr + 1;
  } else {
    ptr = nstr;
  }

  for(; *ptr; ptr++) {
    if(n_braces > 0) {
      if(*ptr == ')') { // close brace
        str_char_append(&left_side, *ptr);
        n_braces--;
        continue;
      } else if(!isspace(*ptr)) { // ignore spaces
        str_char_append(&left_side, *ptr);
      }
      continue;
    }
    if(*ptr == '!') { // negation operator
      if(left_side.len) { // something already on the left side?
        free(nstr);
        free(cur_node);
        str_cleanup(&left_side);
        return NULL;
      }
      break;
    } else if(*ptr == '(') { // open brace
      str_char_append(&left_side, *ptr);
      n_braces++;
      continue;
    } else if(*ptr == '&' || *ptr == '|') {
      break;
    } else if(!isalpha(*ptr) && !isspace(*ptr) && *ptr != '_') { // only letters and underscore allowed
      free(nstr);
      free(cur_node);
      str_cleanup(&left_side);
      return NULL;
    } else if(isspace(*ptr)) { // ignore spaces
      continue;
    }
    str_char_append(&left_side, *ptr);
  }

  cur_node->parent = NULL;

  // end of string => no operator found
  if(!*ptr) {
    // this is a value
    if(!left_side.len) { // parse error
      free(nstr);
      free(cur_node);
      str_cleanup(&left_side);
      return NULL;
    }

    //(value) => remove braces
    if(left_side.content[0] == '(' && left_side.content[left_side.len-1] == ')') {
      if(left_side.len == 2) { // "()" => parse error
        free(nstr);
        free(cur_node);
        str_cleanup(&left_side);
        return NULL;
      }
      buf = strdup(left_side.content);
      str_char_set(&left_side, buf + 1, left_side.len - 2);
      free(buf);
    }

    cur_node->type = MACRO_NODE_VALUE;
    cur_node->left = NULL;
    cur_node->right = NULL;
    cur_node->data.ptr_data = fo_alloc(NULL,1,sizeof(string_t),FO_ALLOC_MALLOC);
    str_init((string_t *)cur_node->data.ptr_data);
    str_str_set((string_t *)cur_node->data.ptr_data, &left_side);
    str_cleanup(&left_side);
    free(nstr);

    return cur_node;
  }

  op = *ptr++;

  cur_node->type = MACRO_NODE_OPERATOR;

  switch(op) {
    case '!':
      str_cleanup(&left_side);
      cur_node->data.int_data = MACRO_OP_NOT;
      cur_node->left = NULL;
      cur_node->right = flt_urlrewrite_parse_macro((const u_char *)ptr);
      if(!cur_node->right) {
        free(nstr);
        free(cur_node);
        return NULL;
      }
      cur_node->right->parent = cur_node;
      break;
    case '&':
      cur_node->data.int_data = MACRO_OP_AND;
      cur_node->left = flt_urlrewrite_parse_macro((const u_char *)left_side.content);
      str_cleanup(&left_side);
      if(!cur_node->left) {
        free(nstr);
        free(cur_node);
        return NULL;
      }
      cur_node->left->parent = cur_node;
      cur_node->right = flt_urlrewrite_parse_macro((const u_char *)ptr);
      if(!cur_node->right) {
      free(cur_node->left);
        free(nstr);
        free(cur_node);
        return NULL;
      }
      cur_node->right->parent = cur_node;
      break;
    case '|':
      cur_node->data.int_data = MACRO_OP_OR;
      cur_node->left = flt_urlrewrite_parse_macro((const u_char *)left_side.content);
      str_cleanup(&left_side);
      if(!cur_node->left) {
        free(nstr);
        free(cur_node);
        return NULL;
      }
      cur_node->left->parent = cur_node;
      cur_node->right = flt_urlrewrite_parse_macro((const u_char *)ptr);
      if(!cur_node->right) {
        free(cur_node->left);
        free(nstr);
        free(cur_node);
        return NULL;
      }
      cur_node->right->parent = cur_node;
      break;
    default:
      str_cleanup(&left_side);
      break;
  }

  free(nstr);
  return cur_node;
}

int flt_urlrewrite_execute(configuration_t *fdc,configuration_t *fvc,const u_char *uri,u_char **new_uri) {
  unsigned int i, j, f;
  int res;
  int nbr;
  u_char buf[3];
  u_char *ptr;
  string_t dest;
  size_t len = 0;
  flt_urlrewrite_rule_t *current_rule = NULL;

  if(!flt_urlrewrite_rules || !flt_urlrewrite_rules->elements) {
    return FLT_DECLINE;
  }
  
  for(i = 0; i < flt_urlrewrite_rules->elements; i++) {
    current_rule = array_element_at(flt_urlrewrite_rules,i);
    if(!flt_urlrewrite_is_macro_true(current_rule->macro_tree)) {
      continue;
    }
    res = pcre_exec(current_rule->regexp, current_rule->regexp_extra, uri, strlen(uri), 0, 0, current_rule->match_arr,(current_rule->match_count + 1) * 3);
    if(res >= 0) { // didn't match
      break; // matched
    }
  }

  if(i == flt_urlrewrite_rules->elements) { // nothing matched
    return FLT_DECLINE;
  }
  
  str_init (&dest);
  len = strlen(current_rule->replacement);
  for(j = 0; j < len; j++) {
    f = 0;
    if(current_rule->replacement[j] == '$') { // replacement varialbe
      if(current_rule->replacement[j+1] == '{') {
        if(isdigit(current_rule->replacement[j+2])) {
          if(isdigit(current_rule->replacement[j+3]) && current_rule->replacement[j+4] == '}') {
            strncpy(buf, &current_rule->replacement[j+2], 2);
            buf[2] = 0;
            f = 4;
          }
          else if(current_rule->replacement[j+3] == '}') {
            strncpy(buf, &current_rule->replacement[j+2], 1);
            buf[1] = 0;
            f = 3;
          }
        }
      }
      else if(isdigit(current_rule->replacement[j+1])) {
        if(isdigit(current_rule->replacement[j+2])) {
          strncpy(buf, &current_rule->replacement[j+1], 2);
          buf[2] = 0;
          f = 2;
        } else {
          strncpy(buf, &current_rule->replacement[j+1], 1);
          buf[1] = 0;
          f = 1;
        }
      }
      if(f) {
        nbr = atoi(buf);
        res = pcre_get_substring(uri,current_rule->match_arr,(current_rule->match_count + 1) * 3,nbr,(const char **)&ptr);
        // error => ignore
        if(res < 0) {
          str_char_append(&dest, current_rule->replacement[j]);
          continue;
        }
        str_chars_append(&dest, ptr, res);
        pcre_free_substring(ptr);
        j += f;
        continue;
      }
    }
    str_char_append(&dest, current_rule->replacement[j]);
  }
  
  // if the new string has not got a length
  if(!dest.len) {
    // make sure there's a buffer
    str_char_append(&dest, '\0');
  }
  
  *new_uri = dest.content;
  free((void *)uri);

  return FLT_EXIT;
}

void flt_urlrewrite_destroy(flt_urlrewrite_rule_t *rule) {
  pcre_free(rule->regexp_extra);
  pcre_free(rule->regexp);
  flt_urlrewrite_free_macro_tree(rule->macro_tree);
  free((void *)rule->replacement);
  free(rule->match_arr);
}

int flt_urlrewrite_handle(configfile_t *cfile,conf_opt_t *opt,const u_char *context,u_char **args,size_t argnum) {
  flt_urlrewrite_rule_t n_rewrite;
  const u_char *error;
  int err_offset;
  
  if(flt_urlrewrite_fname == NULL) flt_urlrewrite_fname = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(context,flt_urlrewrite_fname) != 0) return 0;
  
  if(argnum != 3) {
    return -1;
  }
  
  if(!flt_urlrewrite_rules) {
    flt_urlrewrite_rules = fo_alloc(NULL,sizeof(array_t),1,FO_ALLOC_MALLOC);
    array_init(flt_urlrewrite_rules,sizeof(flt_urlrewrite_rule_t),(void(*)(void *))flt_urlrewrite_destroy);
  }
  
  n_rewrite.macro_tree = flt_urlrewrite_parse_macro(args[2]);
  if(!n_rewrite.macro_tree) {
    return -1;
  }

  n_rewrite.replacement = strdup(args[1]);
  if(!n_rewrite.replacement) {
    flt_urlrewrite_free_macro_tree(n_rewrite.macro_tree);
  }

  n_rewrite.regexp = pcre_compile(args[0], 0, (const char **)&error, &err_offset, NULL);
  if(!n_rewrite.regexp) {
    fprintf(stderr,"flt_urlrewrite: Regexp error with \"%s\": %s\n", args[0], error);
    flt_urlrewrite_free_macro_tree(n_rewrite.macro_tree);
    free((void *)n_rewrite.replacement);
    return -1;
  }
  n_rewrite.regexp_extra = pcre_study(n_rewrite.regexp, 0, (const char **)&error);
  if(error) {
    fprintf(stderr,"Regexp study error with \"%s\": %s\n", args[0], error);
    pcre_free(n_rewrite.regexp);
    flt_urlrewrite_free_macro_tree(n_rewrite.macro_tree);
    free((void *)n_rewrite.replacement);
    return -1;
  }
  n_rewrite.match_count = 0;
  pcre_fullinfo(n_rewrite.regexp, n_rewrite.regexp_extra, PCRE_INFO_CAPTURECOUNT, &(n_rewrite.match_count));
  n_rewrite.match_arr = (int *)fo_alloc(NULL, sizeof(int),(n_rewrite.match_count + 1) * 3, FO_ALLOC_MALLOC);
  if(!n_rewrite.match_arr) {
    pcre_free(n_rewrite.regexp_extra);
    pcre_free(n_rewrite.regexp);
    flt_urlrewrite_free_macro_tree(n_rewrite.macro_tree);
    free((void *)n_rewrite.replacement);
    return -1;
  }

  array_push(flt_urlrewrite_rules,&n_rewrite);

  return 0;
}

void flt_urlrewrite_cleanup(void) {
  if(flt_urlrewrite_rules) {
    array_destroy(flt_urlrewrite_rules);
    free(flt_urlrewrite_rules);
  }
}

conf_opt_t flt_urlrewrite_config[] = {
  { "URLRewrite",  flt_urlrewrite_handle, CFG_OPT_CONFIG|CFG_OPT_LOCAL, NULL },
  { NULL, NULL, 0, NULL }
};

handler_config_t flt_urlrewrite_handlers[] = {
  { URL_REWRITE_HANDLER, flt_urlrewrite_execute },
  { 0, NULL }
};

module_config_t flt_urlrewrite = {
  MODULE_MAGIC_COOKIE,
  flt_urlrewrite_config,
  flt_urlrewrite_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  flt_urlrewrite_cleanup
};

/* eof */
