/**
 * \file template.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 *         Christian Seiler, <self@christian-seiler.de>
 * \brief the template library function implementations
 */

/* {{{ Initial headers */
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

#include <dlfcn.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>

#include <string.h>

#include "utils.h"
#include "hashlib.h"
#include "template.h"
/* }}} */

#ifndef DOXYGEN
/* {{{ cf_tpl_cleanup_var */
void cf_tpl_cleanup_var(void *data) {
  cf_tpl_variable_t *v = (cf_tpl_variable_t *)data;

  cf_tpl_var_destroy(v);
}
/* }}} */
#endif

/* {{{ cf_tpl_init */
int cf_tpl_init(cf_template_t *tpl,const u_char *fname) {
  if(fname) {
    if((tpl->tpl = dlopen(fname,RTLD_LAZY)) == NULL) {
      fprintf(stderr,"%s\n",dlerror());
      return -1;
    }

    tpl->filename = strdup(fname);

  }
  else {
    tpl->tpl = NULL;
    tpl->filename = NULL;
  }

  str_init(&tpl->parsed);
  tpl->varlist = cf_hash_new(cf_tpl_cleanup_var);

  return 0;
}
/* }}} */

/* {{{ cf_tpl_copyvars */
void cf_tpl_copyvars(cf_template_t *dst,cf_template_t *src, int clone) {
  cf_tpl_variable_t *tmp_var;
  long is_arrayref;
  cf_hash_keylist_t *key;

  for(key=src->varlist->keys.elems;key;key=key->next) {
    if((tmp_var = cf_hash_get(src->varlist,key->key,strlen(key->key))) == NULL) continue;

    if(clone) {
      if((tmp_var = cf_tpl_var_clone(tmp_var)) == NULL) continue;
    }

    is_arrayref = tmp_var->arrayref;

    if(clone) tmp_var->temporary = 1;
    else tmp_var->arrayref = 1;

    cf_tpl_setvar(dst,key->key,tmp_var);

    if(!clone && !is_arrayref) tmp_var->arrayref = 0;
  }
}
/* }}} */

/* {{{ cf_tpl_setvar */
void cf_tpl_setvar(cf_template_t *tpl,const u_char *vname,cf_tpl_variable_t *var) {
  int tmp = 0;

  if(var->temporary) tmp = 1;

  var->temporary = 0;
  cf_hash_set(tpl->varlist,(u_char *)vname,strlen(vname),var,sizeof(cf_tpl_variable_t));

  // cleanup if it's temporary
  if(tmp) free(var); // don't destroy the var, only free it
}
/* }}} */

/* {{{ cf_tpl_setvalue */
void cf_tpl_setvalue(cf_template_t *tpl,const u_char *vname,unsigned short type,...) {
  va_list ap;
  signed long int_value;
  const u_char *string_value;
  int string_len;
  cf_tpl_variable_t var;
  
  va_start(ap,type);
  switch(type) {
    case TPL_VARIABLE_STRING:
      string_value = va_arg(ap,const u_char *);
      string_len   = va_arg(ap,int);

      cf_tpl_var_init(&var,type);
      str_char_set(&var.data.d_string,string_value,string_len);
      cf_tpl_setvar(tpl,vname,&var);
      break;

    case TPL_VARIABLE_INT:
      int_value = va_arg(ap,signed long);
      cf_tpl_var_init(&var,type);
      var.data.d_int = int_value;
      cf_tpl_setvar(tpl,vname,&var);
      break;

    case TPL_VARIABLE_ARRAY:
      // arrays may NOT be set this way!
    case TPL_VARIABLE_HASH:
      // hashes neither
    default:
      // illegal
      break;
  }

  va_end(ap);
}
/* }}} */

/* {{{ cf_tpl_appendvalue */
int cf_tpl_appendvalue(cf_template_t *tpl,const u_char *vname,const u_char *value,int len) {
  cf_tpl_variable_t *var = (cf_tpl_variable_t *)cf_hash_get(tpl->varlist,(u_char*)vname,strlen(vname));

  if(var && var->type == TPL_VARIABLE_STRING) {
    str_chars_append(&var->data.d_string,value,len);
    return 0;
  }

  return -1;
}
/* }}} */

