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

/* {{{ process_array_assignment */
int process_array_assignment(array_t *data,string_t *tmp) {
  token_t *token;
  string_t varn;
  long v1n, v2n;
  int had_sep, ret;
  char buf[20];
  char v1nb[20],v2nb[20];
  int n_elems, is_hash, is_hval, nt, is_concat, jh_concat;
  string_t *hkey = NULL;
  
  v1n = current_context->n_cur_assign_vars++;
  snprintf(v1nb,19,"%ld",v1n);
  v2n = current_context->n_cur_assign_vars;

  if(current_context->n_cur_assign_vars > current_context->n_assign_vars) current_context->n_assign_vars = current_context->n_cur_assign_vars;

  snprintf(v2nb,19,"%ld",v2n);
  
  
  if((is_hash = peek_for_hash(data)) == -1) { // invalid tag
    --current_context->n_cur_assign_vars;
    return PARSETPL_ERR_INVALIDTAG;
  }
  
  str_cstr_append(tmp,"va");
  str_cstr_append(tmp,v1nb);
  str_cstr_append(tmp," = fo_alloc(NULL,sizeof(cf_tpl_variable_t),1,FO_ALLOC_MALLOC);\n");
  str_cstr_append(tmp,"cf_tpl_var_init(va");
  str_cstr_append(tmp,v1nb);

  if(is_hash) str_cstr_append(tmp,",TPL_VARIABLE_HASH);\n");
  else str_cstr_append(tmp,",TPL_VARIABLE_ARRAY);\n");

  had_sep = 1;
  n_elems = 0;
  is_hval = 0;
  is_concat = 0;
  jh_concat = 0;
  
  while(data->elements) {
    token = (token_t *)array_shift(data);
    if(token->type == PARSETPL_TOK_WHITESPACE) {
      destroy_token(token);
      free(token);
      continue;
    }

    if(had_sep && token->type != PARSETPL_TOK_ARRAYSTART && token->type != PARSETPL_TOK_INTEGER && token->type != PARSETPL_TOK_STRING && token->type != PARSETPL_TOK_VARIABLE && token->type != PARSETPL_TOK_LOOPVAR) {
      destroy_token(token);
      free(token);

      if(is_hval) {
        str_cleanup(hkey);
        free(hkey);
      }

      --current_context->n_cur_assign_vars;
      return PARSETPL_ERR_INVALIDTAG;
    }

    if(!jh_concat && !had_sep && !is_hval && token->type != PARSETPL_TOK_ARRAYEND && token->type != PARSETPL_TOK_ARRAYSEP) {
      destroy_token(token);
      free(token);

      if(is_hval) {
        str_cleanup(hkey);
        free(hkey);
      }

      --current_context->n_cur_assign_vars;
      return PARSETPL_ERR_INVALIDTAG;
    }

    jh_concat = 0;

    if(token->type == PARSETPL_TOK_ARRAYEND) {
      destroy_token(token);
      free(token);

      if(is_hval) {
        str_cleanup(hkey);
        free(hkey);
        --current_context->n_cur_assign_vars;
        return PARSETPL_ERR_INVALIDTAG;
      }

      break;
    }

    if(token->type == PARSETPL_TOK_ARRAYSEP) {
      destroy_token(token);
      free(token);

      if(is_hval) {
        str_cleanup(hkey);
        free(hkey);
        --current_context->n_cur_assign_vars;
        return PARSETPL_ERR_INVALIDTAG;
      }

      had_sep = 1;
      continue;
    }

    had_sep = 0;

    if(token->type == PARSETPL_TOK_STRING) {
      // peek to see if this is a concatentation or a hash assignment
      if((nt = peek_next_nws_type(data)) == PARSETPL_TOK_CONCAT || is_concat) {
        // ok, we need to concatenate a string
        current_context->uses_tmpstring = 1;

        if(!is_concat) {
          str_cstr_append(tmp,"str_init(&tmp_string);");
          is_concat = 1;
        }

        str_cstr_append(tmp,"str_chars_append(&tmp_string,\"");
        append_escaped_string(tmp,token->data);
        str_cstr_append(tmp,"\",");
        snprintf(buf,19,"%ld",token->data->len);
        str_cstr_append(tmp,buf);
        str_cstr_append(tmp,");\n");

        if(nt == PARSETPL_TOK_CONCAT) {
          // we already know that there will be a token of this type
          while(token->type != PARSETPL_TOK_CONCAT) {
            destroy_token(token); free(token);
            token = (token_t *)array_shift(data);
          }

          jh_concat = 1;
        }
        else {
          // end of concatenations
          if(is_hash && !is_hval) {
            destroy_token(token);
            free(token);
            --current_context->n_cur_assign_vars;
            return PARSETPL_ERR_INVALIDTAG;
          }

          if(is_hval) {
            str_cstr_append(tmp,"cf_tpl_hashvar_setvalue(va");
            str_cstr_append(tmp,v1nb);
            str_cstr_append(tmp,",\"");
            append_escaped_string(tmp,hkey);
            str_cstr_append(tmp,"\",TPL_VARIABLE_STRING,tmp_string.content,tmp_string.len);\n");
            is_hval = 0;
            str_cleanup(hkey);
            free(hkey);
          }
          else {
            str_cstr_append(tmp,"cf_tpl_var_addvalue(va");
            str_cstr_append(tmp,v1nb);
            str_cstr_append(tmp,",TPL_VARIABLE_STRING,tmp_string.content,tmp_string.len);\n");
          }

          str_cstr_append(tmp,"str_cleanup(&tmp_string);\n");
          is_concat = 0;
          jh_concat = 0;
        }
      }
      else if(nt == PARSETPL_TOK_HASHASSIGNMENT) {
        // if we already had elements and this is no hash
        if(n_elems && !is_hash) {
          destroy_token(token);
          free(token);
          --current_context->n_cur_assign_vars;
          return PARSETPL_ERR_INVALIDTAG;
        }

        // if this is a concatenation (["bla". => ] or ["bla"."blub" => ])
        if(is_concat) {
          destroy_token(token);
          free(token);
          --current_context->n_cur_assign_vars;
          return PARSETPL_ERR_INVALIDTAG;
        }

        is_hash = 1;
        is_hval = 1;
        hkey = token->data;
        free(token);
        token = (token_t *)array_shift(data);

        // we already know that there will be a token of this type
        while(token->type != PARSETPL_TOK_HASHASSIGNMENT) {
          destroy_token(token); free(token);
          token = (token_t *)array_shift(data);
        }
      }
      else {
        if(is_hash && !is_hval) {
          destroy_token(token);
          free(token);
          --current_context->n_cur_assign_vars;
          return PARSETPL_ERR_INVALIDTAG;
        }

        if(is_hval) {
          str_cstr_append(tmp,"cf_tpl_hashvar_setvalue(va");
          str_cstr_append(tmp,v1nb);
          str_cstr_append(tmp,",\"");
          append_escaped_string(tmp,hkey);
          str_cstr_append(tmp,"\",TPL_VARIABLE_STRING,\"");
          append_escaped_string(tmp,token->data);
          str_cstr_append(tmp,"\",");
          snprintf(buf,19,"%ld",token->data->len);
          str_cstr_append(tmp,buf);
          str_cstr_append(tmp,");\n");
          is_hval = 0;
          str_cleanup(hkey);
          free(hkey);
        }
        else {
          str_cstr_append(tmp,"cf_tpl_var_addvalue(va");
          str_cstr_append(tmp,v1nb);
          str_cstr_append(tmp,",TPL_VARIABLE_STRING,\"");
          append_escaped_string(tmp,token->data);
          str_cstr_append(tmp,"\",");
          snprintf(buf,19,"%ld",token->data->len);
          str_cstr_append(tmp,buf);
          str_cstr_append(tmp,");\n");
        }
      }
    }
    else if(token->type == PARSETPL_TOK_INTEGER) {
      if(is_concat) {
        destroy_token(token);
        free(token);

        if(is_hval) {
          str_cleanup(hkey);
          free(hkey);
        }

        --current_context->n_cur_assign_vars;
        return PARSETPL_ERR_INVALIDTAG;
      }

      if(is_hval) {
        str_cstr_append(tmp,"cf_tpl_hashvar_setvalue(va");
        str_cstr_append(tmp,v1nb);
        str_cstr_append(tmp,",\"");
        append_escaped_string(tmp,hkey);
        str_cstr_append(tmp,"\",TPL_VARIABLE_INT,");
        str_str_append(tmp,token->data);
        str_cstr_append(tmp,");\n");
      }
      else {
        str_cstr_append(tmp,"cf_tpl_var_addvalue(va");
        str_cstr_append(tmp,v1nb);
        str_cstr_append(tmp,",TPL_VARIABLE_INT,");
        str_str_append(tmp,token->data);
        str_cstr_append(tmp,");\n");
      }
    }
    else if(token->type == PARSETPL_TOK_ARRAYSTART) {
      if(is_concat) {
        destroy_token(token); free(token);

        if(is_hval) {
          str_cleanup(hkey);
          free(hkey);
        }

        --current_context->n_cur_assign_vars;
        return PARSETPL_ERR_INVALIDTAG;
      }

      ret = process_array_assignment(data,tmp);

      if(ret < 0) {
        destroy_token(token); free(token);
        --current_context->n_cur_assign_vars;
        return ret;
      }

      if(is_hval) {
        str_cstr_append(tmp,"cf_tpl_hashvar_set(va");
        str_cstr_append(tmp,v1nb);
        str_cstr_append(tmp,",\"");
        append_escaped_string(tmp,hkey);
        str_cstr_append(tmp,"\",va");
        str_cstr_append(tmp,v2nb);
        str_cstr_append(tmp,");\n");
        is_hval = 0;
        str_cleanup(hkey);
        free(hkey);
      }
      else {
        str_cstr_append(tmp,"cf_tpl_var_add(va");
        str_cstr_append(tmp,v1nb);
        str_cstr_append(tmp,",va");
        str_cstr_append(tmp,v2nb);
        str_cstr_append(tmp,");\n");
      }

      str_cstr_append(tmp,"free(va");
      str_cstr_append(tmp,v2nb);
      str_cstr_append(tmp,");\n");
    }
    else if(token->type == PARSETPL_TOK_VARIABLE) {
      current_context->uses_clonevar = 1;
      str_cstr_append(tmp,"vc = NULL;\n");
      str_init(&varn);
      str_cstr_append(&varn,"vc");
      ret = dereference_variable(tmp,token,data,&varn);
      str_cleanup(&varn);

      if(ret < 0) {
        --current_context->n_cur_assign_vars;
        return ret;
      }

      // peek to see if this is a concatentation or a hash assignment
      if((nt = peek_next_nws_type(data)) == PARSETPL_TOK_CONCAT || is_concat) {
        // ok, we need to concatenate a string
        current_context->uses_tmpstring = 1;

        if(!is_concat) {
          str_cstr_append(tmp,"str_init(&tmp_string);");
          is_concat = 1;
        }

        str_cstr_append(tmp,"vc = cf_tpl_var_convert(NULL,vc,TPL_VARIABLE_STRING);\n");
        str_cstr_append(tmp,"if(vc) {\n");
        str_cstr_append(tmp,"str_str_append(&tmp_string,&vc->data.d_string);\n");
        str_cstr_append(tmp,"cf_tpl_var_destroy(vc);\n");
        str_cstr_append(tmp,"free(vc);\n");
        str_cstr_append(tmp,"}\n");

        if(nt == PARSETPL_TOK_CONCAT) {
          // we already know that there will be a token of this type
          while(token->type != PARSETPL_TOK_CONCAT) {
            destroy_token(token); free(token);
            token = (token_t *)array_shift(data);
          }

          jh_concat = 1;
        }
        else {
          // end of concatenations
          if(is_hash && !is_hval) {
            destroy_token(token); free(token);
            --current_context->n_cur_assign_vars;
            return PARSETPL_ERR_INVALIDTAG;
          }

          if(is_hval) {
            str_cstr_append(tmp,"cf_tpl_hashvar_setvalue(va");
            str_cstr_append(tmp,v1nb);
            str_cstr_append(tmp,",\"");
            append_escaped_string(tmp,hkey);
            str_cstr_append(tmp,"\",TPL_VARIABLE_STRING,tmp_string.content,tmp_string.len);\n");
            is_hval = 0;
            str_cleanup(hkey);
            free(hkey);
          }
          else {
            str_cstr_append(tmp,"cf_tpl_var_addvalue(va");
            str_cstr_append(tmp,v1nb);
            str_cstr_append(tmp,",TPL_VARIABLE_STRING,tmp_string.content,tmp_string.len);\n");
          }

          str_cstr_append(tmp,"str_cleanup(&tmp_string);\n");
          is_concat = 0;
          jh_concat = 0;
        }
      }
      else if(nt == PARSETPL_TOK_HASHASSIGNMENT) {
        // invalid
        destroy_token(token); free(token);
        --current_context->n_cur_assign_vars;
        return PARSETPL_ERR_INVALIDTAG;
      }
      else if(is_hval) {
        str_cstr_append(tmp,"vc = cf_tpl_var_clone(vc);\n");
        str_cstr_append(tmp,"if(!vc) {\n");
        str_cstr_append(tmp,"vc = (cf_tpl_variable_t *)fo_alloc(NULL,sizeof(cf_tpl_variable_t),1,FO_ALLOC_MALLOC);\n");
        str_cstr_append(tmp,"cf_tpl_var_init(vc,TPL_VARIABLE_INVALID);\n");
        str_cstr_append(tmp,"}\n");
        str_cstr_append(tmp,"cf_tpl_hashvar_set(va");
        str_cstr_append(tmp,v1nb);
        str_cstr_append(tmp,",\"");
        append_escaped_string(tmp,hkey);
        str_cstr_append(tmp,"\",vc);\n");
        str_cstr_append(tmp,"free(vc);\n");
        is_hval = 0;
        str_cleanup(hkey);
        free(hkey);
      }
      else {
        str_cstr_append(tmp,"vc = cf_tpl_var_clone(vc);\n");
        str_cstr_append(tmp,"if(!vc) {\n");
        str_cstr_append(tmp,"vc = (cf_tpl_variable_t *)fo_alloc(NULL,sizeof(cf_tpl_variable_t),1,FO_ALLOC_MALLOC);\n");
        str_cstr_append(tmp,"cf_tpl_var_init(vc,TPL_VARIABLE_INVALID);\n");
        str_cstr_append(tmp,"}\n");
        str_cstr_append(tmp,"cf_tpl_var_add(va");
        str_cstr_append(tmp,v1nb);
        str_cstr_append(tmp,",vc);\n");
        str_cstr_append(tmp,"free(vc);\n");
      }
    }
    else if(token->type == PARSETPL_TOK_LOOPVAR) {
      current_context->uses_loopassign = 1;
      str_cstr_append(tmp,"ic = 0;\n");
      str_init(&varn);
      str_cstr_append(&varn,"ic");
      ret = dereference_iterator(tmp,token,data,&varn);
      str_cleanup(&varn);

      if(ret < 0) {
        --current_context->n_cur_assign_vars;
        return ret;
      }

      if(is_hval) {
        str_cstr_append(tmp,"cf_tpl_hashvar_setvalue(va");
        str_cstr_append(tmp,v1nb);
        str_cstr_append(tmp,",\"");
        append_escaped_string(tmp,hkey);
        str_cstr_append(tmp,"\",TPL_VARIABLE_INT,ic);\n");
        is_hval = 0;
        str_cleanup(hkey);
        free(hkey);
      }
      else {
        str_cstr_append(tmp,"cf_tpl_var_addvalue(va");
        str_cstr_append(tmp,v1nb);
        str_cstr_append(tmp,",TPL_VARIABLE_INT,ic);\n");
      }
    }

    destroy_token(token); free(token);
    n_elems++;
  }

  --current_context->n_cur_assign_vars;
  return 0;
}
/* }}} */

