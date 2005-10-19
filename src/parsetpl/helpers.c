/**
 * \file helpers.c
 * \author Christian Seiler, <self@christian-seiler.de>
 *
 * Helper functions for the template parser
 */

#include "config.h"
#include "defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "utils.h"
#include "hashlib.h"
#include "parsetpl.h"

/* {{{ init_context */
void init_context(context_t *context) {
  // set everything to zero
  memset(context,0,sizeof(*context));
  // initialize variables
  cf_str_init(&context->output);
  cf_str_init(&context->output_mem);
  cf_array_init(&context->foreach_var_stack,sizeof(cf_string_t),(void(*)(void *))cf_str_cleanup);
  cf_array_init(&context->if_level_stack,sizeof(int),NULL);
  context->function = NULL;
}
/* }}} */

/* {{{ destroy_context */
void destroy_context(context_t *context) {
  cf_str_cleanup(&context->output);
  cf_str_cleanup(&context->output_mem);
  cf_array_destroy(&context->foreach_var_stack);
  cf_array_destroy(&context->if_level_stack);
}
/* }}} */

/* {{{ init functions */
void init_function(function_t *func) {
  memset(func,0,sizeof(*func));
  cf_str_init(&func->name);
  cf_array_init(&func->params,sizeof(cf_string_t),(void(*)(void *))cf_str_cleanup);
  func->ctx = cf_alloc(NULL,sizeof(context_t),1,CF_ALLOC_MALLOC);
  init_context(func->ctx);
  func->ctx->function = func;
}
/* }}} */

/* {{{ destroy_function */
void destroy_function(void *arg) {
  function_t *func = (function_t *)arg;

  cf_str_cleanup(&func->name);
  cf_array_destroy(&func->params);
  destroy_context(func->ctx);
  free(func->ctx);
}
/* }}} */

/* {{{ destroy_token */
void destroy_token(void *t) {
  token_t *token = (token_t *)t;
  cf_str_cleanup(token->data);
}
/* }}} */

/* {{{ append_escaped_string */
void append_escaped_string(cf_string_t *dest,cf_string_t *src) {
  size_t i;

  for(i = 0; i < src->len; i++) {
    if(src->content[i] == '\n') {
      cf_str_char_append(dest,'\\');
      cf_str_char_append(dest,'n');
      continue;
    }

    if(src->content[i] == '\x0D') {
      cf_str_chars_append(dest,"\\x0D",4);
      continue;
    }

    if(src->content[i] == '\\' || src->content[i] == '"') cf_str_char_append(dest,'\\');

    cf_str_char_append(dest,src->content[i]);
  }
}
/* }}} */

/* eof */
