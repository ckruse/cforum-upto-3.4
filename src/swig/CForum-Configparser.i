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
%}

%init %{
  cfg_init();
%}

typedef unsigned char u_char;

/* {{{ typemaps */
%typemap(in) u_char ** {
  AV *tempav;
  I32 len;
  int i;
  SV  **tv;
  if(!SvROK($input)) croak("Argument $argnum is not a reference.");
  if(SvTYPE(SvRV($input)) != SVt_PVAV) croak("Argument $argnum is not an array.");

  tempav = (AV*)SvRV($input);
  len = av_len(tempav);
  $1 = (u_char **)malloc((len+2)*sizeof(u_char *));
  for(i=0;i<=len;++i) {
    tv = av_fetch(tempav, i, 0);
    $1[i] = (char *) SvPV(*tv,PL_na);
  }

  $1[i] = NULL;
};

// This cleans up the char ** array after the function call
%typemap(freearg) u_char ** {
  free($1);
}

// Creates a new Perl array and places a NULL-terminated char ** into it
%typemap(out) u_char ** {
  AV *myav;
  SV **svs;
  int i = 0,len = 0;
  /* Figure out how many elements we have */
  while($1[len]) len++;
  svs = (SV **)malloc(len*sizeof(SV *));
  for (i = 0; i < len ; i++) {
    svs[i] = sv_newmortal();
    sv_setpv((SV*)svs[i],$1[i]);
  };
  myav = av_make(len,svs);
  free(svs);
  $result = newRV((SV*)myav);
  sv_2mortal($result);
  argvi++;
}
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


/* {{{ t_configfile declarations */
typedef struct s_configfile {
  char *filename;
} t_configfile;
%extend t_configfile {
  t_configfile(char *filename);
  int register_options(t_conf_opt *opts);
  int read(int mode);
}

%{
t_configfile *new_t_configfile(u_char *filename) {
  t_configfile *cfg = malloc(sizeof(*cfg));
  cfg_init_file(cfg,filename);
  return cfg;
}

int t_configfile_register_options(t_configfile *self,t_conf_opt *opts) {
  return cfg_register_options(self,opts);
}

int t_configfile_read(t_configfile *self,int mode) {
  read_config(self,NULL,mode);
}
%}
/* }}} */

/* {{{ t_configuration declarations */
typedef struct s_configuration {} t_configuration;
%extend t_configuration {
  t_name_value *get_first_value(const char *context,const char *name);
  t_cf_list_head *get_value(const char *context,const char *name);
}

%{
t_name_value *t_configuration_get_first_value(t_configuration *self,const u_char *context,const u_char *name) {
  return cfg_get_first_value(self,context,name);
}

t_cf_list_head *t_configuration_get_value(t_configuration *self,const u_char *context,const u_char *name) {
  return cfg_get_value(self,context,name);
}

t_array *t_configuration_get_conf_file(const u_char **which,size_t llen) {
  return get_conf_file(which,llen);
}
%}
/* }}} */

/* {{{ misc declarations */
t_array *get_conf_file(const u_char **which,size_t llen);
typedef struct s_array {
  const int elements;
} t_array;

%extend t_array {
  char *element_at(size_t i);
}
%{
u_char *t_array_element_at(t_array *array,size_t i) {
  return *((u_char **)array_element_at((t_array *)array,i));
}
%}

typedef struct s_name_value {
  %immutable;
  char *name;
  size_t valnum;
} t_name_value;

%extend t_name_value {
  char *get_val(size_t i);
}

%{
u_char *t_name_value_get_val(t_name_value *self,size_t i) {
  if(i < self->valnum) return self->values[i];
  return NULL;
}
%}

/* }}} */

/* {{{ variables */
%immutable;
extern t_configuration fo_default_conf;
extern t_configuration fo_server_conf;
extern t_configuration fo_view_conf;
extern t_configuration fo_arcview_conf;
extern t_configuration fo_post_conf;
extern t_configuration fo_vote_conf;

extern t_conf_opt default_options[];
extern t_conf_opt fo_view_options[];
extern t_conf_opt fo_post_options[];
extern t_conf_opt fo_server_options[];
extern t_conf_opt fo_arcview_options[];
extern t_conf_opt fo_vote_options[];
/* }}} */

// eof