/* {{{ process_variable_assignment_tag */
int process_variable_assignment_tag(token_t *variable,array_t *data) {
  string_t tmp;
  token_t *token = NULL;
  string_t varn;
  char buf[20];
  int ret,nt,is_concat = 0,n = 0;
  
  str_init(&tmp);
  
  // remove all whitespaces
  while(data->elements) {
    token = (token_t *)array_shift(data);
    if(token->type != PARSETPL_TOK_WHITESPACE) break;
  }

  do {
    if(token->type != PARSETPL_TOK_STRING && token->type != PARSETPL_TOK_INTEGER && token->type != PARSETPL_TOK_ARRAYSTART && token->type != PARSETPL_TOK_VARIABLE && token->type != PARSETPL_TOK_LOOPVAR) {
      str_cleanup(&tmp);
      destroy_token(token);
      free(token);
      return PARSETPL_ERR_INVALIDTAG;
    }

    if(n && !is_concat) {
      str_cleanup(&tmp);
      destroy_token(token);
      free(token);
      return PARSETPL_ERR_INVALIDTAG;
    }

    switch(token->type) {
      case PARSETPL_TOK_STRING:
        // peek to see if this is a concatentation or a hash assignment
        if((nt = peek_next_nws_type(data)) == PARSETPL_TOK_CONCAT || is_concat) {
          // ok, we need to concatenate a string
          current_context->uses_tmpstring = 1;

          if(!is_concat) {
            str_cstr_append(&tmp,"str_init(&tmp_string);");
            is_concat = 1;
          }

          str_cstr_append(&tmp,"str_chars_append(&tmp_string,\"");
          append_escaped_string(&tmp,token->data);
          str_cstr_append(&tmp,"\",");
          snprintf(buf,19,"%ld",token->data->len);
          str_cstr_append(&tmp,buf);
          str_cstr_append(&tmp,");\n");

          if(nt == PARSETPL_TOK_CONCAT) {
            // we already know that there will be a token of this type
            while(token->type != PARSETPL_TOK_CONCAT) {
              destroy_token(token); free(token);
              token = (token_t *)array_shift(data);
            }

            while(data->elements && ((token_t *)array_element_at(data,0))->type == PARSETPL_TOK_WHITESPACE) {
              destroy_token(token); free(token);
              token = (token_t *)array_shift(data);
            }
          }
          else {
            str_cstr_append(&tmp,"cf_tpl_setvalue(tpl,\"");
            str_chars_append(&tmp,variable->data->content+1,variable->data->len-1);
            str_cstr_append(&tmp,"\",TPL_VARIABLE_STRING,tmp_string.content,tmp_string.len);\n");
            str_cstr_append(&tmp,"str_cleanup(&tmp_string);\n");
            is_concat = 0;
          }
        }
        else {
          str_cstr_append(&tmp,"cf_tpl_setvalue(tpl,\"");
          str_chars_append(&tmp,variable->data->content+1,variable->data->len-1);
          str_cstr_append(&tmp,"\",TPL_VARIABLE_STRING,\"");
          append_escaped_string(&tmp,token->data);
          str_cstr_append(&tmp,"\",");
          snprintf(buf,19,"%ld",token->data->len);
          str_cstr_append(&tmp,buf);
          str_cstr_append(&tmp,");\n");
        }
        break;

      case PARSETPL_TOK_INTEGER:
        str_cstr_append(&tmp,"cf_tpl_setvalue(tpl,\"");
        str_chars_append(&tmp,variable->data->content+1,variable->data->len-1);
        str_cstr_append(&tmp,"\",TPL_VARIABLE_INT,");
        str_str_append(&tmp,token->data);
        str_cstr_append(&tmp,");\n");
        break;

      case PARSETPL_TOK_VARIABLE:
        current_context->uses_clonevar = 1;
        str_cstr_append(&tmp,"vc = NULL;\n");
        str_init(&varn);
        str_cstr_append(&varn,"vc");
        ret = dereference_variable(&tmp,token,data,&varn);
        str_cleanup(&varn);

        if(ret < 0) {
          str_cleanup(&tmp);
          return ret;
        }

        // peek to see if this is a concatentation or a hash assignment
        if((nt = peek_next_nws_type(data)) == PARSETPL_TOK_CONCAT || is_concat) {
          // ok, we need to concatenate a string
          current_context->uses_tmpstring = 1;

          if(!is_concat) {
            str_cstr_append(&tmp,"str_init(&tmp_string);");
            is_concat = 1;
          }

          str_cstr_append(&tmp,"vc = cf_tpl_var_convert(NULL,vc,TPL_VARIABLE_STRING);\n");
          str_cstr_append(&tmp,"if(vc) {\n");
          str_cstr_append(&tmp,"str_str_append(&tmp_string,&vc->data.d_string);\n");
          str_cstr_append(&tmp,"cf_tpl_var_destroy(vc);\n");
          str_cstr_append(&tmp,"free(vc);\n");
          str_cstr_append(&tmp,"}\n");

          if(nt == PARSETPL_TOK_CONCAT) {
            // we already know that there will be a token of this type
            while(token->type != PARSETPL_TOK_CONCAT) {
              destroy_token(token); free(token);
              token = (token_t *)array_shift(data);
            }

            while(data->elements && ((token_t *)array_element_at(data,0))->type == PARSETPL_TOK_WHITESPACE) {
              destroy_token(token); free(token);
              token = (token_t *)array_shift(data);
            }
          }
          else {
            str_cstr_append(&tmp,"cf_tpl_setvalue(tpl,\"");
            str_chars_append(&tmp,variable->data->content+1,variable->data->len-1);
            str_cstr_append(&tmp,"\",TPL_VARIABLE_STRING,tmp_string.content,tmp_string.len);\n");
            str_cstr_append(&tmp,"str_cleanup(&tmp_string);\n");
            is_concat = 0;
          }
        }
        else {
          str_cstr_append(&tmp,"vc = cf_tpl_var_clone(vc);\n");
          str_cstr_append(&tmp,"if(!vc) {\n");
          str_cstr_append(&tmp,"vc = (cf_tpl_variable_t *)fo_alloc(NULL,sizeof(cf_tpl_variable_t),1,FO_ALLOC_MALLOC);\n");
          str_cstr_append(&tmp,"cf_tpl_var_init(vc,TPL_VARIABLE_INVALID);\n");
          str_cstr_append(&tmp,"}\n");
          str_cstr_append(&tmp,"cf_tpl_setvar(tpl,\"");
          str_chars_append(&tmp,variable->data->content+1,variable->data->len-1);
          str_cstr_append(&tmp,"\",vc);\n");
          str_cstr_append(&tmp,"free(vc);\n");
        }
        break;

      case PARSETPL_TOK_LOOPVAR:
        current_context->uses_loopassign = 1;
        str_cstr_append(&tmp,"ic = 0;\n");
        str_init(&varn);
        str_cstr_append(&varn,"ic");
        ret = dereference_iterator(&tmp,token,data,&varn);
        str_cleanup(&varn);

        if(ret < 0) {
          destroy_token(token); free(token);
          str_cleanup(&tmp);
          return ret;
        }

        str_cstr_append(&tmp,"cf_tpl_setvalue(tpl,\"");
        str_chars_append(&tmp,variable->data->content+1,variable->data->len-1);
        str_cstr_append(&tmp,"\",TPL_VARIABLE_INT,ic);\n");
        break;

      case PARSETPL_TOK_ARRAYSTART:
        if((ret = process_array_assignment(data,&tmp)) < 0) {
          destroy_token(token);
          free(token);
          str_cleanup(&tmp);
          return PARSETPL_ERR_INVALIDTAG;
        }

        if(data->elements) {
          destroy_token(token);
          free(token);
          str_cleanup(&tmp);
          return PARSETPL_ERR_INVALIDTAG;
        }

        str_cstr_append(&tmp,"cf_tpl_setvar(tpl,\"");
        str_chars_append(&tmp,variable->data->content+1,variable->data->len-1);
        str_cstr_append(&tmp,"\",va0);\n");
        str_cstr_append(&tmp,"free(va0);\n");
        break;
    }

    destroy_token(token);
    free(token);

    if(data->elements) token = (token_t *)array_shift(data);
    else token = NULL;

    n++;
  } while(token);

  str_str_append(&current_context->output,&tmp);
  str_str_append(&current_context->output_mem,&tmp);

  return 0;
}
/* }}} */