/* {{{ cf_tpl_freevar */
void cf_tpl_freevar(cf_template_t *tpl,const u_char *vname) {
  cf_hash_entry_delete(tpl->varlist,(u_char *)vname,strlen(vname));
}
/* }}} */

/* {{{ cf_tpl_var_init */
void cf_tpl_var_init(cf_tpl_variable_t *var,unsigned short type) {
  var->type      = type;
  var->temporary = 0;
  var->arrayref  = 0;

  switch(type) {
    case TPL_VARIABLE_STRING:
      str_init(&var->data.d_string);
      break;

    case TPL_VARIABLE_INT:
      var->data.d_int = 0;
      break;

    case TPL_VARIABLE_ARRAY:
      array_init(&var->data.d_array,sizeof(cf_tpl_variable_t),cf_tpl_cleanup_var);
      break;

    case TPL_VARIABLE_HASH:
      var->data.d_hash = cf_hash_new(cf_tpl_cleanup_var);
      break;

    default:
      var->type = TPL_VARIABLE_INVALID;
      break;
  }
}
/* }}} */

/* {{{ cf_tpl_var_destroy */
void cf_tpl_var_destroy(cf_tpl_variable_t *var) {
  // if this is a variable created in a foreach loop => do nothing
  if(var->arrayref) return;

  switch(var->type) {
    case TPL_VARIABLE_STRING:
      str_cleanup(&var->data.d_string);
      break;

    case TPL_VARIABLE_INT:
      // do nothing
      break;

    case TPL_VARIABLE_ARRAY:
      array_destroy(&var->data.d_array);
      break;

    case TPL_VARIABLE_HASH:
      cf_hash_destroy(var->data.d_hash);
      break;
  }
}
/* }}} */

/* {{{ cf_tpl_var_setvalue */
void cf_tpl_var_setvalue(cf_tpl_variable_t *var,...) {
  va_list ap;
  int string_len;
  long int_value;
  const u_char *string_value;
  
  va_start(ap,var);

  switch(var->type) {
    case TPL_VARIABLE_STRING:
      string_value = va_arg(ap,const u_char *);
      string_len   = va_arg(ap,int);
      str_char_set(&var->data.d_string,string_value,string_len);
      break;

    case TPL_VARIABLE_INT:
      int_value = va_arg(ap,signed long);
      var->data.d_int = int_value;
      break;

    case TPL_VARIABLE_ARRAY:
      // arrays are not supported
    case TPL_VARIABLE_HASH:
      // hashes neither
    default:
      // invalid
      break;
  }

  va_end(ap);
}
/* }}} */

