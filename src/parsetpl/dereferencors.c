/**
 * \file helpers.c
 * \author Christian Seiler, <self@christian-seiler.de>
 *
 * Helper functions for the template parser
 */

#include "cfconfig.h"
#include "defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "utils.h"
#include "hashlib.h"
#include "parsetpl.h"

/* {{{ dereference_variable */
int dereference_variable(cf_string_t *out_str,token_t *var,cf_array_t *data,cf_string_t *c_var) {
  int level, is_hash;
  char buf[20];
  cf_string_t *arrayidx;
  token_t *token;
  
  cf_str_cstr_append(out_str,"v = (cf_tpl_variable_t *)cf_tpl_getvar(tpl,\"");
  cf_str_chars_append(out_str,var->data->content+1,var->data->len-1);
  cf_str_cstr_append(out_str,"\");\n");
  token = NULL;
  level = 0;

  // get array indexes
  while(data->elements) {
    token = (token_t *)cf_array_element_at(data,0);
    if(token->type != PARSETPL_TOK_ARRAYSTART) break;

    token = (token_t *)cf_array_shift(data);
    destroy_token(token);
    free(token);

    if(!data->elements) return PARSETPL_ERR_INVALIDTAG;

    token = (token_t *)cf_array_shift(data);
    if((token->type != PARSETPL_TOK_STRING && token->type != PARSETPL_TOK_INTEGER) || !data->elements) {
      destroy_token(token);
      free(token);
      return PARSETPL_ERR_INVALIDTAG;
    }

    if(token->type == PARSETPL_TOK_INTEGER) is_hash = 0;
    else is_hash = 1;

    arrayidx = token->data; free(token);
    token = (token_t *)cf_array_shift(data);

    if(token->type != PARSETPL_TOK_ARRAYEND) {
      destroy_token(token);
      free(token);
      cf_str_cleanup(arrayidx);
      free(arrayidx);
      return PARSETPL_ERR_INVALIDTAG;
    }

    destroy_token(token);
    free(token);

    if(is_hash) {
      cf_str_cstr_append(out_str,"if(v && v->type == TPL_VARIABLE_HASH) {\n");
      cf_str_cstr_append(out_str,"v = (cf_tpl_variable_t *)cf_hash_get(v->data.d_hash,\"");
      append_escaped_string(out_str,arrayidx);
      cf_str_cstr_append(out_str,"\",");
      snprintf(buf,19,"%ld",arrayidx->len);
      cf_str_cstr_append(out_str,buf);
    }
    else {
      cf_str_cstr_append(out_str,"if(v && v->type == TPL_VARIABLE_ARRAY) {\n");
      cf_str_cstr_append(out_str,"v = (cf_tpl_variable_t *)cf_array_element_at(&v->data.d_array,");
      cf_str_str_append(out_str,arrayidx);
    }

    cf_str_cleanup(arrayidx);
    free(arrayidx);
    cf_str_cstr_append(out_str,");\n");
    level++;
  }

  cf_str_str_append(out_str,c_var);
  cf_str_cstr_append(out_str," = v;\n");

  for(;level > 0;level--) cf_str_cstr_append(out_str,"}\n");

  return 0;
}
/* }}} */

/* {{{ dereference_iterator */
int dereference_iterator(cf_string_t *out_str,token_t *var,cf_array_t *data,cf_string_t *c_var) {
  long idx = -1;
  token_t *token;
  char idxbuf[20];
  char idx2buf[20];

  if(strcmp(var->data->content,"@first") && strcmp(var->data->content,"@last") && strcmp(var->data->content,"@iterator")) return PARSETPL_ERR_INVALIDTAG;

  // get array indexes
  while(data->elements) {
    token = (token_t *)cf_array_element_at(data,0);
    if(token->type != PARSETPL_TOK_ARRAYSTART) break;

    token = (token_t *)cf_array_shift(data);
    destroy_token(token);
    free(token);

    if(!data->elements) return PARSETPL_ERR_INVALIDTAG;

    token = (token_t *)cf_array_shift(data);
    if(token->type != PARSETPL_TOK_INTEGER || !data->elements) {
      destroy_token(token);
      free(token);
      return PARSETPL_ERR_INVALIDTAG;
    }

    idx   = strtol(token->data->content,NULL,10); free(token);
    token = (token_t *)cf_array_shift(data);

    if(token->type != PARSETPL_TOK_ARRAYEND) {
      destroy_token(token);
      free(token);
      return PARSETPL_ERR_INVALIDTAG;
    }

    destroy_token(token); free(token);
    // only one loop pass!
    break;
  }
  
  if(!current_context->foreach_var_stack.elements) return PARSETPL_ERR_NOTINLOOP;
  if(idx >= 0 && current_context->foreach_var_stack.elements <= (size_t)idx) return PARSETPL_ERR_NOTINLOOP;
  if(idx < 0 && current_context->foreach_var_stack.elements < (size_t)-idx) return PARSETPL_ERR_NOTINLOOP;
  if(idx < 0) idx += current_context->foreach_var_stack.elements;

  snprintf(idxbuf,19,"%ld",idx);
  snprintf(idx2buf,19,"%ld",idx*2);

  if(!strcmp(var->data->content,"@first")) {
    cf_str_str_append(out_str,c_var);
    cf_str_cstr_append(out_str," = (i");
    cf_str_cstr_append(out_str,idxbuf);
    cf_str_cstr_append(out_str," == 0) ? 1 : 0;\n");
  }
  else if(!strcmp(var->data->content,"@last")) {
    cf_str_str_append(out_str,c_var);
    cf_str_cstr_append(out_str," = (i");
    cf_str_cstr_append(out_str,idxbuf);
    cf_str_cstr_append(out_str," == vf");
    cf_str_cstr_append(out_str,idx2buf);
    cf_str_cstr_append(out_str,"->data.d_array.elements - 1) ? 1 : 0;\n");
  }
  else if(!strcmp(var->data->content,"@iterator")) {
    cf_str_str_append(out_str,c_var);
    cf_str_cstr_append(out_str," = i");
    cf_str_cstr_append(out_str,idxbuf);
    cf_str_cstr_append(out_str,";\n");
  }

  return 0;
}
/* }}} */


/* eof */