/* {{{ process_variable_print_tag */
int process_variable_print_tag(token_t *variable,array_t *data) {
  string_t tmp;
  string_t c_var;
  token_t *token;
  int escape_html = 0;
  int ret;
  
  str_init(&tmp);
  str_init(&c_var);
  str_char_set(&c_var,"vp",2);
  str_cstr_append(&tmp,"vp = NULL;\n");
  ret = dereference_variable(&tmp,variable,data,&c_var);
  str_cleanup(&c_var);

  if(ret < 0) ret = 0;

  if(data->elements) {
    token = (token_t *)array_shift(data);

    if(token->type != PARSETPL_TOK_MODIFIER_ESCAPE || data->elements) {
      destroy_token(token); free(token);
      str_cleanup(&tmp);
      return PARSETPL_ERR_INVALIDTAG;
    }

    destroy_token(token); free(token);
    escape_html = 1;
  }

  str_cstr_append(&tmp,"if(vp) {\n");
  str_cstr_append(&tmp,"if(vp->type != TPL_VARIABLE_STRING) {\n");
  str_cstr_append(&tmp,"vp = cf_tpl_var_convert(NULL,vp,TPL_VARIABLE_STRING);\n}\n");
  str_cstr_append(&tmp,"if(vp && vp->type == TPL_VARIABLE_STRING) {\n");
  str_str_append(&current_context->output,&tmp);
  str_str_append(&current_context->output_mem,&tmp);
  str_cleanup(&tmp);
  
  if(escape_html) {
    str_cstr_append(&current_context->output,"print_htmlentities_encoded(vp->data.d_string.content,0,stdout);\n}\n}\n");
    str_cstr_append(&current_context->output_mem,"tmp = htmlentities(vp->data.d_string.content,0);\nstr_chars_append(&tpl->parsed,tmp,strlen(tmp));\nfree(tmp);\n}\n}\n");
  }
  else {
    str_cstr_append(&current_context->output,"my_write(vp->data.d_string.content);\n}\n}\n");
    str_cstr_append(&current_context->output_mem,"str_chars_append(&tpl->parsed,vp->data.d_string.content,vp->data.d_string.len);\n}\n}\n");
  }

  str_cstr_append(&current_context->output,"if(vp && vp->temporary) {\ncf_tpl_var_destroy(vp); free(vp);\n");
  str_cstr_append(&current_context->output_mem,"if(vp && vp->temporary) {\ncf_tpl_var_destroy(vp); free(vp);\n");
  str_cstr_append(&current_context->output,"}\n");
  str_cstr_append(&current_context->output_mem,"}\n");
  current_context->uses_print = 1;

  return 0;
}
/* }}} */

/* {{{ process_iterator_print_tag */
int process_iterator_print_tag (token_t *iterator,array_t *data) {
  string_t tmp;
  string_t c_var;
  int ret;
  
  str_init(&tmp);
  str_init(&c_var);
  str_char_set(&c_var,"iter_var",8);
  ret = dereference_iterator(&tmp,iterator,data,&c_var);
  str_cleanup(&c_var);

  if(ret < 0) {
    str_cleanup(&tmp);
    return ret;
  }

  if(data->elements) {
    str_cleanup(&tmp);
    return PARSETPL_ERR_INVALIDTAG;
  }

  str_str_append(&current_context->output,&tmp);
  str_str_append(&current_context->output_mem,&tmp);
  str_cleanup(&tmp);

  str_cstr_append(&current_context->output,"printf(\"%ld\",iter_var);\n");
  str_cstr_append(&current_context->output_mem,"snprintf(iter_buf,19,\"%ld\",iter_var);\n");
  str_cstr_append(&current_context->output_mem,"str_chars_append(&tpl->parsed,iter_buf,strlen(iter_buf));\n");
  current_context->uses_iter_print = 1;
  return 0;
}
/* }}} */

/* {{{ process_include_tag */
int process_include_tag(string_t *file) {
  string_t tmp;
  char buf[20];

  if(!strcmp(PARSETPL_INCLUDE_EXT,file->content+file->len-strlen(PARSETPL_INCLUDE_EXT))) {
    file->content[file->len-strlen(PARSETPL_INCLUDE_EXT)] = '\0';
    file->len -= strlen(PARSETPL_INCLUDE_EXT);
  }

  str_init(&tmp);
  str_chars_append(&tmp,"inc_filename = fo_alloc(NULL,sizeof(string_t),1,FO_ALLOC_MALLOC);\n",66);
  str_chars_append(&tmp,"inc_filepart = fo_alloc(NULL,sizeof(string_t),1,FO_ALLOC_MALLOC);\n",66);
  str_chars_append(&tmp,"inc_fileext = fo_alloc(NULL,sizeof(string_t),1,FO_ALLOC_MALLOC);\n",65);
  str_chars_append(&tmp,"inc_tpl = fo_alloc(NULL,sizeof(cf_template_t),1,FO_ALLOC_MALLOC);\n",66);
  str_chars_append(&tmp,"str_init(inc_filename);\n",24);
  str_chars_append(&tmp,"str_init(inc_filepart);\n",24);
  str_chars_append(&tmp,"str_init(inc_fileext);\n",23);
  str_chars_append(&tmp,"str_char_set(inc_filename,tpl->filename,strlen(tpl->filename));\n",64);
  str_chars_append(&tmp,"str_char_set(inc_filepart,\"",27);
  append_escaped_string(&tmp,&current_file);
  str_chars_append(&tmp,"\",",2);
  snprintf(buf,19,"%ld",current_file.len);
  str_chars_append(&tmp,buf,strlen(buf));
  str_chars_append(&tmp,");\n",3);
  str_chars_append(&tmp,"p = inc_filename->content+inc_filename->len-1;\n",47);
  str_chars_append(&tmp,"while(strncmp(p,inc_filepart->content,inc_filepart->len) && p > inc_filename->content) p--;\n",92);
  str_chars_append(&tmp,"if(!strncmp(p,inc_filepart->content,inc_filepart->len)) {\n",58);
  str_chars_append(&tmp,"*p = '\\0'; inc_filename->len = p - inc_filename->content;\n",58);
  str_chars_append(&tmp,"str_char_set(inc_fileext,p+inc_filepart->len,strlen(p+inc_filepart->len));\n",75);
  str_chars_append(&tmp,"str_chars_append(inc_filename,\"",31);
  append_escaped_string(&tmp,file);
  str_chars_append(&tmp,"\",",2);
  snprintf(buf,19,"%ld",file->len);
  str_chars_append(&tmp,buf,strlen(buf));  
  str_chars_append(&tmp,");\n",3);
  str_chars_append(&tmp,"str_str_append(inc_filename,inc_fileext);\n",42);
  str_chars_append(&tmp,"ret = cf_tpl_init(inc_tpl,inc_filename->content);\n",50);
  str_chars_append(&tmp,"if(!ret) {\n",11);
  // evil, we copy the varlist - but i don't have a better idea that doesn't cost much code
  str_chars_append(&tmp,"ov = inc_tpl->varlist;\n",23);
  str_chars_append(&tmp,"inc_tpl->varlist = tpl->varlist;\n",33);
  str_str_append(&current_context->output,&tmp);
  str_str_append(&current_context->output_mem,&tmp);
  str_chars_append(&current_context->output,"cf_tpl_parse(inc_tpl);\n",23);
  str_chars_append(&current_context->output_mem,"cf_tpl_parse_to_mem(inc_tpl);\n",30);
  str_chars_append(&current_context->output_mem,"if(inc_tpl->parsed.len) str_str_append(&tpl->parsed,&inc_tpl->parsed);\n",71);
  str_cleanup(&tmp);
  str_init(&tmp);
  str_chars_append(&tmp,"inc_tpl->varlist = ov;\n",23);
  str_chars_append(&tmp,"}\n",2);
  str_chars_append(&tmp,"cf_tpl_finish(inc_tpl);\n",24);
  str_chars_append(&tmp,"}\n",2);
  str_chars_append(&tmp,"str_cleanup(inc_filepart);\n",27);
  str_chars_append(&tmp,"str_cleanup(inc_filename);\n",27);
  str_chars_append(&tmp,"str_cleanup(inc_fileext);\n",26);
  str_chars_append(&tmp,"free(inc_tpl);\n",15);
  str_chars_append(&tmp,"free(inc_filepart);\n",20);
  str_chars_append(&tmp,"free(inc_filename);\n",20);
  str_chars_append(&tmp,"free(inc_fileext);\n",19);
  str_str_append(&current_context->output,&tmp);
  str_str_append(&current_context->output_mem,&tmp);
  current_context->uses_include = 1;

  return 0;
}
/* }}} */

