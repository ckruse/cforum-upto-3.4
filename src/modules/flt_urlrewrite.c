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

typedef struct s_macro_node {
   int type;
   union {
     void *ptr_data;
     int int_data;
   } data;

   struct s_macro_node *left,
                       *right,
                       *parent;

} t_macro_node;

typedef struct s_urlrewrite {
  pcre *regexp;
  pcre_extra *regexp_extra;
  t_macro_node *macro_tree;
  const u_char *replacement;
  unsigned long int match_count;
  int *match_arr;
} t_urlrewrite;

t_urlrewrite **UrlRewrites = NULL;
int UrlRewriteCount = 0;
int UrlRewriteSize = 0;

int is_macro_true(t_macro_node *tree) {
  t_string *data;
  int op_type;
  int *bool_value;

  if(!tree)
    return 0;

  switch(tree->type) {
    case MACRO_NODE_VALUE:
      data = (t_string *)tree->data.ptr_data;
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
          return !is_macro_true(tree->right);
        case MACRO_OP_AND:
          return is_macro_true(tree->left) && is_macro_true(tree->right);
        case MACRO_OP_OR:
          return is_macro_true(tree->left) || is_macro_true(tree->right);
        default:
          return 0;
      }
      break;
    default:
      return 0;
    }
  }
}

void free_macro_tree(t_macro_node *tree) {
  if(!tree)
    return;
  if(tree->left)
    free_macro_tree(tree->left);
  if(tree->right)
    free_macro_tree(tree->right);
  switch(tree->type) {
    case MACRO_NODE_VALUE:
      str_cleanup((t_string *)tree->data.ptr_data);
      free(tree->data.ptr_data);
      break;
    default:
      break;
  }
  free(tree);
}

