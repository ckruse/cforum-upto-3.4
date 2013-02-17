%module "CForum::Template"
%{
#include "config.h"
#include "defines.h"

#include "utils.h"
#include "hashlib.h"
#include "template.h"
%}

#define TPL_VARIABLE_INVALID      0
#define TPL_VARIABLE_STRING       1
#define TPL_VARIABLE_ARRAY        3
#define TPL_VARIABLE_HASH         4

typedef struct s_cf_tpl_variable {
  %immutable;
  int type;
  %mutable;
} cf_tpl_variable_t;

%extend cf_tpl_variable_t {
  cf_tpl_variable_t(int type);
  void setvalue(const char *value);
  cf_tpl_variable_t *clone();
  void add(cf_tpl_variable_t *element);
  void addvalue(const char *value);
  void h_set(const char *key,cf_tpl_variable_t *value);
  void h_setvalue(const char *key,const char *value);
}

/* {{{ cf_tpl_variable_t */
%{
cf_tpl_variable_t *new_cf_tpl_variable_t(int type) {
  cf_tpl_variable_t *var = fo_alloc(NULL,1,sizeof(*var),FO_ALLOC_MALLOC);

  cf_tpl_var_init(var,type);
  return var;
}

void cf_tpl_variable_t_setvalue(cf_tpl_variable_t *var,const char *value) {
  cf_tpl_var_setvalue(var,value,strlen(value));
}

cf_tpl_variable_t *cf_tpl_variable_t_clone(cf_tpl_variable_t *var) {
  return cf_tpl_var_clone(var);
}

void cf_tpl_variable_t_add(cf_tpl_variable_t *var,cf_tpl_variable_t *element) {
  cf_tpl_var_add(var,element);
}

void cf_tpl_variable_t_addvalue(cf_tpl_variable_t *var,const char *value) {
  cf_tpl_var_addvalue(var,TPL_VARIABLE_STRING,value,(int)strlen(value));
}

void cf_tpl_variable_t_h_set(cf_tpl_variable_t *var,const char *key,cf_tpl_variable_t *val) {
  cf_tpl_hashvar_set(var,key,val);
}

void cf_tpl_variable_t_h_setvalue(cf_tpl_variable_t *var,const char *key,const char *val) {
  cf_tpl_hashvar_setvalue(var,key,TPL_VARIABLE_STRING,val,strlen(val));
}
%}
/* }}} */

typedef struct s_cf_template {
  %immutable;
  char *filename;
  %mutable;
} cf_template_t;

%extend cf_template_t {
  cf_template_t(const char *filename);
  void setvar(const char *vname,cf_tpl_variable_t *var);
  void setvalue(const char *vname,const char *val);
  void appendvalue(const char *vname,const char *value);
  void parse();
  const char *parseToMem();
  const cf_tpl_variable_t *getvar(const char *vname);
}

/* {{{ cf_template_t */
%{
cf_template_t *new_cf_template_t(const char *filename) {
  cf_template_t *tpl = fo_alloc(NULL,1,sizeof(*tpl),FO_ALLOC_MALLOC);

  if(cf_tpl_init(tpl,filename) == -1) {
    free(tpl);
    return NULL;
  }

  return tpl;
}

void cf_template_t_setvar(cf_template_t *tpl,const char *vname,cf_tpl_variable_t *var) {
  cf_tpl_setvar(tpl,vname,var);
}

const cf_tpl_variable_t *cf_template_t_getvar(cf_template_t *tpl,const char *vname) {
  return cf_tpl_getvar(tpl,vname);
}

void cf_template_t_setvalue(cf_template_t *tpl,const char *vname,const char *val) {
  cf_tpl_setvalue(tpl,vname,TPL_VARIABLE_STRING,(const u_char *)val,(int)strlen(val));
}

void cf_template_t_appendvalue(cf_template_t *tpl,const char *vname,const char *value) {
  cf_tpl_appendvalue(tpl,vname,value,strlen(value));
}

void cf_template_t_parse(cf_template_t *tpl) {
  cf_tpl_parse(tpl);
}

char *cf_template_t_parseToMem(cf_template_t *tpl) {
  u_char *retval;

  cf_tpl_parse_to_mem(tpl);
  retval = tpl->parsed.content;
  str_init(&tpl->parsed);

  return retval;
}
%}
/* }}} */

/* eof */