/* {{{ process_foreach_tag */
int process_foreach_tag(array_t *data) {
  string_t tmp,vs,varn;
  token_t *token,*var2;
  string_t *tvs;
  long v1n, v2n, ivn;
  char v1nb[20],v2nb[20],ivnb[20];
  int ret;
  
  token = (token_t*)array_shift(data);
  if(token->type == PARSETPL_TOK_ENDFOREACH) {
    destroy_token(token);
    free(token);

    if(data->elements) return PARSETPL_ERR_INVALIDTAG;
    if(!current_context->foreach_var_stack.elements) return PARSETPL_ERR_INVALIDTAG; // impossible

    tvs = (string_t*)array_pop(&current_context->foreach_var_stack);
    ivn = --current_context->n_cur_foreach_vars;
    v1n = ivn*2;
    v2n = v1n + 1;
    snprintf(v1nb,19,"%ld",v1n);
    snprintf(v2nb,19,"%ld",v2n);
    snprintf(ivnb,19,"%ld",ivn);

    // make sure that the variable is invalid because the memory structure will be freed by cf_tpl_setvar
    // please note that getvar IS necessary because we can NOT use the old value of vf3 since that will
    // overwrite the *ORIGINAL* template variable (array member of vf2)
    str_init(&tmp);
    str_cstr_append(&tmp,"}\n}\n");
    str_str_append(&current_context->output,&tmp);
    str_str_append(&current_context->output_mem,&tmp);
    str_cleanup(&tmp);
    str_cleanup(tvs);
    free(tvs);
  }
  else if(token->type == PARSETPL_TOK_FOREACH) {
    destroy_token(token);
    free(token);

    if(!data->elements) return PARSETPL_ERR_INVALIDTAG;

    token = (token_t*)array_shift(data);
    if(token->type != PARSETPL_TOK_WHITESPACE || !data->elements) {
      destroy_token(token);
      free(token);
      return PARSETPL_ERR_INVALIDTAG;
    }

    while(token->type == PARSETPL_TOK_WHITESPACE) {
      destroy_token(token);
      free(token);

      if(!data->elements) return PARSETPL_ERR_INVALIDTAG;

      token = (token_t*)array_shift(data);
    }

    if(token->type != PARSETPL_TOK_VARIABLE || !data->elements) {
      destroy_token(token);
      free(token);
      return PARSETPL_ERR_INVALIDTAG;
    }

    ivn = current_context->n_cur_foreach_vars++;
    v1n = ivn*2;
    v2n = v1n + 1;

    if(current_context->n_cur_foreach_vars > current_context->n_foreach_vars) current_context->n_foreach_vars = current_context->n_cur_foreach_vars;

    str_init(&tmp);
    snprintf(v1nb,19,"%ld",v1n);
    snprintf(v2nb,19,"%ld",v2n);
    snprintf(ivnb,19,"%ld",ivn);
    str_init(&varn);
    str_cstr_append(&tmp,"vf");
    str_cstr_append(&tmp,v1nb);
    str_cstr_append(&tmp," = NULL;\n");
    str_cstr_append(&varn,"vf");
    str_cstr_append(&varn,v1nb);
    ret = dereference_variable(&tmp,token,data,&varn);
    str_cleanup(&varn);
    destroy_token(token);
    free(token);

    if(ret < 0) {
      str_cleanup(&tmp);
      current_context->n_cur_foreach_vars--;
      return PARSETPL_ERR_INVALIDTAG;
    }

    token = (token_t*)array_shift(data);
    if(token->type != PARSETPL_TOK_WHITESPACE || !data->elements) {
      destroy_token(token);
      free(token);
      str_cleanup(&tmp);
      current_context->n_cur_foreach_vars--;
      return PARSETPL_ERR_INVALIDTAG;
    }

    while(token->type == PARSETPL_TOK_WHITESPACE) {
      destroy_token(token);
      free(token);

      if(!data->elements) {
        str_cleanup(&tmp);
        current_context->n_cur_foreach_vars--;
        return PARSETPL_ERR_INVALIDTAG;
      }

      token = (token_t*)array_shift(data);
    }

    if(token->type != PARSETPL_TOK_AS || !data->elements) {
      destroy_token(token); free(token);
      return PARSETPL_ERR_INVALIDTAG;
    }

    destroy_token(token); free(token);
    token = (token_t*)array_shift(data);
    if(token->type != PARSETPL_TOK_WHITESPACE || !data->elements) {
      destroy_token(token);
      free(token);
      str_cleanup(&tmp);
      current_context->n_cur_foreach_vars--;
      return PARSETPL_ERR_INVALIDTAG;
    }

    while(token->type == PARSETPL_TOK_WHITESPACE) {
      destroy_token(token);
      free(token);

      if(!data->elements) {
        str_cleanup(&tmp);
        current_context->n_cur_foreach_vars--;
        return PARSETPL_ERR_INVALIDTAG;
      }

      token = (token_t*)array_shift(data);
    }

    if(token->type != PARSETPL_TOK_VARIABLE || data->elements) {
      destroy_token(token);
      free(token);
      str_cleanup(&tmp);
      current_context->n_cur_foreach_vars--;
      return PARSETPL_ERR_INVALIDTAG;
    }

    var2 = token;
    str_cstr_append(&tmp,"i");
    str_cstr_append(&tmp,ivnb);
    str_cstr_append(&tmp," = 0;\n");
    str_cstr_append(&tmp,"vf");
    str_cstr_append(&tmp,v2nb);
    str_cstr_append(&tmp," = NULL;\n");
    str_cstr_append(&tmp,"if(vf");
    str_cstr_append(&tmp,v1nb);
    str_cstr_append(&tmp," && vf");
    str_cstr_append(&tmp,v1nb);
    str_cstr_append(&tmp,"->type == TPL_VARIABLE_ARRAY) {\n");
    str_cstr_append(&tmp,"for(i");
    str_cstr_append(&tmp,ivnb);
    str_cstr_append(&tmp," = 0;i");
    str_cstr_append(&tmp,ivnb);
    str_cstr_append(&tmp," < vf");
    str_cstr_append(&tmp,v1nb);
    str_cstr_append(&tmp,"->data.d_array.elements;i");
    str_cstr_append(&tmp,ivnb);
    str_cstr_append(&tmp,"++) {\n");
    str_cstr_append(&tmp,"vf");
    str_cstr_append(&tmp,v2nb);
    str_cstr_append(&tmp," = (cf_tpl_variable_t*)array_element_at(&vf");
    str_cstr_append(&tmp,v1nb);
    str_cstr_append(&tmp,"->data.d_array,i");
    str_cstr_append(&tmp,ivnb);
    str_cstr_append(&tmp,");\n");
    str_cstr_append(&tmp,"vf");
    str_cstr_append(&tmp,v2nb);
    str_cstr_append(&tmp,"->arrayref = 1;\n");
    str_cstr_append(&tmp,"cf_tpl_setvar(tpl,\"");
    str_chars_append(&tmp,var2->data->content+1,var2->data->len-1);
    str_cstr_append(&tmp,"\",vf");
    str_cstr_append(&tmp,v2nb);
    str_cstr_append(&tmp,");\n");
    str_cstr_append(&tmp,"vf");
    str_cstr_append(&tmp,v2nb);
    str_cstr_append(&tmp,"->arrayref = 0;\n");
    str_init(&vs);
    str_str_set(&vs,var2->data);
    array_push(&current_context->foreach_var_stack,&vs); // do not clean it up, this will be done by the array destroy function
    str_str_append(&current_context->output,&tmp);
    str_str_append(&current_context->output_mem,&tmp);
    destroy_token(var2); free(var2);
  }
  else {
    destroy_token(token);
    free(token);
    return PARSETPL_ERR_INVALIDTAG;
  }

  return 0;
}
/* }}} */

