/**
 * \file template.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
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
void tpl_cf_cleanup_var(void *data) {
  t_cf_tpl_variable *v = (t_cf_tpl_variable *)data;

  str_cleanup(v->data);
  free(v->data);
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
int tpl_cf_init(t_cf_template *tpl,const u_char *fname) {
  if((tpl->tpl = dlopen(fname,RTLD_LAZY)) == NULL) {
    fprintf(stderr,"%s\n",dlerror());
    return -1;
  }

  str_init(&tpl->parsed);

  tpl->varlist = cf_hash_new(tpl_cf_cleanup_var);

  return 0;
}

/*
 * Returns: nothing
 * Parameters:
 *   - t_cf_template *tpl    a pointer to the template variable
 *   - const u_char *name     the name of the variable
 *   - const u_char *val      the value of the variable
 *   - int len               the length of the content in val
 *
 * this function sets a variable value
 *
 */
void tpl_cf_setvar(t_cf_template *tpl,u_char *vname,const u_char *value,int len,int escapehtml) {
  t_cf_tpl_variable var;

  var.data = fo_alloc(NULL,1,sizeof(t_string),FO_ALLOC_CALLOC);
  var.escape_html = escapehtml;

  str_char_set(var.data,value,len);

  cf_hash_set(tpl->varlist,vname,strlen(vname),&var,sizeof(t_cf_tpl_variable));
}

/*
 * Returns: nothing
 * Parameters:
 *   - t_cf_template *tpl    a pointer to the template variable
 *   - const u_char *name     the name of the variable
 *   - const u_char *val      the value of the variable
 *   - int len               the length of the content in val
 *
 * this function appends a value to a variable
 *
 */
int tpl_cf_appendvar(t_cf_template *tpl,u_char *vname,const u_char *value,int len) {
  t_cf_tpl_variable *var = (t_cf_tpl_variable *)cf_hash_get(tpl->varlist,vname,strlen(vname));

  if(var) {
    str_chars_append(var->data,value,len);
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
void tpl_cf_freevar(t_cf_template *tpl,u_char *vname) {
  cf_hash_entry_delete(tpl->varlist,vname,strlen(vname));
}

/*
 * Returns: nothing
 * Parameters:
 *   - t_cf_template *tpl    a pointer to the template variable
 *
 * this function starts the parsing process
 *
 */
void tpl_cf_parse(t_cf_template *tpl) {
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
void tpl_cf_parse_to_mem(t_cf_template *tpl) {
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
void tpl_cf_finish(t_cf_template *tpl) {
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
const t_cf_tpl_variable *tpl_cf_getvar(t_cf_template *tpl,u_char *name) {
  t_cf_tpl_variable *var = (t_cf_tpl_variable *)cf_hash_get(tpl->varlist,name,strlen(name));

  return (const t_cf_tpl_variable *)var;
}

/* eof */
