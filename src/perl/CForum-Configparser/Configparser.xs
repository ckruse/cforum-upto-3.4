#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "ppport.h"

#include "const-c.inc"

#include "config.h"
#include "defines.h"

#include "hashlib.h"
#include "utils.h"
#include "configparser.h"

cforum_cfgparser_error(const char *err) {
  SV *errmsg = newSVpvf("%s",err);
  SV *errsv  = get_sv("@", TRUE);
  sv_setsv(errsv,errmsg);
}

MODULE = CForum::Configparser		PACKAGE = CForum::Configparser		

INCLUDE: const-xs.inc
PROTOTYPES: DISABLE


SV *
new(class)
    u_char *class
  PREINIT:
    char *CLASS = "CForum::Configparser";
    SV *obj;
  CODE:
    obj = newSV(0);
    init_modules();
    cfg_init();
    RETVAL=sv_setref_iv(obj,CLASS,0);
  OUTPUT:
    RETVAL

bool
read(class,what)
    SV *class
    SV *what
  PREINIT:
    AV *which;
    SV *elem;
    u_char **wanted = NULL;
    u_char *file;
    size_t wlen = 0;
    STRLEN len;
    I32 i,alen;
    char *CLASS = "CForum::Configparser::Configuration";
    t_array *array;
    t_configfile cfg;

  CODE:
    if(!SvROK(what)) {
      if(SvTYPE(what) == SVt_PV) {
        wanted = fo_alloc(NULL,1,sizeof(u_char **),FO_ALLOC_MALLOC);
        wanted[0] = SvPV(what,len);
        wlen = 1;
      }
      else {
        cforum_cfgparser_error("Wrong type for read()");
        XSRETURN_UNDEF;
      }
    }
    else {
      if(SvTYPE(SvRV(what)) == SVt_PVAV) {
        which = (AV*)SvRV(what);
        if((alen = av_len(which)) == -1) {
          cforum_cfgparser_error("wanted array is empty");
          XSRETURN_UNDEF;
        }

        for(i=0;i<=alen;i++) {
          elem = *av_fetch(which,i,0);
          if(SvTYPE(elem) != SVt_PV) continue;
          wanted = fo_alloc(wanted,++wlen,sizeof(char **),FO_ALLOC_REALLOC);
          wanted[wlen-1] = SvPV(elem,len);
        }
      }
      else {
        cforum_cfgparser_error("Wrong type for read()");
        XSRETURN_UNDEF;
      }
    }

    if(wlen == 0) {
      cforum_cfgparser_error("got nothing to read...\n");
      XSRETURN_UNDEF;
    }

    if((array = get_conf_file((const u_char **)wanted,wlen)) == NULL) {
      cforum_cfgparser_error("one of the wanted files is not present\n");
      XSRETURN_UNDEF;
    }

    for(i=0;i<wlen;i++) {
      file = *((u_char **)array_element_at(array,i));
      cfg_init_file(&cfg,file);

      if(cf_strcmp(wanted[i],"fo_default") == 0) {
        cfg_register_options(&cfg,default_options);

        if(read_config(&cfg,NULL,CFG_MODE_CONFIG) != 0) {
          cforum_cfgparser_error("Error parsing fo_default.conf");
          XSRETURN_UNDEF;
        }

        elem = get_sv("CForum::Configparser::fo_default_conf",TRUE|GV_ADDMULTI);
        sv_setref_pv(elem,CLASS,&fo_default_conf);
      }
      else if(cf_strcmp(wanted[i],"fo_view") == 0) {
        cfg_register_options(&cfg,fo_view_options);

        if(read_config(&cfg,NULL,CFG_MODE_CONFIG) != 0) {
          cforum_cfgparser_error("Error parsing fo_view.conf");
          XSRETURN_UNDEF;
        }

        elem = get_sv("CForum::Configparser::fo_view_conf",TRUE|GV_ADDMULTI);
        sv_setref_pv(elem,CLASS,&fo_view_conf);
      }
      else if(cf_strcmp(wanted[i],"fo_post") == 0) {
        cfg_register_options(&cfg,fo_post_options);

        if(read_config(&cfg,NULL,CFG_OPT_CONFIG) != 0) {
          cforum_cfgparser_error("Error parsing fo_post.conf");
          XSRETURN_UNDEF;
        }

        elem = get_sv("CForum::Configparser::fo_post_conf",TRUE|GV_ADDMULTI);
        sv_setref_pv(elem,CLASS,&fo_post_conf);
      }
      else if(cf_strcmp(wanted[i],"fo_server") == 0) {
        cfg_register_options(&cfg,fo_server_options);

        if(read_config(&cfg,NULL,CFG_OPT_CONFIG) != 0) {
          cforum_cfgparser_error("Error parsing fo_server.conf");
          XSRETURN_UNDEF;
        }

        elem = get_sv("CForum::Configparser::fo_server_conf",TRUE|GV_ADDMULTI);
        sv_setref_pv(elem,CLASS,&fo_server_conf);
      }

      cfg_cleanup_file(&cfg);
      free(file);
    }

    free(wanted);
    array_destroy(array);

    XSRETURN_YES;

void
destroy(class)
    SV *class
  ALIAS:
    CForum::Configparser::DESTROY = 1
  CODE:

MODULE = CForum::Configparser    PACKAGE = CForum::Configparser::Configuration

SV *
get_entry(class,name)
    t_configuration *class
    const u_char *name
  PREINIT:
    SV *var;
    t_cf_list_head *v;
    const char *CLASS = "CForum::Configparser::Configuration::Option";
  CODE:
    if((v = cfg_get_value(class,name)) == NULL) {
      cforum_cfgparser_error("configuration option not present\n");
      XSRETURN_UNDEF;
    }

    var = newSV(0);
    RETVAL=sv_setref_pv(var,CLASS,v->elements);
  OUTPUT:
    RETVAL

void
destroy(class)
    t_configuration *class
  ALIAS:
    CForum::Configparser::Configuration::DESTROY = 1
  CODE:
    cfg_cleanup(class);

MODULE = CForum::Configparser    PACKAGE = CForum::Configparser::Configuration::Option

const u_char *
get_value(class,idx)
    const t_cf_list_element *class;
    I32 idx;
  PREINIT:
    t_name_value *val;
  CODE:
    if(idx > 2 || idx < 0) {
      cforum_cfgparser_error("index out of bound\n");
      XSRETURN_UNDEF;
    }

    val = (t_name_value *)class->data;

    if(val->values[idx] == NULL) {
      XSRETURN_UNDEF;
    }

    RETVAL=val->values[idx];
  OUTPUT:
    RETVAL


const u_char *
get_name(class)
    const t_cf_list_element *class;
  PREINIT:
    t_name_value *val;
  CODE:
    val = (t_name_value *)class->data;
    RETVAL=val->name;
  OUTPUT:
    RETVAL

SV *
next(class)
    const t_cf_list_element *class
  PREINIT:
    SV *var;
    char *CLASS = "CForum::Configparser::Configuration::Option";
  CODE:
    if(class->next == NULL) {
      XSRETURN_UNDEF;
    }
    
    var = newSV(0);
    RETVAL=sv_setref_pv(var,CLASS,class->next);
  OUTPUT:
    RETVAL

# eof