/* {{{ process_if_tag */
int process_if_tag(array_t *data, int is_elseif) {
  string_t tmp;
  string_t *compop;
  token_t *token;
  string_t iv1,iv2,v1,v2;
  int invert, level, ret;
  int type1, type2;
  int *ilevel;
  token_t *tok1 = NULL, *tok2 = NULL;
  char buf[20];
  
  token = (token_t*)array_shift(data);

  if(token->type == PARSETPL_TOK_ENDIF) {
    destroy_token(token);
    free(token);

    if(data->elements) return PARSETPL_ERR_INVALIDTAG;

    if(!current_context->if_level_stack.elements) return PARSETPL_ERR_INVALIDTAG;

    ilevel = array_pop(&current_context->if_level_stack);

    for(level = 0;level <= *ilevel;level++) {
      str_cstr_append(&current_context->output,"}\n");
      str_cstr_append(&current_context->output_mem,"}\n");
    }

    free(ilevel);
    return 0;
  }
  else if(token->type == PARSETPL_TOK_ELSE) {
    destroy_token(token);
    free(token);

    if(data->elements) return PARSETPL_ERR_INVALIDTAG;
    if(!current_context->if_level_stack.elements) return PARSETPL_ERR_INVALIDTAG;

    str_cstr_append(&current_context->output,"} else {\n");
    str_cstr_append(&current_context->output_mem,"} else {\n");
    return 0;
  }
  else if(token->type == PARSETPL_TOK_ELSIF) {
    if(!current_context->if_level_stack.elements) return PARSETPL_ERR_INVALIDTAG;

    token->type = PARSETPL_TOK_IF;
    array_unshift(data,token);
    str_cstr_append(&current_context->output,"} else {\n");
    str_cstr_append(&current_context->output_mem,"} else {\n");
    ret = process_if_tag(data, 1);

    if(ret < 0) {
      // length of } else {\n is 9 chars
      // remove the else again
      current_context->output.len -= 9;
      current_context->output.content[current_context->output.len] = '\0';
      current_context->output_mem.len -= 9;
      current_context->output_mem.content[current_context->output_mem.len] = '\0';
      return ret;
    }

    ilevel = array_element_at(&current_context->if_level_stack,current_context->if_level_stack.elements-1);
    (*ilevel)++;
    return 0;
  }
  else if(token->type == PARSETPL_TOK_IF) {
    destroy_token(token); free(token);
    if(!data->elements) return PARSETPL_ERR_INVALIDTAG;

    token = (token_t*)array_shift(data);
    if(token->type != PARSETPL_TOK_WHITESPACE || !data->elements) {
      destroy_token(token); free(token);
      return PARSETPL_ERR_INVALIDTAG;
    }

    while(token->type == PARSETPL_TOK_WHITESPACE) {
      destroy_token(token); free(token);
      if(!data->elements) return PARSETPL_ERR_INVALIDTAG;
      token = (token_t*)array_shift(data);
    }

    if(token->type == PARSETPL_TOK_NOT) {
      invert = 1;
      destroy_token(token); free(token);

      if(!data->elements) return PARSETPL_ERR_INVALIDTAG;

      token = (token_t*)array_shift(data);
    }
    else invert = 0;

    str_init(&tmp);
    str_init(&iv1);
    str_init(&iv2);
    str_init(&v1);
    str_init(&v2);
    str_cstr_append(&tmp,"cmp_res = 0;\n");

    if(token->type == PARSETPL_TOK_VARIABLE) {
      type1 = PARSETPL_TOK_VARIABLE;
      str_char_set(&v1,"vi",2);
      snprintf(buf,19,"%ld",current_context->n_cur_if_vars++);
      str_cstr_append(&v1,buf);

      if(current_context->n_cur_if_vars > current_context->n_if_vars) current_context->n_if_vars = current_context->n_cur_if_vars;

      ret = dereference_variable(&tmp,token,data,&v1);
      destroy_token(token); free(token);
      if(ret < 0) {
        str_cleanup(&tmp);
        str_cleanup(&iv1);
        str_cleanup(&iv2);
        str_cleanup(&v1);
        str_cleanup(&v2);
        return PARSETPL_ERR_INVALIDTAG;
      }
    }
    else if (token->type == PARSETPL_TOK_STRING || token->type == PARSETPL_TOK_INTEGER) {
      type1 = token->type;
      tok1 = token;
    }
    else if (token->type == PARSETPL_TOK_LOOPVAR) {
      type1 = token->type;
      str_char_set(&iv1,"ii",2);
      snprintf(buf,19,"%ld",current_context->n_cur_if_iters++);
      str_cstr_append(&iv1,buf);

      if(current_context->n_cur_if_iters > current_context->n_if_iters) current_context->n_if_iters = current_context->n_cur_if_iters;

      str_str_append(&tmp,&iv1);
      str_cstr_append(&tmp," = 0;\n");
      ret = dereference_iterator(&tmp,token,data,&iv1);
      destroy_token(token); free(token);

      if(ret < 0) {
        str_cleanup(&tmp);
        str_cleanup(&iv1);
        str_cleanup(&iv2);
        str_cleanup(&v1);
        str_cleanup(&v2);
        return ret;
      }
    }
    else {
      str_cleanup(&tmp);
      return PARSETPL_ERR_INVALIDTAG;
    }

    if(!data->elements) {
      switch(type1) {
        case PARSETPL_TOK_STRING:
        case PARSETPL_TOK_INTEGER:
          str_cleanup(&tmp);
          str_cleanup(&iv1);
          str_cleanup(&iv2);
          str_cleanup(&v1);
          str_cleanup(&v2);
          destroy_token(tok1); free(tok1);
          return PARSETPL_ERR_INVALIDTAG;

        case PARSETPL_TOK_VARIABLE:
          if(invert) str_cstr_append(&tmp,"if(!");
          else str_cstr_append(&tmp,"if(");

          str_str_append(&tmp,&v1);
          str_cstr_append(&tmp,") {\n");
          str_cstr_append(&tmp,"cmp_res = 1;\n");
          str_cstr_append(&tmp,"} else {\n");
          str_cstr_append(&tmp,"cmp_res = 0;\n");
          str_cstr_append(&tmp,"}\n");
          str_cstr_append(&tmp,"if(");
          str_str_append(&tmp,&v1);
          str_cstr_append(&tmp," && ");
          str_str_append(&tmp,&v1);
          str_cstr_append(&tmp,"->temporary) { cf_tpl_var_destroy(");
          str_str_append(&tmp,&v1);
          str_cstr_append(&tmp,"); free(");
          str_str_append(&tmp,&v1);
          str_cstr_append(&tmp,");\n");
          str_str_append(&tmp,&v1);
          str_cstr_append(&tmp," = NULL; }\n");
          str_cstr_append(&tmp,"if(cmp_res) {\n");
          current_context->n_cur_if_vars -= 1;
          break;

        case PARSETPL_TOK_LOOPVAR:
          if(invert) str_cstr_append(&tmp,"if(!");
          else str_cstr_append(&tmp,"if(");

          str_str_append(&tmp,&iv1);
          str_cstr_append(&tmp,") {\n");
          current_context->n_cur_if_iters -= 1;
          break;

        default:
          if(tok1) {
            destroy_token(tok1);
            free(tok1);
          }

          str_cleanup(&tmp);
          str_cleanup(&iv1);
          str_cleanup(&iv2);
          str_cleanup(&v1);
          str_cleanup(&v2);
          return PARSETPL_ERR_INVALIDTAG;
      }

      str_str_append(&current_context->output,&tmp);
      str_str_append(&current_context->output_mem,&tmp);
      str_cleanup(&tmp);

      if (!is_elseif) {
        level = 0;
        array_push(&current_context->if_level_stack,&level);
      }

      return 0;
    }

    token = (token_t*)array_shift(data);
    while(token->type == PARSETPL_TOK_WHITESPACE) {
      destroy_token(token);
      free(token);

      if(!data->elements) {
        str_cleanup(&tmp);
        str_cleanup(&iv1);
        str_cleanup(&iv2);
        str_cleanup(&v1);
        str_cleanup(&v2);

        if(tok1) destroy_token(tok1); free(tok1);

        return PARSETPL_ERR_INVALIDTAG;
      }

      token = (token_t*)array_shift(data);
    }

    if(!data->elements || token->type != PARSETPL_TOK_COMPARE) {
      destroy_token(token); free(token);
      str_cleanup(&tmp);
      str_cleanup(&iv1);
      str_cleanup(&iv2);
      str_cleanup(&v1);
      str_cleanup(&v2);

      if(tok1) destroy_token(tok1); free(tok1);

      return PARSETPL_ERR_INVALIDTAG;
    }

    compop = token->data;
    free(token);
    token = (token_t*)array_shift(data);

    while(token->type == PARSETPL_TOK_WHITESPACE) {
      destroy_token(token);
      free(token);

      if(!data->elements) {
        str_cleanup(&tmp);
        str_cleanup(&iv1);
        str_cleanup(&iv2);
        str_cleanup(&v1);
        str_cleanup(&v2);

        if(tok1) destroy_token(tok1); free(tok1);

        str_cleanup(compop); free(compop);
        return PARSETPL_ERR_INVALIDTAG;
      }

      token = (token_t*)array_shift(data);
    }

    if(token->type == PARSETPL_TOK_VARIABLE) {
      type2 = PARSETPL_TOK_VARIABLE;
      str_char_set(&v2,"vi",2);
      snprintf(buf,19,"%ld",current_context->n_cur_if_vars++);
      str_cstr_append(&v2,buf);

      if(current_context->n_cur_if_vars > current_context->n_if_vars) current_context->n_if_vars = current_context->n_cur_if_vars;

      ret = dereference_variable(&tmp,token,data,&v2);
      destroy_token(token); free(token);

      if(ret < 0) {
        str_cleanup(&tmp);
        str_cleanup(&iv1);
        str_cleanup(&iv2);
        str_cleanup(&v1);
        str_cleanup(&v2);

        if(tok1) {
          destroy_token(tok1);
          free(tok1);
        }

        str_cleanup(compop); free(compop);
        return PARSETPL_ERR_INVALIDTAG;
      }
    }
    else if (token->type == PARSETPL_TOK_STRING || token->type == PARSETPL_TOK_INTEGER) {
      type2 = token->type;
      tok2 = token;
    }
    else if (token->type == PARSETPL_TOK_LOOPVAR) {
      type2 = PARSETPL_TOK_LOOPVAR;
      str_char_set(&iv2,"ii",2);
      snprintf(buf,19,"%ld",current_context->n_cur_if_iters++);
      str_cstr_append(&iv2,buf);

      if(current_context->n_cur_if_iters > current_context->n_if_iters) current_context->n_if_iters = current_context->n_cur_if_iters;

      str_str_append(&tmp,&iv2);
      str_cstr_append(&tmp," = 0;\n");

      ret = dereference_iterator(&tmp,token,data,&iv2);
      destroy_token(token);
      free(token);

      if(ret < 0) {
        str_cleanup(&tmp);
        str_cleanup(&iv1);
        str_cleanup(&iv2);
        str_cleanup(&v1);
        str_cleanup(&v2);

        if(tok1) {
          destroy_token(tok1);
          free(tok1);
        }

        str_cleanup(compop); free(compop);
        return ret;
      }
    }
    else {
      str_cleanup(&tmp);
      str_cleanup(&iv1);
      str_cleanup(&iv2);
      str_cleanup(&v1);
      str_cleanup(&v2);

      if(tok1) {
        destroy_token(tok1);
        free(tok1);
      }

      str_cleanup(compop);
      free(compop);

      return PARSETPL_ERR_INVALIDTAG;
    }

    if(data->elements) {
      str_cleanup(&iv1);
      str_cleanup(&iv2);
      str_cleanup(&v1);
      str_cleanup(&v2);
      str_cleanup(&tmp);

      if(tok1) destroy_token(tok1); free(tok1);
      if(tok2) destroy_token(tok2); free(tok2);

      str_cleanup(compop); free(compop);
      return PARSETPL_ERR_INVALIDTAG;
    }

    level = 0;

    switch(type2) {
      case PARSETPL_TOK_VARIABLE:
        switch(type1) {
          case PARSETPL_TOK_VARIABLE:
            // if both are arrays, just compare the number of elements
            str_cstr_append(&tmp,"if(!");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp," && !");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp,") {\n");

            if(compop->content[0] == '!') str_cstr_append(&tmp,"cmp_res = 0;\n");
            else str_cstr_append(&tmp,"cmp_res = 1;\n"); // two nonexistant variables are the same

            str_cstr_append(&tmp,"} else ");
            str_cstr_append(&tmp,"if((!");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp," && ");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp,") || (");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp," && !");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp,")) {\n");

            if(compop->content[0] == '!') str_cstr_append(&tmp,"cmp_res = 1;\n");
            else str_cstr_append(&tmp,"cmp_res = 0;\n");

            str_cstr_append(&tmp,"} else ");
            str_cstr_append(&tmp,"if(");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp," && ");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp," && ");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp,"->type == ");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp,"->type == TPL_VARIABLE_ARRAY) {\n");
            str_cstr_append(&tmp,"cmp_res = (");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp,"->data.d_array.elements == ");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp,"->data.d_array.elements) ? ");

            if(compop->content[0] == '!') str_cstr_append(&tmp,"0 : 1;\n");
            else str_cstr_append(&tmp,"1 : 0;\n");

            str_cstr_append(&tmp,"} else {\n");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp," = (cf_tpl_variable_t *)cf_tpl_var_convert(NULL,");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp,",");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp,"->type);\n");
            str_cstr_append(&tmp,"if(");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp," && ((");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp,"->type == TPL_VARIABLE_STRING && !strcmp(");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp,"->data.d_string.content,");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp,"->data.d_string.content)) || (");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp,"->type == TPL_VARIABLE_INT && ");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp,"->data.d_int == ");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp,"->data.d_int))) {\n");

            if(compop->content[0] == '!') str_cstr_append(&tmp,"cmp_res = 0;\n");
            else str_cstr_append(&tmp,"cmp_res = 1;\n");

            str_cstr_append(&tmp,"} else {\n");

            if(compop->content[0] == '!') str_cstr_append(&tmp,"cmp_res = 1;\n");
            else str_cstr_append(&tmp,"cmp_res = 0;\n");

            str_cstr_append(&tmp,"}\n");
            str_cstr_append(&tmp,"if(");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp," && ");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp,"->temporary) { cf_tpl_var_destroy(");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp,"); free(");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp,");\n");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp," = NULL; }\n");
            str_cstr_append(&tmp,"if(");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp," && ");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp,"->temporary) { cf_tpl_var_destroy(");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp,"); free(");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp,");\n");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp," = NULL; }\n");
            str_cstr_append(&tmp,"}\n");
            current_context->n_cur_if_vars -= 2;
            break;

          case PARSETPL_TOK_LOOPVAR:
            // convert it to integer
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp," = (cf_tpl_variable_t *)cf_tpl_var_convert(NULL,");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp,",TPL_VARIABLE_INT);\n");
            str_cstr_append(&tmp,"if(");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp," && ");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp,"->data.d_int == ");
            str_str_append(&tmp,&iv1);
            str_cstr_append(&tmp,") {\n");

            if(compop->content[0] == '!') str_cstr_append(&tmp,"cmp_res = 0;\n");
            else str_cstr_append(&tmp,"cmp_res = 1;\n");

            str_cstr_append(&tmp,"} else {\n");

            if(compop->content[0] == '!') str_cstr_append(&tmp,"cmp_res = 1;\n");
            else str_cstr_append(&tmp,"cmp_res = 0;\n");

            str_cstr_append(&tmp,"}\n");
            str_cstr_append(&tmp,"if(");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp," && ");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp,"->temporary) { cf_tpl_var_destroy(");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp,"); free(");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp,");");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp," = NULL; }\n");
            current_context->n_cur_if_vars -= 1;
            current_context->n_cur_if_iters -= 1;
            break;

          case PARSETPL_TOK_INTEGER:
            // convert it to integer
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp," = (cf_tpl_variable_t *)cf_tpl_var_convert(NULL,");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp,",TPL_VARIABLE_INT);\n");
            str_cstr_append(&tmp,"if(");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp," && ");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp,"->data.d_int == ");
            str_str_append(&tmp,tok1->data);
            str_cstr_append(&tmp,") {\n");

            if(compop->content[0] == '!') str_cstr_append(&tmp,"cmp_res = 0;\n");
            else str_cstr_append(&tmp,"cmp_res = 1;\n");

            str_cstr_append(&tmp,"} else {\n");

            if(compop->content[0] == '!') str_cstr_append(&tmp,"cmp_res = 1;\n");
            else str_cstr_append(&tmp,"cmp_res = 0;\n");

            str_cstr_append(&tmp,"}\n");
            str_cstr_append(&tmp,"if(");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp," && ");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp,"->temporary) { cf_tpl_var_destroy(");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp,"); free(");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp,");\n");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp," = NULL; }\n");
            current_context->n_cur_if_vars -= 1;
            break;

          case PARSETPL_TOK_STRING:
            // convert it to string
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp," = (cf_tpl_variable_t *)cf_tpl_var_convert(NULL,");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp,",TPL_VARIABLE_STRING);\n");
            str_cstr_append(&tmp,"if(");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp," && !strcmp(");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp,"->data.d_string.content,\"");
            append_escaped_string(&tmp,tok1->data);
            str_cstr_append(&tmp,"\")) {\n");

            if(compop->content[0] == '!') str_cstr_append(&tmp,"cmp_res = 0;\n");
            else str_cstr_append(&tmp,"cmp_res = 1;\n");

            str_chars_append(&tmp,"} else {\n",9);

            if(compop->content[0] == '!') str_cstr_append(&tmp,"cmp_res = 1;\n");
            else str_cstr_append(&tmp,"cmp_res = 0;\n");

            str_cstr_append(&tmp,"}\n");
            str_cstr_append(&tmp,"if(");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp," && ");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp,"->temporary) { cf_tpl_var_destroy(");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp,"); free(");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp,");\n");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp," = NULL; }\n");
            current_context->n_cur_if_vars -= 1;
            break;

          default:
            str_chars_append(&tmp,"cmp_res = 0;\n",13);
            break;
        }
        break;

      case PARSETPL_TOK_LOOPVAR:
        switch(type1) {
          case PARSETPL_TOK_VARIABLE:
            // convert it to integer
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp," = (cf_tpl_variable_t *)cf_tpl_var_convert(NULL,");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp,",TPL_VARIABLE_INT);\n");
            str_cstr_append(&tmp,"if(");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp," && ");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp,"->data.d_int == ");
            str_str_append(&tmp,&iv2);
            str_cstr_append(&tmp,") {\n");

            if(compop->content[0] == '!') str_cstr_append(&tmp,"cmp_res = 0;\n");
            else str_cstr_append(&tmp,"cmp_res = 1;\n");

            str_cstr_append(&tmp,"} else {\n");

            if(compop->content[0] == '!') str_cstr_append(&tmp,"cmp_res = 1;\n");
            else str_cstr_append(&tmp,"cmp_res = 0;\n");

            str_cstr_append(&tmp,"}\n");
            str_cstr_append(&tmp,"if(");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp," && ");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp,"->temporary) { cf_tpl_var_destroy(");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp,"); free(");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp,");\n");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp," = NULL; }\n");
            current_context->n_cur_if_vars -= 1;
            current_context->n_cur_if_iters -= 1;
            break;

          case PARSETPL_TOK_LOOPVAR:
            str_cstr_append(&tmp,"if(");
            str_str_append(&tmp,&iv1);
            str_cstr_append(&tmp," == ");
            str_str_append(&tmp,&iv2);
            str_cstr_append(&tmp,") {\n");

            if(compop->content[0] == '!') str_cstr_append(&tmp,"cmp_res = 0;\n");
            else str_cstr_append(&tmp,"cmp_res = 1;\n");

            str_cstr_append(&tmp,"} else {\n");

            if(compop->content[0] == '!') str_cstr_append(&tmp,"cmp_res = 1;\n");
            else str_cstr_append(&tmp,"cmp_res = 0;\n");

            str_cstr_append(&tmp,"}\n");
            current_context->n_cur_if_iters -= 2;
            break;

          case PARSETPL_TOK_INTEGER:
            str_cstr_append(&tmp,"if(");
            str_str_append(&tmp,tok1->data);
            str_cstr_append(&tmp," == ");
            str_str_append(&tmp,&iv2);
            str_cstr_append(&tmp,") {\n");

            if(compop->content[0] == '!') str_cstr_append(&tmp,"cmp_res = 0;\n");
            else str_cstr_append(&tmp,"cmp_res = 1;\n");

            str_cstr_append(&tmp,"} else {\n");

            if(compop->content[0] == '!') str_cstr_append(&tmp,"cmp_res = 1;\n");
            else str_cstr_append(&tmp,"cmp_res = 0;\n");

            str_cstr_append(&tmp,"}\n");
            current_context->n_cur_if_iters -= 1;
            break;

          case PARSETPL_TOK_STRING:
            str_cstr_append(&tmp,"if(strtol(\"");
            append_escaped_string(&tmp,tok1->data);
            str_cstr_append(&tmp,"\",NULL,10) == ");
            str_str_append(&tmp,&iv2);
            str_cstr_append(&tmp,") {\n");

            if(compop->content[0] == '!') str_cstr_append(&tmp,"cmp_res = 0;\n");
            else str_cstr_append(&tmp,"cmp_res = 1;\n");

            str_cstr_append(&tmp,"} else {\n");

            if(compop->content[0] == '!') str_cstr_append(&tmp,"cmp_res = 1;\n");
            else str_cstr_append(&tmp,"cmp_res = 0;\n");

            str_cstr_append(&tmp,"}\n");
            current_context->n_cur_if_iters -= 1;
            break;

          default:
            str_cstr_append(&tmp,"cmp_res = 0;\n");
            break;
        }
        break;

      case PARSETPL_TOK_STRING:
        switch(type1) {
          case PARSETPL_TOK_VARIABLE:
            // convert it to string
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp," = (cf_tpl_variable_t *)cf_tpl_var_convert(NULL,");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp,",TPL_VARIABLE_STRING);\n");
            str_cstr_append(&tmp,"if(");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp," && !strcmp(");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp,"->data.d_string.content,\"");
            append_escaped_string(&tmp,tok2->data);
            str_cstr_append(&tmp,"\")) {\n");

            if(compop->content[0] == '!') str_cstr_append(&tmp,"cmp_res = 0;\n");
            else str_cstr_append(&tmp,"cmp_res = 1;\n");

            str_cstr_append(&tmp,"} else {\n");
            if(compop->content[0] == '!') str_cstr_append(&tmp,"cmp_res = 1;\n");
            else str_cstr_append(&tmp,"cmp_res = 0;\n");

            str_cstr_append(&tmp,"}\n");
            str_cstr_append(&tmp,"if(");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp," && ");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp,"->temporary) { cf_tpl_var_destroy(");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp,"); free(");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp,");\n");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp," = NULL; }\n");
            current_context->n_cur_if_vars -= 1;
            break;

          case PARSETPL_TOK_LOOPVAR:
            str_cstr_append(&tmp,"if(strtol(\"");
            append_escaped_string(&tmp,tok2->data);
            str_cstr_append(&tmp,"\",NULL,10) == ");
            str_str_append(&tmp,&iv1);
            str_cstr_append(&tmp,") {\n");

            if(compop->content[0] == '!') str_cstr_append(&tmp,"cmp_res = 0;\n");
            else str_cstr_append(&tmp,"cmp_res = 1;\n");

            str_cstr_append(&tmp,"} else {\n");

            if(compop->content[0] == '!') str_cstr_append(&tmp,"cmp_res = 1;\n");
            else str_cstr_append(&tmp,"cmp_res = 0;\n");

            str_cstr_append(&tmp,"}\n");
            current_context->n_cur_if_iters -= 1;
            break;

          case PARSETPL_TOK_STRING:
          case PARSETPL_TOK_INTEGER:
            // do compare here
            if(!strcmp(tok1->data->content,tok2->data->content)) {
              if(compop->content[0] == '!') str_cstr_append(&tmp,"cmp_res = 0;\n");
              else str_cstr_append(&tmp,"cmp_res = 1;\n");
            }
            else {
              if(compop->content[0] == '!') str_cstr_append(&tmp,"cmp_res = 1;\n");
              else str_cstr_append(&tmp,"cmp_res = 0;\n");
            }
            break;

          default:
            str_cstr_append(&tmp,"cmp_res = 0;\n");
            break;
        }
        break;

      case PARSETPL_TOK_INTEGER:
        switch(type1) {
          case PARSETPL_TOK_VARIABLE:
            // convert it to integer
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp," = (cf_tpl_variable_t *)cf_tpl_var_convert(NULL,");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp,",TPL_VARIABLE_INT);\n");
            str_cstr_append(&tmp,"if(");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp," && ");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp,"->data.d_int == ");
            str_str_append(&tmp,tok2->data);
            str_cstr_append(&tmp,") {\n");

            if(compop->content[0] == '!') str_cstr_append(&tmp,"cmp_res = 0;\n");
            else str_cstr_append(&tmp,"cmp_res = 1;\n");

            str_cstr_append(&tmp,"} else {\n");

            if(compop->content[0] == '!') str_cstr_append(&tmp,"cmp_res = 1;\n");
            else str_cstr_append(&tmp,"cmp_res = 0;\n");

            str_cstr_append(&tmp,"}\n");
            str_cstr_append(&tmp,"if(");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp," && ");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp,"->temporary) { cf_tpl_var_destroy(");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp,"); free(");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp,");\n");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp," = NULL; }\n");
            current_context->n_cur_if_vars -= 1;
            break;

          case PARSETPL_TOK_LOOPVAR:
            str_cstr_append(&tmp,"if(");
            str_str_append(&tmp,tok2->data);
            str_cstr_append(&tmp," == ");
            str_str_append(&tmp,&iv1);
            str_cstr_append(&tmp,") {\n");

            if(compop->content[0] == '!') str_cstr_append(&tmp,"cmp_res = 0;\n");
            else str_cstr_append(&tmp,"cmp_res = 1;\n");

            str_cstr_append(&tmp,"} else {\n");
            if(compop->content[0] == '!') str_cstr_append(&tmp,"cmp_res = 1;\n");
            else str_cstr_append(&tmp,"cmp_res = 0;\n");

            str_cstr_append(&tmp,"}\n");
            current_context->n_cur_if_iters -= 1;
            break;

          case PARSETPL_TOK_STRING:
          case PARSETPL_TOK_INTEGER:
            // do compare here
            if(strtol(tok1->data->content,NULL,10) == strtol(tok2->data->content,NULL,10)) {
              if(compop->content[0] == '!') str_cstr_append(&tmp,"cmp_res = 0;\n");
              else str_cstr_append(&tmp,"cmp_res = 1;\n");
            }
            else {
              if(compop->content[0] == '!') str_cstr_append(&tmp,"cmp_res = 1;\n");
              else str_cstr_append(&tmp,"cmp_res = 0;\n");
            }
            break;

          default:
            str_cstr_append(&tmp,"cmp_res = 0;\n");
            break;
        }
        break;

      default:
        str_cstr_append(&tmp,"cmp_res = 0;\n");
        break;
    }

    str_cstr_append(&tmp,"if(cmp_res) {\n");
    str_str_append(&current_context->output,&tmp);
    str_str_append(&current_context->output_mem,&tmp);
    str_cleanup(&tmp);
    str_cleanup(&iv1);
    str_cleanup(&iv2);
    str_cleanup(&v1);
    str_cleanup(&v2);
    str_cleanup(compop);
    free(compop);

    if(tok1) destroy_token(tok1); free(tok1);
    if(tok2) destroy_token(tok2); free(tok2);

    if(!is_elseif) {
      level = 0;
      array_push(&current_context->if_level_stack,&level);
    }

    return 0;
  }
  else { // elseif is not supported currently!
    destroy_token(token); free(token);
    return PARSETPL_ERR_INVALIDTAG;
  }

  return PARSETPL_ERR_INVALIDTAG;
}
/* }}} */

