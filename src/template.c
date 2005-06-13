/**
 * \file template.c
 * \author Christian Kruse, <ckruse@wwwtech.de>,
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
/*
 * Returns:          nothing
 * Parameters:
 *   - void *data    the entry data
 *
 * this function cleans up hash entry
 *
 */
void cf_tpl_cleanup_var(void *data) {
  t_cf_tpl_variable *v = (t_cf_tpl_variable *)data;

  cf_tpl_var_destroy(v);
}
#endif

/*
 * Returns: nothing
 * Parameters:
 *   - const u_char *file      the absolute path to the file
 *   - t_cf_template *tpl    a pointer to the template variable
 *
 * this function binds a template lib
 *
 */
int cf_tpl_init(t_cf_template *tpl,const u_char *fname) {
  if(fname) {
    if((tpl->tpl = dlopen(fname,RTLD_LAZY)) == NULL) {
      fprintf(stderr,"%s\n",dlerror());
      return -1;
    }
    tpl->filename = strdup(fname);
  } else {
    tpl->tpl = NULL;
    tpl->filename = NULL;
  }

  str_init(&tpl->parsed);
  tpl->varlist = cf_hash_new(cf_tpl_cleanup_var);

  return 0;
}

/*
 * Returns: nothing
 * Parameters:
 *   - t_cf_template *dst    a pointer to the destination template variable
 *   - t_cf_template *src    a pointer to the source template variable
 *
 * copies template vars from one template to another
 *
 */
void cf_tpl_copyvars(t_cf_template *dst,t_cf_template *src) {
  t_cf_hash_entry *ent;
  t_cf_tpl_variable *tmp_var;
  long i;
  
  for(i=0;i<hashsize(src->varlist->tablesize);i++) {
    if(!src->varlist->table[i]) {
      continue;
    }
    for(ent = src->varlist->table[i];ent;ent=ent->next) {
      tmp_var = cf_tpl_var_clone((t_cf_tpl_variable *)ent->data);
      if(!tmp_var) {
        continue;
      }
      tmp_var->temporary = 1;
      cf_tpl_setvar(dst,ent->key,tmp_var);
    }
  }
}

/*
 * Returns: nothing
 * Parameters:
 *   - t_cf_template *tpl       a pointer to the template variable
 *   - const u_char *vname      the name of the varialbe
 *   - t_cf_tpl_variable *var   the variable that is to be added
 */
void cf_tpl_setvar(t_cf_template *tpl,const u_char *vname,t_cf_tpl_variable *var) {
  int tmp = 0;
  if(var->temporary) {
    tmp = 1;
  }
  var->temporary = 0;
  cf_hash_set(tpl->varlist,(u_char *)vname,strlen(vname),var,sizeof(t_cf_tpl_variable));
  // cleanup if it's temporary
  if(tmp) {
    // don't destroy the var, only free it
    free(var);
  }
}

/*
 * Returns: nothing
 * Parameters:
 *   - t_cf_template *tpl       a pointer to the template variable
 *   - const u_char *vname      the name of the varialbe
 *   - unsigned short type      the type of the variable
 *                              (TPL_VARIABLE_*)
 *      - signed long value     the integer value
 *    or:
 *      - const u_char *value   the string value
 *      - int len               the length of the value
 */
