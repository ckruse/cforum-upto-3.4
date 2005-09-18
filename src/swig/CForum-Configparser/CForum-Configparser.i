%module "CForum::Configparser"
%{

#include "config.h"
#include "defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include <dlfcn.h>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/uio.h>

#include <pwd.h>

#include "utils.h"
#include "hashlib.h"
#include "configparser.h"

static SV *callback = NULL;

#ifndef ENTER
#define ENTER push_scope()
#endif

typedef struct s_xs_conf {
  SV *data;
  SV *callback;
} xs_conf_t;

/* {{{ xs_callback */
int xs_callback(configfile_t *file,conf_opt_t *entry,const u_char *context,u_char **args,size_t len) {
  size_t i;
  int ret;
  xs_conf_t *data = (xs_conf_t *)entry->data;

  dSP;

  ENTER;
  SAVETMPS;

  PUSHMARK(SP);

  /* put the name (a copy), the context and the userdata (what the user gave us) on the stack */
  XPUSHs(sv_2mortal(newSVpv(entry->name,strlen(entry->name))));
  if(context) XPUSHs(sv_2mortal(newSVpv(context,strlen(context))));
  else XPUSHs(sv_2mortal(newSVpv("_global",7)));
  XPUSHs(data->data);

  /* put arguments on the stack */
  for(i=0;i<len;i++) XPUSHs(sv_2mortal(newSVpv(args[i], strlen(args[i]))));

  PUTBACK;

  /* the user dared it to give us a callback routine. Now, ok, lets go for it then */
  ret = call_sv(callback,G_SCALAR);

  /* refresh stack */
  SPAGAIN;

  /* wah! at least one return value. If none, return error */
  if(ret == 1) i = POPi;
  else i = 0;

  FREETMPS;
  LEAVE;

  return i;
}
/* }}} */

/* {{{ xs_callback_dflt */
int xs_callback_dflt(configfile_t *cfile,const u_char *context,u_char *name,u_char **args,size_t len) {
  size_t i;
  int ret;

  dSP;

  ENTER;
  SAVETMPS;

  PUSHMARK(SP);

  /* put the name (a copy), the context and the userdata (what the user gave us) on the stack */
  XPUSHs(sv_2mortal(newSVpv(name,strlen(name))));
  if(context) XPUSHs(sv_2mortal(newSVpv(context,strlen(context))));
  else XPUSHs(sv_2mortal(newSVpv("_global",7)));

  /* put arguments on the stack */
  for(i=0;i<len;++i) XPUSHs(sv_2mortal(newSVpv(args[i], strlen(args[i]))));

  PUTBACK;

  /* the user dared it to give us a callback routine. Now, ok, lets go for it then */
  ret = call_sv(callback,G_SCALAR);

  /* refresh stack */
  SPAGAIN;

  /* wah! at least one return value. If none, return error */
  if(ret == 1) i = POPi;
  else i = 0;

  FREETMPS;
  LEAVE;

  return i;
}
/* }}} */

%}

%init %{
  cfg_init();
  init_modules();
%}

/* include general typemaps */
%include "../typemaps.i";

/* {{{ typemap for configuration option */
%typemap(out) conf_opt_t * {
  AV *tempav,*tempav1;
  I32 len,len1;
  SV  **tv,**tv1;
  int i;
  xs_conf_t conf;

  if(!SvROK($input)) croak("Argument $argnum is not a reference.");
  if(SvTYPE(SvRV($input)) != SVt_PVAV) croak("Argument $argnum is not an array.");

  tempav = (AV*)SvRV($input);
  len = av_len(tempav);
  $1 = (conf_opt_t *)fo_alloc(NULL,len+2,sizeof(conf_opt_t *),FO_ALLOC_MALLOC);

  for(i=0;i<=len;++i) {
    tv = av_fetch(tempav, i, 0);
    if(!SvROK(*tv) || SvTYPE(SvRV(*tv)) != SVt_PVAV) croak("argument $argnum consists not of valid configuration option entries");

    tempav1 = (AV *)SvRV(*tv);
    len1 = av_len(tempav1);
    if(len1 != 3) croak("argument $argnum consists not of valid configuration option entries");

    tv1 = av_fetch(tempav1,0,0);
    $1[i].name = (u_char *)SvPV(*tv,PL_na);
    $1[i].callback = xs_callback;

    tv1 = av_fetch(tempav1,1,0);
    if(!SvROK(*tv1) || SvTYPE(SvRV(*tv)) != SVt_PVCV) croak("argument $argnum consists not of valid configuration option entries");
    conf.callback = *tv1;

    tv1 = av_fetch(tempav1,2,0);
    $1[i].flags = sv_2iv(*tv1);

    tv1 = av_fetch(tempav1,3,0);
    conf.data = *tv1;

    $1[i].data = memdup(&conf,sizeof(conf));
  }

  $1[i] = NULL;

};
/* }}} */