/* {{{ process_func_tag */
int process_func_tag(array_t *data) {
  token_t *token;
  function_t *func;

  token = (token_t*)array_shift(data);
  if(token->type == PARSETPL_TOK_FUNC_END) {
    if(current_context == &global_context) return PARSETPL_ERR_INVALIDTAG;
    if(data->elements) return PARSETPL_ERR_INVALIDTAG;

    current_context = &global_context;
    return 0;
  }

  if(token->type != PARSETPL_TOK_FUNC || !data->elements) return PARSETPL_ERR_INVALIDTAG;

  token = (token_t*)array_shift(data);
  while(data->elements && token->type == PARSETPL_TOK_WHITESPACE) {
    destroy_token(token); free(token);
    token = (token_t*)array_shift(data);
  }

  if(!data->elements) {
    destroy_token(token); free(token);
    return PARSETPL_ERR_INVALIDTAG;
  }

  if(token->type != PARSETPL_TOK_STRING) {
    destroy_token(token); free(token);
    return PARSETPL_ERR_INVALIDTAG;
  }

  if(cf_hash_get(defined_functions,token->data->content,token->data->len)) return PARSETPL_ERR_INVALIDTAG;

  func = fo_alloc(NULL,sizeof(function_t),1,FO_ALLOC_MALLOC);
  init_function(func);
  str_str_set(&func->name,token->data);
  destroy_token(token); free(token);
  token = (token_t*)array_shift(data);

  while(data->elements && token->type == PARSETPL_TOK_WHITESPACE) {
    destroy_token(token); free(token);
    token = (token_t*)array_shift(data);
  }

  if(!data->elements || token->type != PARSETPL_TOK_PARAMS_START) {
    destroy_token(token); free(token);
    destroy_function(func); free(func);
    return PARSETPL_ERR_INVALIDTAG;
  }

  token = (token_t*)array_shift(data);
  while(data->elements && token->type == PARSETPL_TOK_WHITESPACE) {
    destroy_token(token); free(token);
    token = (token_t*)array_shift(data);
  }

  if(token->type != PARSETPL_TOK_PARAMS_END) {
    do {
      if(!data->elements || token->type != PARSETPL_TOK_VARIABLE) {
        destroy_token(token); free(token);
        destroy_function(func); free(func);
        return PARSETPL_ERR_INVALIDTAG;
      }

      array_push(&func->params,token->data);
      free(token);
      token = (token_t*)array_shift(data);

      while(data->elements && token->type == PARSETPL_TOK_WHITESPACE) {
        destroy_token(token); free(token);
        token = (token_t*)array_shift(data);
      }

      if(token->type == PARSETPL_TOK_PARAMS_END) break;

      if(!data->elements || token->type != PARSETPL_TOK_ARRAYSEP) {
        destroy_token(token); free(token);
        destroy_function(func); free(func);
        return PARSETPL_ERR_INVALIDTAG;
      }

      token = (token_t*)array_shift(data);
      while(data->elements && token->type == PARSETPL_TOK_WHITESPACE) {
        destroy_token(token); free(token);
        token = (token_t*)array_shift(data);
      }

      if(!data->elements) {
        destroy_token(token); free(token);
        destroy_function(func); free(func);
        return PARSETPL_ERR_INVALIDTAG;
      }
    } while(1);
  }

  destroy_token(token); free(token);

  if(data->elements) {
    token = (token_t*)array_shift(data);

    while(token->type == PARSETPL_TOK_WHITESPACE) {
      destroy_token(token); free(token);
      if(!data->elements) break;
      token = (token_t*)array_shift(data);
    }

    if(data->elements) {
      destroy_token(token); free(token);
      destroy_function(func); free(func);
      return PARSETPL_ERR_INVALIDTAG;
    }
  }

  cf_hash_set_static(defined_functions,func->name.content,func->name.len,func);
  array_push(defined_function_list,&func);
  current_context = func->ctx;

  return 0;
}
/* }}} */