void cf_tpl_setvalue(t_cf_template *tpl,const u_char *vname,unsigned short type,...) {
  va_list ap;
  signed long int_value;
  const u_char *string_value;
  int string_len;
  t_cf_tpl_variable var;
  
  va_start(ap,type);
  switch(type) {
    case TPL_VARIABLE_STRING:
      string_value = va_arg(ap,const u_char *);
      string_len = va_arg(ap,int);
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

/*
 * Returns: nothing
 * Parameters:
 *   - t_cf_template *tpl    a pointer to the template variable
 *   - const u_char *vname   the name of the variable
 *   - const u_char *value   the value of the variable
 *   - int len               the length of the content in val
 *
 * this function appends a value to a variable
 *
 */
int cf_tpl_appendvalue(t_cf_template *tpl,const u_char *vname,const u_char *value,int len) {
  t_cf_tpl_variable *var = (t_cf_tpl_variable *)cf_hash_get(tpl->varlist,(u_char*)vname,strlen(vname));

  if(var && var->type == TPL_VARIABLE_STRING) {
    str_chars_append(&var->data.d_string,value,len);
    return 0;
  }

  return -1;
}

/*
 * Returns: nothing
 * Parameters:
 *   - t_cf_template *tpl    a pointer to the template variable
 *   - const u_char *name     the name of the variable
 *
 * this function frees a template variable
 *
 */
void cf_tpl_freevar(t_cf_template *tpl,const u_char *vname) {
  cf_hash_entry_delete(tpl->varlist,(u_char *)vname,strlen(vname));
}

/*
 * Returns: nothing
 * Parameters:
 *   - t_cf_tpl_variable *var    a pointer to the variable structure
 *   - unsigned short type       the type of the variable
 *
 * this function initializes a template variable
 *
 */
void cf_tpl_var_init(t_cf_tpl_variable *var,unsigned short type) {
  var->type = type;
  var->temporary = 0;
  var->arrayref = 0;
  switch(type) {
    case TPL_VARIABLE_STRING:
      str_init(&var->data.d_string);
      break;
    case TPL_VARIABLE_INT:
      var->data.d_int = 0;
      break;
    case TPL_VARIABLE_ARRAY:
      array_init(&var->data.d_array,sizeof(t_cf_tpl_variable),cf_tpl_cleanup_var);
      break;
    case TPL_VARIABLE_HASH:
      var->data.d_hash = cf_hash_new(cf_tpl_cleanup_var);
      break;
    default:
      var->type = TPL_VARIABLE_INVALID;
      break;
  }
}

/*
 * Returns: nothing
 * Parameters:
 *   - t_cf_tpl_variable *var    a pointer to the variable structure
  *
 * this function frees a template variable
 *
 */
void cf_tpl_var_destroy(t_cf_tpl_variable *var) {
  // if this is a variable created in a foreach loop => do nothing
  if(var->arrayref) {
    return;
  }
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

/*
 * Returns: nothing
 * Parameters:
 *   - t_cf_tpl_variable *var    a pointer to the variable structure
 *
 *      - signed long value     the integer value
 *    or:
 *      - const u_char *value   the string value
 *      - int len               the length of the value
 *
 * this function frees a template variable
 *
 */
void cf_tpl_var_setvalue(t_cf_tpl_variable *var,...) {
  va_list ap;
  signed long int_value;
  const u_char *string_value;
  int string_len;
  
  va_start(ap,var);
  switch(var->type) {
    case TPL_VARIABLE_STRING:
      string_value = va_arg(ap,const u_char *);
      string_len = va_arg(ap,int);
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

/*
 * Returns: The destination variable that contains the converted value
 * Parameters:
 *   - t_cf_tpl_variable *dest    a pointer to the destination variable structure or NULL
 *                                if a new one is to be created. should be not initialized
 *   - t_cf_tpl_variable *src     a pointer to the source variable structure
 *   - unsigned short new_type    the new type of the variable 
 *
 * this function converts a template variable from one type to another
 *
 */
t_cf_tpl_variable *cf_tpl_var_convert(t_cf_tpl_variable *dest,t_cf_tpl_variable *src,unsigned short new_type) {
  t_cf_tpl_variable *var;
  u_char intbuf[20]; /* this will not be longer than 20 digits */

  if(!src) return NULL;
  
  // other conversions are NOT possible
  if(new_type != TPL_VARIABLE_STRING && new_type != TPL_VARIABLE_INT) {
    return NULL;
  }
  if(src->type != TPL_VARIABLE_STRING && src->type != TPL_VARIABLE_INT && src->type != TPL_VARIABLE_ARRAY && src->type != TPL_VARIABLE_HASH) {
    return NULL;
  }
  if(!dest) {
    var = fo_alloc(NULL,sizeof(t_cf_tpl_variable),1,FO_ALLOC_MALLOC);
  } else {
    var = dest;
  }
  cf_tpl_var_init(var,new_type);
  if(!dest) {
    var->temporary = 1;
  }
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
        snprintf(intbuf,19,"%ld",src->data.d_array.elements);
        str_char_set(&var->data.d_string,intbuf,strlen(intbuf));
        break;
      case TPL_VARIABLE_HASH:
        snprintf(intbuf,19,"%ld",src->data.d_hash->elements);
        str_char_set(&var->data.d_string,intbuf,strlen(intbuf));
        break;
    }
  } else if(new_type == TPL_VARIABLE_INT) {
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

/*
 * Returns: The cloned variable
 * Parameters:
 *   - t_cf_tpl_variable *var     a pointer to the variable structure
 *
 * this function clones a template variable and all subvariables
 *
 */
t_cf_tpl_variable *cf_tpl_var_clone(t_cf_tpl_variable *var) {
  t_cf_tpl_variable *new_var, *tmp_var;
  long i;
  
  t_cf_hash_entry *ent;
  
  if(!var || (var->type != TPL_VARIABLE_STRING && var->type != TPL_VARIABLE_INT && var->type != TPL_VARIABLE_ARRAY && var->type != TPL_VARIABLE_HASH)) {
    return NULL;
  }
  
  new_var = (t_cf_tpl_variable *)fo_alloc(NULL,sizeof(t_cf_tpl_variable),1,FO_ALLOC_MALLOC);
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
        tmp_var = cf_tpl_var_clone((t_cf_tpl_variable *)array_element_at(&var->data.d_array,i));
        if(!tmp_var) {
          cf_tpl_var_destroy(new_var);
          free(new_var);
          return NULL;
        }
        tmp_var->temporary = 1;
        cf_tpl_var_add(new_var,tmp_var);
      }
      break;
    case TPL_VARIABLE_HASH:
      for(i=0;i<hashsize(var->data.d_hash->tablesize);i++) {
        if(!var->data.d_hash->table[i]) {
          continue;
        }
        for(ent = var->data.d_hash->table[i];ent;ent=ent->next) {
          tmp_var = cf_tpl_var_clone((t_cf_tpl_variable *)ent->data);
          if(!tmp_var) {
            cf_tpl_var_destroy(new_var);
            free(new_var);
            return NULL;
          }
          tmp_var->temporary = 1;
          cf_tpl_hashvar_set(new_var,ent->key,tmp_var);
        }
      }
      break;
  }
  
  return new_var;
}

/*
 * Returns: The destination variable that contains the converted value
 * Parameters:
 *   - t_cf_tpl_variable *var     a pointer to the variable structure
 *   - t_cf_tpl_variable *element a pointer to the element that is to be added
 *
 * This function adds an element to an array
 *
 */
void cf_tpl_var_add(t_cf_tpl_variable *var,t_cf_tpl_variable *element) {
  int tmp = 0;
  if(var->type != TPL_VARIABLE_ARRAY) {
    return;
  }
  if(element->temporary) {
    tmp = 1;
  }
  element->temporary = 0;
  array_push(&var->data.d_array,element);
  // cleanup if it's temporary
  if(tmp) {
    // don't destroy the var, only free it
    free(element);
  }
}

/*
 * Parameters:
 *   - t_cf_tpl_variable *array_var   a pointer to the variable structure
 *   - unsigned short type            the type of the new element
 *      - signed long value           the integer value
 *    or:
 *      - const u_char *value         the string value
 *      - int len                     the length of the value
 *
 * This function adds an element to an array by its value. Only string and integer values are supported
 *
 */
void cf_tpl_var_addvalue(t_cf_tpl_variable *array_var,unsigned short type,...) {
  va_list ap;
  signed long int_value;
  const u_char *string_value;
  int string_len;
  t_cf_tpl_variable var;
  
  va_start(ap,type);
  switch(type) {
    case TPL_VARIABLE_STRING:
      string_value = va_arg(ap,const u_char *);
      string_len = va_arg(ap,int);
      cf_tpl_var_init(&var,type);
      str_char_set(&var.data.d_string,string_value,string_len);
      cf_tpl_var_add(array_var,&var);
      break;
    case TPL_VARIABLE_INT:
      int_value = va_arg(ap,signed long);
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

/*
 * Parameters:
 *   - t_cf_tpl_variable *var     a pointer to the variable structure
 *   - const u_char *key          the key of the element
 *   - t_cf_tpl_variable *element a pointer to the element that is to be set
 *
 * This function sets an element of a hash
 *
 */
void cf_tpl_hashvar_set(t_cf_tpl_variable *var,const u_char *key,t_cf_tpl_variable *element) {
  int tmp = 0;
  if(var->type != TPL_VARIABLE_HASH) {
    return;
  }
  if(element->temporary) {
    tmp = 1;
  }
  element->temporary = 0;
  cf_hash_set(var->data.d_hash,(u_char *)key,strlen(key),element,sizeof(t_cf_tpl_variable));
  // cleanup if it's temporary
  if(tmp) {
    // don't destroy the var, only free it
    free(element);
  }
}

/*
 * Parameters:
 *   - t_cf_tpl_variable *hash_var    a pointer to the variable structure
 *   - unsigned short type            the type of the new element
 *      - signed long value           the integer value
 *    or:
 *      - const u_char *value         the string value
 *      - int len                     the length of the value
 *
 * This function sets an element of a hash by its value. Only string and integer values are supported
 *
 */
void cf_tpl_hashvar_setvalue(t_cf_tpl_variable *hash_var,const u_char *key,unsigned short type,...) {
  va_list ap;
  signed long int_value;
  const u_char *string_value;
  int string_len;
  t_cf_tpl_variable var;
  
  va_start(ap,type);
  switch(type) {
    case TPL_VARIABLE_STRING:
      string_value = va_arg(ap,const u_char *);
      string_len = va_arg(ap,int);
      cf_tpl_var_init(&var,type);
      str_char_set(&var.data.d_string,string_value,string_len);
      cf_tpl_hashvar_set(hash_var,key,&var);
      break;
    case TPL_VARIABLE_INT:
      int_value = va_arg(ap,signed long);
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

/*
 * Returns: nothing
 * Parameters:
 *   - t_cf_template *tpl    a pointer to the template variable
 *
 * this function starts the parsing process
 *
 */
void cf_tpl_parse(t_cf_template *tpl) {
  void *pa = dlsym(tpl->tpl,"parse");

  if(pa) {
    t_parse x = (t_parse)pa;
    x(tpl);
  }
}

/*
 * Returns: nothing
 * Parameters:
 *   - t_cf_template *tpl    a pointer to the template variable
 *
 * this function starts the parsing process and spits it out
 * to memory
 *
 */
void cf_tpl_parse_to_mem(t_cf_template *tpl) {
  void *pa = dlsym(tpl->tpl,"parse_to_mem");

  if(pa) {
    t_parse_mem x = (t_parse_mem)pa;
    x(tpl);
  }
}

/*
 * Returns: nothing
 * Parameters:
 *   - t_cf_template *tpl    a pointer to the template variable
 *
 * this function frees the internal template structures
 *
 */
void cf_tpl_finish(t_cf_template *tpl) {
  if(tpl->tpl) {
    dlclose(tpl->tpl);
  }
  str_cleanup(&tpl->parsed);
  cf_hash_destroy(tpl->varlist);
}

/*
 * Returns: t_cf_variable *  a pointer to the variable
 * Parameters:
 *   - t_cf_template *tpl    a pointer to the template variable
 *   - const u_char *name     the variable name
 *
 * this function searches a template variable and returns it
 * if not found, it returns NULL
 *
 */
const t_cf_tpl_variable *cf_tpl_getvar(t_cf_template *tpl,const u_char *name) {
  t_cf_tpl_variable *var = (t_cf_tpl_variable *)cf_hash_get(tpl->varlist,(u_char *)name,strlen(name));

  return (const t_cf_tpl_variable *)var;
}

/* eof */