t_macro_node *parse_macro(const u_char *str) {
  t_macro_node *cur_node;
  t_string left_side;
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

  cur_node = (t_macro_node *)fo_alloc(NULL,1,sizeof(t_macro_node),FO_ALLOC_MALLOC);
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
    cur_node->data.ptr_data = fo_alloc(NULL,1,sizeof(t_string),FO_ALLOC_MALLOC);
    str_init((t_string *)cur_node->data.ptr_data);
    str_str_set((t_string *)cur_node->data.ptr_data, &left_side);
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
      cur_node->right = parse_macro((const u_char *)ptr);
      if(!cur_node->right) {
        free(nstr);
        free(cur_node);
        return NULL;
      }
      cur_node->right->parent = cur_node;
      break;
    case '&':
      cur_node->data.int_data = MACRO_OP_AND;
      cur_node->left = parse_macro((const u_char *)left_side.content);
      str_cleanup(&left_side);
      if(!cur_node->left) {
        free(nstr);
        free(cur_node);
        return NULL;
      }
      cur_node->left->parent = cur_node;
      cur_node->right = parse_macro((const u_char *)ptr);
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
      cur_node->left = parse_macro((const u_char *)left_side.content);
      str_cleanup(&left_side);
      if(!cur_node->left) {
        free(nstr);
        free(cur_node);
        return NULL;
      }
      cur_node->left->parent = cur_node;
      cur_node->right = parse_macro((const u_char *)ptr);
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

int treat_link(t_string *dest, u_char *src) {
  int i, j;
  int res;
  int nbr;
  u_char buf[3];
  u_char *ptr;
  size_t len;

  for(i = 0; i < UrlRewriteCount; i++) {
    if(!is_macro_true(UrlRewrites[i]->macro_tree)) {
      continue;
    }
    res = pcre_exec(UrlRewrites[i]->regexp, UrlRewrites[i]->regexp_extra, src, strlen(src), 0, 0, UrlRewrites[i]->match_arr,(UrlRewrites[i]->match_count + 1) * 3);
    if(res >= 0) { // didn't match
      break; // matched
    }
  }

  if(i == UrlRewriteCount) { // nothing matched
    str_char_set(dest, src, strlen(src));
    return 0;
  }

  len = strlen(UrlRewrites[i]->replacement);
  for(j = 0; j < len; j++) {
    if(UrlRewrites[i]->replacement[j] == '$') { // replacement varialbe
      if(isdigit(UrlRewrites[i]->replacement[j+1])) {
        if(isdigit(UrlRewrites[i]->replacement[j+2])) {
          strncpy(buf, &UrlRewrites[i]->replacement[j+1], 2);
          buf[2] = 0;
        } else {
          strncpy(buf, &UrlRewrites[i]->replacement[j+1], 1);
          buf[1] = 0;
        }
        nbr = atoi(buf);
        res = pcre_get_substring(src, UrlRewrites[i]->match_arr,(UrlRewrites[i]->match_count + 1) * 3, nbr,(const char **)&ptr);
        // error => ignore
        if(res < 0) {
          str_char_append(dest, UrlRewrites[i]->replacement[j]);
          continue;
        }
        str_chars_append(dest, ptr, res);
        pcre_free_substring(ptr);
        if(isdigit(UrlRewrites[i]->replacement[j+2])) {
          j += 2;
        } else {
          j++;
        }
        continue;
      }
    }
    str_char_append(dest, UrlRewrites[i]->replacement[j]);
  }

  return 0;
}

int execute_filter(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_cl_thread *thread,t_cf_template *tpl) {
  const t_cf_tpl_variable *msg_content;
  t_string content, link;
  u_char *ptr, *ptr_end_attr, *ptr_end_tag, *ptr_end_elem, *ptr_link;
  int res;

  str_init(&content);

  // get old message(this plugin has to be called *after* flt_posting!
  msg_content = tpl_cf_getvar(tpl, "message");

  if(!msg_content) // is not set? do nothing.
    return FLT_OK;

  ptr = msg_content->data->content;

  for(; *ptr; ptr++) {
    if(!cf_strncmp(ptr, "<a href=\"", 9)) {
      str_init(&link);

      ptr += 9;
      ptr_end_attr = strchr(ptr, '"');
      ptr_end_tag = strchr(ptr_end_attr, '>');
      ptr_end_elem = strchr(ptr_end_tag, '<');

      // not found?
      if(!ptr_end_attr || !ptr_end_tag || !ptr_end_elem) {
        // ignore
        str_cleanup(&link);
        str_chars_append(&content, "<a href=\"", 9);
        continue;
      }

      ptr_link = strndup(ptr,(int)(ptr_end_attr - ptr));
      if(!ptr_link) {
        str_cleanup(&link);
        str_chars_append(&content, "<a href=\"", 9);
        continue;
      }

      res = treat_link(&link, ptr_link);
      free(ptr_link);

      if(res < 0) {
        str_cleanup(&link);
        str_chars_append(&content, "<a href=\"", 9);
        continue;
      }

      str_chars_append(&content, "<a href=\"", 9);
      str_str_append(&content, &link);
      str_chars_append(&content, ptr_end_attr,(int)(ptr_end_tag - ptr_end_attr + 1));
      str_str_append(&content, &link);

      ptr = ptr_end_elem - 1;

      str_cleanup(&link);
    } else {
      str_char_append(&content, *ptr);
    }
  }

  tpl_cf_setvar(tpl,"message", content.content, content.len, 0);
  str_cleanup(&content);

  return FLT_OK;
}


int add_rewriterule(t_configfile *cfile,t_conf_opt *opt,u_char **args,int argnum) {
  t_urlrewrite **n_rewrites;
  t_urlrewrite *n_rewrite;
  int alloc_type;
  const u_char *error;
  int err_offset;

  if(UrlRewriteCount + 1 > UrlRewriteSize) {
    if(!UrlRewriteSize) {
      alloc_type = FO_ALLOC_MALLOC;
    } else {
      alloc_type = FO_ALLOC_REALLOC;
    }
    UrlRewriteSize += DEFAULT_URLREWRITE_SIZE;
    n_rewrites = fo_alloc(UrlRewrites, sizeof(t_urlrewrite *), UrlRewriteSize, alloc_type);
    if(!n_rewrites) {
      return 1;
    }
    UrlRewrites = n_rewrites;
  }
  n_rewrite = fo_alloc(NULL, sizeof(t_urlrewrite), 1, FO_ALLOC_MALLOC);
  if(!n_rewrite)
    return 1;

  n_rewrite->macro_tree = parse_macro(args[2]);
  if(!n_rewrite->macro_tree) {
    free(n_rewrite);
    return 1;
  }

  n_rewrite->replacement = strdup(args[1]);
  if(!n_rewrite->replacement) {
    free_macro_tree(n_rewrite->macro_tree);
    free(n_rewrite);
  }

  n_rewrite->regexp = pcre_compile(args[0], 0, (const char **)&error, &err_offset, NULL);
  if(!n_rewrite->regexp) {
    fprintf(stderr,"flt_urlrewrite: Regexp error with \"%s\": %s\n", args[0], error);
    free_macro_tree(n_rewrite->macro_tree);
    free((void *)n_rewrite->replacement);
    free(n_rewrite);
    return 1;
  }
  n_rewrite->regexp_extra = pcre_study(n_rewrite->regexp, 0, (const char **)&error);
  if(error) {
    printf("Regexp study error with \"%s\": %s\n", args[0], error);
    pcre_free(n_rewrite->regexp);
    free_macro_tree(n_rewrite->macro_tree);
    free((void *)n_rewrite->replacement);
    free(n_rewrite);
    return 1;
  }
  n_rewrite->match_count = 0;
  pcre_fullinfo(n_rewrite->regexp, n_rewrite->regexp_extra, PCRE_INFO_CAPTURECOUNT, &(n_rewrite->match_count));
  n_rewrite->match_arr = (int *)fo_alloc(NULL, sizeof(int),(n_rewrite->match_count + 1) * 3, FO_ALLOC_MALLOC);
  if(!n_rewrite->match_arr) {
    pcre_free(n_rewrite->regexp_extra);
    pcre_free(n_rewrite->regexp);
    free_macro_tree(n_rewrite->macro_tree);
    free((void *)n_rewrite->replacement);
    free(n_rewrite);
  }

  UrlRewrites[UrlRewriteCount++] = n_rewrite;

  return 0;
}

void flt_urlrewrite_cleanup(void) {
  // cleanup stuff
  int i;

  for(i = 0; i < UrlRewriteCount; i++) {
    pcre_free(UrlRewrites[i]->regexp_extra);
    pcre_free(UrlRewrites[i]->regexp);
    free_macro_tree(UrlRewrites[i]->macro_tree);
    free((void *)UrlRewrites[i]->replacement);
    free(UrlRewrites[i]->match_arr);
    free(UrlRewrites[i]);
  }

  if(UrlRewrites)
    free(UrlRewrites);
}

t_conf_opt flt_urlrewrite_config[] = {
  { "URLRewrite",  add_rewriterule, CFG_OPT_CONFIG, NULL },
  { NULL, NULL, 0, NULL }
};

t_handler_config flt_urlrewrite_handlers[] = {
  { POSTING_HANDLER, execute_filter },
  { 0, NULL }
};

t_module_config flt_urlrewrite = {
  flt_urlrewrite_config,
  flt_urlrewrite_handlers,
  NULL,
  NULL,
  NULL,
  flt_urlrewrite_cleanup
};

/* eof */
