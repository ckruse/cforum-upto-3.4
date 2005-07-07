/**
 * \file helpers.c
 * \author Christian Seiler, <self@christian-seiler.de>
 *
 * Helper functions for the template parser
 */

#include "config.h"
#include "defines.h"

#include "utils.h"
#include "hashlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "parsetpl.h"

/* {{{ init_context */
void init_context(t_context *context) {
  // set everything to zero
  memset(context,0,sizeof(*context));
  // initialize variables
  str_init(&context->output);
  str_init(&context->output_mem);
  array_init(&context->foreach_var_stack,sizeof(t_string),(void(*)(void *))str_cleanup);
  array_init(&context->if_level_stack,sizeof(int),NULL);
  context->function = NULL;
}
/* }}} */

/* {{{ destroy_context */
void destroy_context(t_context *context) {
  str_cleanup(&context->output);
  str_cleanup(&context->output_mem);
  array_destroy(&context->foreach_var_stack);
  array_destroy(&context->if_level_stack);
}
/* }}} */

/* {{{ init functions */
void init_function(t_function *func) {
  memset(func,0,sizeof(*func));
  str_init(&func->name);
  array_init(&func->params,sizeof(t_string),(void(*)(void *))str_cleanup);
  func->ctx = fo_alloc(NULL,sizeof(t_context),1,FO_ALLOC_MALLOC);
  init_context(func->ctx);
  func->ctx->function = func;
}
/* }}} */

/* {{{ destroy_function */
void destroy_function(void *arg) {
  t_function *func = (t_function *)arg;

  str_cleanup(&func->name);
  array_destroy(&func->params);
  destroy_context(func->ctx);
  free(func->ctx);
}
/* }}} */

/* {{{ destroy_token */
void destroy_token(void *t) {
  t_token *token = (t_token *)t;
  str_cleanup(token->data);
}
/* }}} */

/* {{{ append_escaped_string */
void append_escaped_string(t_string *dest,t_string *src) {
  size_t i;

  for(i = 0; i < src->len; i++) {
    if(src->content[i] == '\n') {
      str_char_append(dest,'\\');
      str_char_append(dest,'n');
      continue;
    }

    if(src->content[i] == '\x0D') {
      str_chars_append(dest,"\\x0D",4);
      continue;
    }

    if(src->content[i] == '\\' || src->content[i] == '"') str_char_append(dest,'\\');

    str_char_append(dest,src->content[i]);
  }
}
/* }}} */

/* eof */
