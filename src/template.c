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
  if((tpl->tpl = dlopen(fname,RTLD_LAZY)) == NULL) {
    fprintf(stderr,"%s\n",dlerror());
    return -1;
  }

  str_init(&tpl->parsed);
  
  tpl->filename = strdup(fname);
  tpl->varlist = cf_hash_new(cf_tpl_cleanup_var);

  return 0;
}

/*
 * Returns: nothing
 * Parameters:
 *   - t_cf_template *tpl       a pointer to the template variable
 *   - const u_char *vname      the name of the varialbe
 *   - t_cf_tpl_variable *var   the variable that is to be added
 */
void cf_tpl_setvar(t_cf_template *tpl,const u_char *vname,t_cf_tpl_variable *var) {
   cf_hash_set(tpl->varlist,(u_char *)vname,strlen(vname),var,sizeof(t_cf_tpl_variable));
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

  if(!dest || !src) return NULL;
  
  // other conversions are NOT possible
  if(new_type != TPL_VARIABLE_STRING && new_type != TPL_VARIABLE_INT) {
    return NULL;
  }
  if(src->type != TPL_VARIABLE_STRING && src->type != TPL_VARIABLE_INT && src->type != TPL_VARIABLE_ARRAY) {
    return NULL;
  }
  if(src->type == TPL_VARIABLE_ARRAY && new_type != TPL_VARIABLE_INT) {
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
    }
  }
  
  if(src->temporary) {
    cf_tpl_var_destroy(src);
    free(src);
  }
  return var;
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
  if(var->type != TPL_VARIABLE_ARRAY) {
    return;
  }
  array_push(&var->data.d_array,element);
  // cleanup if it's temporary
  if(element->temporary) {
    cf_tpl_var_destroy(element);
    free(element);
  }
}

/*
 * Returns: The destination variable that contains the converted value
 * Parameters:
 *   - t_cf_tpl_variable *array_var   a pointer to the variable structure
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
  dlclose(tpl->tpl);
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