/* {{{ process_func_call_tag */
int process_func_call_tag(array_t *data) {
  token_t *token;
  function_t *func;
  array_t params;
  string_t tmp,v1,iv1;
  string_t *v;
  size_t i;
  int ret;
  char buf[20];

  array_init(&params,sizeof(string_t),(void (*)(void *))str_cleanup); // internal vars
  str_init(&tmp);

  token = (token_t*)array_shift(data);
  if(token->type != PARSETPL_TOK_FUNC_CALL || !data->elements) {
    array_destroy(&params);
    str_cleanup(&tmp);
    return PARSETPL_ERR_INVALIDTAG;
  }

  token = (token_t*)array_shift(data);
  while(data->elements && token->type == PARSETPL_TOK_WHITESPACE) {
    destroy_token(token); free(token);
    token = (token_t*)array_shift(data);
  }

  if(!data->elements) {
    destroy_token(token); free(token);
    array_destroy(&params);
    str_cleanup(&tmp);
    return PARSETPL_ERR_INVALIDTAG;
  }

  if(token->type != PARSETPL_TOK_STRING) {
    destroy_token(token); free(token);
    array_destroy(&params);
    str_cleanup(&tmp);
    return PARSETPL_ERR_INVALIDTAG;
  }

  if(!(func = cf_hash_get(defined_functions,token->data->content,token->data->len))) {
    array_destroy(&params);
    str_cleanup(&tmp);
    return PARSETPL_ERR_INVALIDTAG;
  }

  destroy_token(token); free(token);
  token = (token_t*)array_shift(data);

  while(data->elements && token->type == PARSETPL_TOK_WHITESPACE) {
    destroy_token(token); free(token);
    token = (token_t*)array_shift(data);
  }

  if(!data->elements || token->type != PARSETPL_TOK_PARAMS_START) {
    destroy_token(token); free(token);
    array_destroy(&params);
    str_cleanup(&tmp);
    return PARSETPL_ERR_INVALIDTAG;
  }

  token = (token_t*)array_shift(data);
  while(data->elements && token->type == PARSETPL_TOK_WHITESPACE) {
    destroy_token(token); free(token);
    token = (token_t*)array_shift(data);
  }

  if(token->type != PARSETPL_TOK_PARAMS_END) {
    do {
      if(!data->elements) {
        destroy_token(token); free(token);
        array_destroy(&params);
        str_cleanup(&tmp);
        return PARSETPL_ERR_INVALIDTAG;
      }

      if(token->type == PARSETPL_TOK_VARIABLE) {
        str_init(&v1);
        str_char_set(&v1,"vfc",3);
        snprintf(buf,19,"%ld",current_context->n_cur_call_vars++);
        str_cstr_append(&v1,buf);

        if(current_context->n_cur_call_vars > current_context->n_call_vars) current_context->n_call_vars = current_context->n_cur_call_vars;

        ret = dereference_variable(&tmp,token,data,&v1);
        if(ret < 0) {
          current_context->n_cur_call_vars--;
          str_cleanup(&v1);
          array_destroy(&params);
          str_cleanup(&tmp);
          return PARSETPL_ERR_INVALIDTAG;
        }

        array_push(&params,&v1);
      }
      else if(token->type == PARSETPL_TOK_LOOPVAR) {
        str_init(&iv1);
        str_char_set(&iv1,"ifc",3);
        snprintf(buf,19,"%ld",current_context->n_cur_call_iters++);
        str_cstr_append(&iv1,buf);

        if(current_context->n_cur_call_iters > current_context->n_call_iters) current_context->n_call_iters = current_context->n_cur_call_iters;

        str_str_append(&tmp,&iv1);
        str_cstr_append(&tmp," = 0;\n");
        ret = dereference_iterator(&tmp,token,data,&iv1);

        if(ret < 0) {
          current_context->n_cur_call_iters--;
          str_cleanup(&iv1);
          array_destroy(&params);
          str_cleanup(&tmp);
          return PARSETPL_ERR_INVALIDTAG;
        }

        str_init(&v1);
        str_char_set(&v1,"vfc",3);
        snprintf(buf,19,"%ld",current_context->n_cur_call_vars++);
        str_cstr_append(&v1,buf);

        if(current_context->n_cur_call_vars > current_context->n_call_vars) current_context->n_call_vars = current_context->n_cur_call_vars;

        str_str_append(&tmp,&v1);
        str_cstr_append(&tmp," = fo_alloc(NULL,sizeof(cf_tpl_variable_t),1,FO_ALLOC_MALLOC);\n");
        str_cstr_append(&tmp,"cf_tpl_var_init(");
        str_str_append(&tmp,&v1);
        str_cstr_append(&tmp,",TPL_VARIABLE_INT);\n");
        str_cstr_append(&tmp,"cf_tpl_var_setvalue(");
        str_str_append(&tmp,&v1);
        str_cstr_append(&tmp,",");
        str_str_append(&tmp,&iv1);
        str_cstr_append(&tmp,");\n");
        str_str_append(&tmp,&v1);
        str_cstr_append(&tmp,"->temporary = 1;\n");
        str_cleanup(&iv1);
        current_context->n_cur_call_iters--;
        array_push(&params,&v1);
      }
      else if(token->type == PARSETPL_TOK_INTEGER) {
        str_init(&v1);
        str_char_set(&v1,"vfc",3);
        snprintf(buf,19,"%ld",current_context->n_cur_call_vars++);
        str_cstr_append(&v1,buf);

        if(current_context->n_cur_call_vars > current_context->n_call_vars) current_context->n_call_vars = current_context->n_cur_call_vars;

        str_str_append(&tmp,&v1);
        str_cstr_append(&tmp," = fo_alloc(NULL,sizeof(cf_tpl_variable_t),1,FO_ALLOC_MALLOC);\n");
        str_cstr_append(&tmp,"cf_tpl_var_init(");
        str_str_append(&tmp,&v1);
        str_cstr_append(&tmp,",TPL_VARIABLE_INT);\n");
        str_cstr_append(&tmp,"cf_tpl_var_setvalue(");
        str_str_append(&tmp,&v1);
        str_cstr_append(&tmp,",");
        str_str_append(&tmp,token->data);
        str_cstr_append(&tmp,");\n");
        str_str_append(&tmp,&v1);
        str_cstr_append(&tmp,"->temporary = 1;\n");
        array_push(&params,&v1);
      }
      else if(token->type == PARSETPL_TOK_STRING) {
        str_init(&v1);
        str_char_set(&v1,"vfc",3);
        snprintf(buf,19,"%ld",current_context->n_cur_call_vars++);
        str_cstr_append(&v1,buf);

        if(current_context->n_cur_call_vars > current_context->n_call_vars) current_context->n_call_vars = current_context->n_cur_call_vars;

        str_str_append(&tmp,&v1);
        str_cstr_append(&tmp," = fo_alloc(NULL,sizeof(cf_tpl_variable_t),1,FO_ALLOC_MALLOC);\n");
        str_cstr_append(&tmp,"cf_tpl_var_init(");
        str_str_append(&tmp,&v1);
        str_cstr_append(&tmp,",TPL_VARIABLE_STRING);\n");
        str_cstr_append(&tmp,"cf_tpl_var_setvalue(");
        str_str_append(&tmp,&v1);
        str_cstr_append(&tmp,",\"");
        append_escaped_string(&tmp,token->data);
        str_cstr_append(&tmp,"\",");
        snprintf(buf,19,"%ld",token->data->len);
        str_cstr_append(&tmp,buf);
        str_cstr_append(&tmp,");\n");
        str_str_append(&tmp,&v1);
        str_cstr_append(&tmp,"->temporary = 1;\n");
        array_push(&params,&v1);
      }

      destroy_token(token); free(token);
      token = (token_t*)array_shift(data);

      while(data->elements && token->type == PARSETPL_TOK_WHITESPACE) {
        destroy_token(token); free(token);
        token = (token_t*)array_shift(data);
      }

      if(token->type == PARSETPL_TOK_PARAMS_END) break;

      if(!data->elements || token->type != PARSETPL_TOK_ARRAYSEP) {
        destroy_token(token); free(token);
        array_destroy(&params);
        str_cleanup(&tmp);
        return PARSETPL_ERR_INVALIDTAG;
      }

      token = (token_t*)array_shift(data);
      while(data->elements && token->type == PARSETPL_TOK_WHITESPACE) {
        destroy_token(token); free(token);
        token = (token_t*)array_shift(data);
      }

      if(!data->elements) {
        destroy_token(token); free(token);
        array_destroy(&params);
        str_cleanup(&tmp);
        return PARSETPL_ERR_INVALIDTAG;
      }
    } while(1);
  }

  destroy_token(token);
  free(token);

  if(data->elements) {
    token = (token_t*)array_shift(data);

    while(token->type == PARSETPL_TOK_WHITESPACE) {
      destroy_token(token); free(token);
      if(!data->elements) break;
      token = (token_t*)array_shift(data);
    }

    if(data->elements) {
      destroy_token(token); free(token);
      array_destroy(&params);
      str_cleanup(&tmp);
      return PARSETPL_ERR_INVALIDTAG;
    }
  }

  if(params.elements != func->params.elements) {
    array_destroy(&params);
    str_cleanup(&tmp);
    return PARSETPL_ERR_INVALIDTAG;
  }

  str_cstr_append(&tmp,"tpl_func_");
  str_str_append(&tmp,&func->name);
  str_str_append(&current_context->output,&tmp);
  str_str_append(&current_context->output_mem,&tmp);
  str_cleanup(&tmp);
  str_cstr_append(&current_context->output,"(tpl,");
  str_cstr_append(&current_context->output_mem,"_to_mem(tpl,");
  str_init(&tmp);

  for(i = 0; i < params.elements; i++) {
    if(i > 0) str_char_append(&tmp,',');

    v = (string_t *)array_element_at(&params,i);
    str_str_append(&tmp,v);
  }

  str_cstr_append(&tmp,");\n");
  str_str_append(&current_context->output,&tmp);
  str_str_append(&current_context->output_mem,&tmp);
  str_cleanup(&tmp);
  current_context->n_cur_call_vars -= params.elements;
  array_destroy(&params);

  return 0;
}
/* }}} */

