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

typedef struct s_cf_tpl_variable {
  %immutable;
  int type;
  %mutable;
} t_cf_tpl_variable;

%extend t_cf_tpl_variable {
  t_cf_tpl_variable(int type);
  void setvalue(const char *value);
  t_cf_tpl_variable *clone();
  void add(t_cf_tpl_variable *element);
  void addvalue(const char *value);
}

/* {{{ t_cf_tpl_variable */
%{
t_cf_tpl_variable *new_t_cf_tpl_variable(int type) {
  t_cf_tpl_variable *var = fo_alloc(NULL,1,sizeof(*var),FO_ALLOC_MALLOC);

  cf_tpl_var_init(var,type);
  return var;
}

void t_cf_tpl_variable_setvalue(t_cf_tpl_variable *var,const char *value) {
  cf_tpl_var_setvalue(var,value);
}

t_cf_tpl_variable *t_cf_tpl_variable_clone(t_cf_tpl_variable *var) {
  return cf_tpl_var_clone(var);
}

void t_cf_tpl_variable_add(t_cf_tpl_variable *var,t_cf_tpl_variable *element) {
  cf_tpl_var_add(var,element);
}

void t_cf_tpl_variable_addvalue(t_cf_tpl_variable *var,const char *value) {
  cf_tpl_var_addvalue(var,TPL_VARIABLE_STRING,value);
}
%}
/* }}} */

typedef struct s_cf_template {
  %immutable;
  char *filename;
  %mutable;
} t_cf_template;

%extend t_cf_template {
  t_cf_template(const char *filename);
  void setvar(const char *vname,t_cf_tpl_variable *var);
  void setvalue(const char *vname,const char *val);
  void appendvalue(const char *vname,const char *value);
  void parse();
  const char *parseToMem();
  const t_cf_tpl_variable *getvar(const char *vname);
}

/* {{{ t_cf_template */
%{
t_cf_template *new_t_cf_template(const char *filename) {
  t_cf_template *tpl = fo_alloc(NULL,1,sizeof(*tpl),FO_ALLOC_MALLOC);

  if(cf_tpl_init(tpl,filename) == -1) {
    free(tpl);
    return NULL;
  }

  return tpl;
}

void t_cf_template_setvar(t_cf_template *tpl,const char *vname,t_cf_tpl_variable *var) {
  cf_tpl_setvar(tpl,vname,var);
}

void t_cf_template_setvalue(t_cf_template *tpl,const char *vname,const char *val) {
  cf_tpl_setvalue(tpl,vname,TPL_VARIABLE_STRING,(const u_char *)val,(int)strlen(val));
}

void t_cf_template_appendvalue(t_cf_template *tpl,const char *vname,const char *value) {
  cf_tpl_appendvalue(tpl,vname,value,strlen(value));
}

void t_cf_template_parse(t_cf_template *tpl) {
  cf_tpl_parse(tpl);
}

char *t_cf_template_parseToMem(t_cf_template *tpl) {
  u_char *retval;

  cf_tpl_parse_to_mem(tpl);
  retval = tpl->parsed.content;
  str_init(&tpl->parsed);

  return retval;
}
%}
/* }}} */

/* eof */