/* {{{ constants */
#define CFG_OPT_NEEDED     (0x1<<0)
#define CFG_OPT_CONFIG     (0x1<<1)
#define CFG_OPT_USER       (0x1<<2)
#define CFG_OPT_UNIQUE     (0x1<<3)
#define CFG_OPT_GLOBAL     (0x1<<4)
#define CFG_OPT_LOCAL      (0x1<<5)
#define CFG_OPT_NOOVERRIDE (0x1<<6)

#define CFG_OPT_SEEN       (0x1<<7)

#define CFG_MODE_CONFIG CFG_OPT_CONFIG
#define CFG_MODE_USER   CFG_OPT_USER
#define CFG_MODE_NOLOAD (0x1<<8)
/* }}} */


/* {{{ configfile_t declarations */
typedef struct s_configfile {
  char *filename;
} configfile_t;
%extend configfile_t {
  configfile_t(char *filename);
  int register_options(conf_opt_t *opts);
  int read(int mode,SV *cllbck = NULL);
}

%{
configfile_t *new_configfile_t(u_char *filename) {
  configfile_t *cfg = malloc(sizeof(*cfg));
  cfg_init_file(cfg,filename);
  return cfg;
}

int configfile_t_register_options(configfile_t *self,conf_opt_t *opts) {
  return cfg_register_options(self,opts);
}

int configfile_t_read(configfile_t *self,int mode,SV *cllbck) {
  int ret;

  if(cllbck) {
    callback = cllbck;
    ret = read_config(self,xs_callback_dflt,mode);
  }
  else ret = read_config(self,NULL,mode);

  return ret;
}
%}
/* }}} */

/* {{{ configuration_t declarations */
typedef struct s_configuration {} configuration_t;
%extend configuration_t {
  name_value_t *get_first_value(const char *context,const char *name);
  cf_list_head_t *get_value(const char *context,const char *name);
}

%{
name_value_t *configuration_t_get_first_value(configuration_t *self,const u_char *context,const u_char *name) {
  return cfg_get_first_value(self,context,name);
}

cf_list_head_t *configuration_t_get_value(configuration_t *self,const u_char *context,const u_char *name) {
  return cfg_get_value(self,context,name);
}

array_t *configuration_t_get_conf_file(const u_char **which,size_t llen) {
  return get_conf_file(which,llen);
}
%}
/* }}} */

/* {{{ misc declarations */
array_t *get_conf_file(const u_char **which,size_t llen);
typedef struct s_array {
  const int elements;
} array_t;

%extend array_t {
  char *element_at(size_t i);
}
%{
u_char *array_t_element_at(array_t *array,size_t i) {
  return *((u_char **)array_element_at((array_t *)array,i));
}
%}

typedef struct s_name_value {
  %immutable;
  char *name;
  size_t valnum;
} name_value_t;

%extend name_value_t {
  char *get_val(size_t i);
}

%{
u_char *name_value_t_get_val(name_value_t *self,size_t i) {
  if(i < self->valnum) return self->values[i];
  return NULL;
}
%}

/* }}} */

/* {{{ variables */
%immutable;
extern configuration_t fo_default_conf;
extern configuration_t fo_server_conf;
extern configuration_t fo_view_conf;
extern configuration_t fo_arcview_conf;
extern configuration_t fo_post_conf;
extern configuration_t fo_vote_conf;

extern conf_opt_t default_options[];
extern conf_opt_t fo_view_options[];
extern conf_opt_t fo_post_options[];
extern conf_opt_t fo_server_options[];
extern conf_opt_t fo_arcview_options[];
extern conf_opt_t fo_vote_options[];
/* }}} */

// eof