/* {{{ cf_tpl_var_convert */
cf_tpl_variable_t *cf_tpl_var_convert(cf_tpl_variable_t *dest,cf_tpl_variable_t *src,unsigned short new_type) {
  cf_tpl_variable_t *var;
  u_char intbuf[20]; /* this will not be longer than 20 digits */

  if(!src) return NULL;
  
  // other conversions are NOT possible
  if(new_type != TPL_VARIABLE_STRING && new_type != TPL_VARIABLE_INT) return NULL;
  if(src->type != TPL_VARIABLE_STRING && src->type != TPL_VARIABLE_INT && src->type != TPL_VARIABLE_ARRAY && src->type != TPL_VARIABLE_HASH) return NULL;

  if(!dest) var = fo_alloc(NULL,sizeof(cf_tpl_variable_t),1,FO_ALLOC_MALLOC);
  else var = dest;

  cf_tpl_var_init(var,new_type);
  if(!dest) var->temporary = 1;

  if(new_type == TPL_VARIABLE_STRING) {
    switch(src->type) {
      case TPL_VARIABLE_STRING:
        str_str_set(&var->data.d_string,&src->data.d_string);
        break;

      case TPL_VARIABLE_INT:
        snprintf(intbuf,19,"%ld",src->data.d_int);
        str_char_set(&var->data.d_string,intbuf,strlen(intbuf));
        break;

      case TPL_VARIABLE_ARRAY:
        var->data.d_string.len = 0;
        u_int32_to_str(&var->data.d_string,src->data.d_array.elements);
        break;

      case TPL_VARIABLE_HASH:
        var->data.d_string.len = 0;
        u_int32_to_str(&var->data.d_string,src->data.d_hash->elements);
        break;
    }

  }
  else if(new_type == TPL_VARIABLE_INT) {
    switch(src->type) {
      case TPL_VARIABLE_STRING:
        var->data.d_int = strtol(src->data.d_string.content,(char **)NULL,10);
        break;

      case TPL_VARIABLE_INT:
        var->data.d_int = src->data.d_int;
        break;

      case TPL_VARIABLE_ARRAY:
        var->data.d_int = (signed long)src->data.d_array.elements;
        break;

      case TPL_VARIABLE_HASH:
        var->data.d_int = (signed long)src->data.d_hash->elements;
        break;
    }
  }
  
  if(src->temporary) {
    cf_tpl_var_destroy(src);
    free(src);
  }

  return var;
}
/* }}} */

/* {{{ cf_tpl_var_clone */
cf_tpl_variable_t *cf_tpl_var_clone(cf_tpl_variable_t *var) {
  cf_hash_keylist_t *key;
  cf_tpl_variable_t *new_var, *tmp_var;
  size_t i;
  
  if(!var || (var->type != TPL_VARIABLE_STRING && var->type != TPL_VARIABLE_INT && var->type != TPL_VARIABLE_ARRAY && var->type != TPL_VARIABLE_HASH)) return NULL;
  
  new_var = (cf_tpl_variable_t *)fo_alloc(NULL,sizeof(cf_tpl_variable_t),1,FO_ALLOC_MALLOC);
  cf_tpl_var_init(new_var,var->type);
  
  switch(var->type) {
    case TPL_VARIABLE_STRING:
      str_str_set(&new_var->data.d_string,&var->data.d_string);
      break;

    case TPL_VARIABLE_INT:
      new_var->data.d_int = var->data.d_int;
      break;

    case TPL_VARIABLE_ARRAY:
      for(i = 0; i < var->data.d_array.elements; i++) {
        if((tmp_var = cf_tpl_var_clone((cf_tpl_variable_t *)array_element_at(&var->data.d_array,i))) == NULL) {
          cf_tpl_var_destroy(new_var);
          free(new_var);
          return NULL;
        }

        tmp_var->temporary = 1;
        cf_tpl_var_add(new_var,tmp_var);
      }
      break;

    case TPL_VARIABLE_HASH:
      for(key=var->data.d_hash->keys.elems;key;key=key->next) {
        tmp_var = cf_hash_get(var->data.d_hash,key->key,strlen(key->key));

        if((tmp_var = cf_tpl_var_clone(tmp_var)) == NULL) {
          cf_tpl_var_destroy(new_var);
          free(new_var);
          return NULL;
        }

        tmp_var->temporary = 1;
        cf_tpl_hashvar_set(new_var,key->key,tmp_var);
      }
      break;
  }
  
  return new_var;
}
/* }}} */

/* {{{ cf_tpl_var_add */
void cf_tpl_var_add(cf_tpl_variable_t *var,cf_tpl_variable_t *element) {
  int tmp = 0;

  if(var->type != TPL_VARIABLE_ARRAY) return;
  if(element->temporary) tmp = 1;

  element->temporary = 0;
  array_push(&var->data.d_array,element);

  // cleanup if it's temporary
  if(tmp) free(element); // don't destroy the var, only free it
}
/* }}} */

