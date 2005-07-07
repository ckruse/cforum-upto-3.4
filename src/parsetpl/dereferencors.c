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

/* {{{ dereference_variable */
int dereference_variable(t_string *out_str,t_token *var,t_array *data,t_string *c_var) {
  int level, is_hash;
  char buf[20];
  t_string *arrayidx;
  t_token *token;
  
  str_cstr_append(out_str,"v = (t_cf_tpl_variable *)cf_tpl_getvar(tpl,\"");
  str_chars_append(out_str,var->data->content+1,var->data->len-1);
  str_cstr_append(out_str,"\");\n");
  token = NULL;
  level = 0;

  // get array indexes
  while(data->elements) {
    token = (t_token *)array_element_at(data,0);
    if(token->type != PARSETPL_TOK_ARRAYSTART) break;

    token = (t_token *)array_shift(data);
    destroy_token(token);
    free(token);

    if(!data->elements) return PARSETPL_ERR_INVALIDTAG;

    token = (t_token *)array_shift(data);
    if((token->type != PARSETPL_TOK_STRING && token->type != PARSETPL_TOK_INTEGER) || !data->elements) {
      destroy_token(token);
      free(token);
      return PARSETPL_ERR_INVALIDTAG;
    }

    if(token->type == PARSETPL_TOK_INTEGER) is_hash = 0;
    else is_hash = 1;

    arrayidx = token->data; free(token);
    token = (t_token *)array_shift(data);

    if(token->type != PARSETPL_TOK_ARRAYEND) {
      destroy_token(token);
      free(token);
      str_cleanup(arrayidx);
      free(arrayidx);
      return PARSETPL_ERR_INVALIDTAG;
    }

    destroy_token(token);
    free(token);

    if(is_hash) {
      str_cstr_append(out_str,"if(v && v->type == TPL_VARIABLE_HASH) {\n");
      str_cstr_append(out_str,"v = (t_cf_tpl_variable *)cf_hash_get(v->data.d_hash,\"");
      append_escaped_string(out_str,arrayidx);
      str_cstr_append(out_str,"\",");
      snprintf(buf,19,"%ld",arrayidx->len);
      str_cstr_append(out_str,buf);
    }
    else {
      str_cstr_append(out_str,"if(v && v->type == TPL_VARIABLE_ARRAY) {\n");
      str_cstr_append(out_str,"v = (t_cf_tpl_variable *)array_element_at(&v->data.d_array,");
      str_str_append(out_str,arrayidx);
    }

    str_cleanup(arrayidx);
    free(arrayidx);
    str_cstr_append(out_str,");\n");
    level++;
  }

  str_str_append(out_str,c_var);
  str_cstr_append(out_str," = v;\n");

  for(;level > 0;level--) str_cstr_append(out_str,"}\n");

  return 0;
}
/* }}} */

/* {{{ dereference_iterator */
int dereference_iterator(t_string *out_str,t_token *var,t_array *data,t_string *c_var) {
  long idx = -1;
  t_token *token;
  char idxbuf[20];
  char idx2buf[20];

  if(strcmp(var->data->content,"@first") && strcmp(var->data->content,"@last") && strcmp(var->data->content,"@iterator")) return PARSETPL_ERR_INVALIDTAG;

  // get array indexes
  while(data->elements) {
    token = (t_token *)array_element_at(data,0);
    if(token->type != PARSETPL_TOK_ARRAYSTART) break;

    token = (t_token *)array_shift(data);
    destroy_token(token);
    free(token);

    if(!data->elements) return PARSETPL_ERR_INVALIDTAG;

    token = (t_token *)array_shift(data);
    if(token->type != PARSETPL_TOK_INTEGER || !data->elements) {
      destroy_token(token);
      free(token);
      return PARSETPL_ERR_INVALIDTAG;
    }

    idx   = strtol(token->data->content,NULL,10); free(token);
    token = (t_token *)array_shift(data);

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
    str_str_append(out_str,c_var);
    str_cstr_append(out_str," = (i");
    str_cstr_append(out_str,idxbuf);
    str_cstr_append(out_str," == 0) ? 1 : 0;\n");
  }
  else if(!strcmp(var->data->content,"@last")) {
    str_str_append(out_str,c_var);
    str_cstr_append(out_str," = (i");
    str_cstr_append(out_str,idxbuf);
    str_cstr_append(out_str," == vf");
    str_cstr_append(out_str,idx2buf);
    str_cstr_append(out_str,"->data.d_array.elements - 1) ? 1 : 0;\n");
  }
  else if(!strcmp(var->data->content,"@iterator")) {
    str_str_append(out_str,c_var);
    str_cstr_append(out_str," = i");
    str_cstr_append(out_str,idxbuf);
    str_cstr_append(out_str,";\n");
  }

  return 0;
}
/* }}} */


/* eof */