/* {{{ process_tag */
int process_tag(array_t *data) {
  token_t *variable, *token;
  int ret, had_whitespace, rtype;
  
  if(!data->elements) return PARSETPL_ERR_INVALIDTAG;
  
  rtype = ((token_t*)array_element_at(data,0))->type;
  
  if(rtype == PARSETPL_TOK_IWS_START && data->elements == 1) {
    current_context->iws = 1;
    return 0;
  }
  else if(rtype == PARSETPL_TOK_IWS_END && data->elements == 1) {
    current_context->iws = 0;
    return 0;
  }
  else if(rtype == PARSETPL_TOK_NLE_START && data->elements == 1) {
    current_context->nle = 1;
    return 0;
  }
  else if(rtype == PARSETPL_TOK_NLE_END && data->elements == 1) {
    current_context->nle = 0;
    return 0;
  }
  else if(rtype == PARSETPL_TOK_FUNC || rtype == PARSETPL_TOK_FUNC_END) {
    return process_func_tag(data);
  }
  else if(rtype == PARSETPL_TOK_FUNC_CALL) return process_func_call_tag(data);
  else if(rtype == PARSETPL_TOK_VARIABLE) {
    variable = (token_t *)array_shift(data);
    // 2 possibilities:
    //   a) print $variable ($variable,$variable[0][2],$variable->escaped,$variable[0]->escaped)
    //   b) assign new value to $variable ($variable = "value",$variable = 0,$variable = ["hi","ho"],$variable = [["hi","ho"],["ha","hu"]])
    
    // no more elements or next element is => type a
    if(!data->elements) ret = process_variable_print_tag(variable,data); // directly output
    else {
      had_whitespace = 0;

      do {
        token = (token_t *)array_shift(data);

        if(token->type == PARSETPL_TOK_WHITESPACE) {
          had_whitespace = 1;
          continue;
        }
        else if(token->type == PARSETPL_TOK_ASSIGNMENT) break;
        else if(token->type == PARSETPL_TOK_ARRAYSTART || token->type == PARSETPL_TOK_MODIFIER_ESCAPE) {
          if(had_whitespace) {
            destroy_token(variable); free(variable);
            destroy_token(token); free(token);
            return PARSETPL_ERR_INVALIDTAG;
          }

          array_unshift(data,token);
          break;
        }
        else {
          destroy_token(variable); free(variable);
          destroy_token(token); free(token);
          return PARSETPL_ERR_INVALIDTAG;
        }

        destroy_token(token); free(token);
      } while(data->elements);

      if(token->type == PARSETPL_TOK_ASSIGNMENT) ret = process_variable_assignment_tag(variable,data);
      else ret = process_variable_print_tag(variable,data);

      destroy_token(token); free(token);
    }

    destroy_token(variable); free(variable);
    if (ret < 0) return ret;

    return 0;
  }
  else if(rtype == PARSETPL_TOK_LOOPVAR) {
    token = (token_t *)array_shift(data);

    // directly output
    ret = process_iterator_print_tag(token,data);
    destroy_token(token);
    free(token);
    if (ret < 0) return ret;

    return 0;
  }
  else if(rtype == PARSETPL_TOK_IF || rtype == PARSETPL_TOK_ELSE || rtype == PARSETPL_TOK_ELSIF || rtype == PARSETPL_TOK_ENDIF) return process_if_tag(data, 0);
  else if(rtype == PARSETPL_TOK_INCLUDE) {
    token = (token_t*)array_shift(data);
    destroy_token(token); free(token);
    
    if(!data->elements) return PARSETPL_ERR_INVALIDTAG;
    token = (token_t*)array_shift(data);

    if(token->type != PARSETPL_TOK_WHITESPACE) {
      destroy_token(token); free(token);
      return PARSETPL_ERR_INVALIDTAG;
    }

    while(token->type == PARSETPL_TOK_WHITESPACE) {
      destroy_token(token); free(token);
      if(!data->elements) return PARSETPL_ERR_INVALIDTAG;
      token = (token_t*)array_shift(data);
    }

    if(token->type != PARSETPL_TOK_STRING || data->elements) {
      destroy_token(token); free(token);
      return PARSETPL_ERR_INVALIDTAG;
    }

    return process_include_tag(token->data);
  }
  else if(rtype == PARSETPL_TOK_FOREACH || rtype == PARSETPL_TOK_ENDFOREACH) return process_foreach_tag(data);

  return PARSETPL_ERR_INVALIDTAG;
}
/* }}} */

/* eof */