/* {{{ cf_tpl_var_addvalue */
void cf_tpl_var_addvalue(cf_tpl_variable_t *array_var,unsigned short type,...) {
  va_list ap;
  signed long int_value;
  const u_char *string_value;
  int string_len;
  cf_tpl_variable_t var;
  
  va_start(ap,type);

  switch(type) {
    case TPL_VARIABLE_STRING:
      string_value = va_arg(ap,const u_char *);
      string_len   = va_arg(ap,int);
      cf_tpl_var_init(&var,type);
      str_char_set(&var.data.d_string,string_value,string_len);
      cf_tpl_var_add(array_var,&var);
      break;

    case TPL_VARIABLE_INT:
      int_value      = va_arg(ap,signed long);
      cf_tpl_var_init(&var,type);
      var.data.d_int = int_value;
      cf_tpl_var_add(array_var,&var);
      break;

    case TPL_VARIABLE_ARRAY:
      // arrays may NOT be set this way!
    case TPL_VARIABLE_HASH:
      // hashes neither!
    default:
      // illegal
      break;
  }

  va_end(ap);
}
/* }}} */

/* {{{ cf_tpl_hashvar_set */
void cf_tpl_hashvar_set(cf_tpl_variable_t *var,const u_char *key,cf_tpl_variable_t *element) {
  int tmp = 0;

  if(var->type != TPL_VARIABLE_HASH) return;
  if(element->temporary) tmp = 1;

  element->temporary = 0;
  cf_hash_set(var->data.d_hash,(u_char *)key,strlen(key),element,sizeof(cf_tpl_variable_t));

  // cleanup if it's temporary
  if(tmp) free(element); // don't destroy the var, only free it
}
/* }}} */

/* {{{ cf_tpl_hashvar_setvalue */
void cf_tpl_hashvar_setvalue(cf_tpl_variable_t *hash_var,const u_char *key,unsigned short type,...) {
  va_list ap;
  signed long int_value;
  const u_char *string_value;
  int string_len;
  cf_tpl_variable_t var;
  
  va_start(ap,type);

  switch(type) {
    case TPL_VARIABLE_STRING:
      string_value = va_arg(ap,const u_char *);
      string_len   = va_arg(ap,int);
      cf_tpl_var_init(&var,type);
      str_char_set(&var.data.d_string,string_value,string_len);
      cf_tpl_hashvar_set(hash_var,key,&var);
      break;

    case TPL_VARIABLE_INT:
      int_value      = va_arg(ap,signed long);
      cf_tpl_var_init(&var,type);
      var.data.d_int = int_value;
      cf_tpl_hashvar_set(hash_var,key,&var);
      break;

    case TPL_VARIABLE_ARRAY:
      // arrays may NOT be set this way!
    case TPL_VARIABLE_HASH:
      // hashes neither!
    default:
      // illegal
      break;
  }

  va_end(ap);
}
/* }}} */

/* {{{ cf_tpl_parse */
void cf_tpl_parse(cf_template_t *tpl) {
  void *pa = dlsym(tpl->tpl,"parse");

  if(pa) {
    parse_t x = (parse_t)pa;
    x(tpl);
  }
}
/* }}} */

/* {{{ cf_tpl_parse_to_mem */
void cf_tpl_parse_to_mem(cf_template_t *tpl) {
  void *pa = dlsym(tpl->tpl,"parse_to_mem");

  if(pa) {
    parse_mem_t x = (parse_mem_t)pa;
    x(tpl);
  }
}
/* }}} */

/* {{{ cf_tpl_finish */
void cf_tpl_finish(cf_template_t *tpl) {
  if(tpl->tpl) dlclose(tpl->tpl);

  str_cleanup(&tpl->parsed);
  cf_hash_destroy(tpl->varlist);
}
/* }}} */

/* {{{ cf_tpl_getvar */
const cf_tpl_variable_t *cf_tpl_getvar(cf_template_t *tpl,const u_char *name) {
  cf_tpl_variable_t *var = (cf_tpl_variable_t *)cf_hash_get(tpl->varlist,(u_char *)name,strlen(name));

  return (const cf_tpl_variable_t *)var;
}
/* }}} */

/* eof */
