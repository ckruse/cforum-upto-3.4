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
#include "readline.h"
#include "template.h"
#include "clientlib.h"

typedef struct s_cfxs_conn {
  int sock;
  rline_t tsd;
} t_cfxs_conn;


cforum_plg_error(const char *err) {
  SV *errmsg = sv_2mortal(newSVpvf("%s",err));
  SV *errsv  = get_sv("@", TRUE);
  sv_setsv(errsv,errmsg);
}



MODULE = CForum::Plugins		PACKAGE = CForum::Plugins		

INCLUDE: const-xs.inc
PROTOTYPES: DISABLE

t_array *
get_plugin_group(type)
    int type
  PREINIT:
    const u_char *CLASS = "CForum::Plugins::PluginGroup";
  CODE:
    if(type >= MOD_MAX || type <= 0) {
      cforum_plg_error("Not a valid type");
      XSRETURN_UNDEF;
    }

    if(Modules[type].elements == 0) {
      cforum_plg_error("No plugins available for this type");
      XSRETURN_UNDEF;
    }

    RETVAL=&Modules[type];
  OUTPUT:
    RETVAL


MODULE = CForum::Plugins  PACKAGE = CForum::Plugins::PluginGroup

t_array *
new(class,type)
    const u_char *class
    int type
  PREINIT:
    const u_char *CLASS = "CForum::Plugins::PluginGroup";
  CODE:
    if(type >= MOD_MAX || type <= 0) {
      cforum_plg_error("not a valid type");
      XSRETURN_UNDEF;
    }

    if(Modules[type].elements == 0) {
      cforum_plg_error("No plugins available for this type");
      XSRETURN_UNDEF;
    }

    RETVAL=&Modules[type];
  OUTPUT:
    RETVAL

I32
length(class)
    t_array *class
  CODE:
    RETVAL=class->elements;
  OUTPUT:
    RETVAL

t_handler_config *
get_plugin(class,pos)
    t_array *class
    I32 pos
  PREINIT:
    u_char *CLASS;
    t_handler_config *cfg;
  CODE:
    if((cfg = array_element_at(class,pos)) == NULL) {
      XSRETURN_UNDEF;
    }

    switch(cfg->handler) {
      case INIT_HANDLER:
        CLASS = "CForum::Plugins::Plugin::InitHandler";
        break;
      case VIEW_HANDLER:
        CLASS = "CForum::Plugins::Plugin::ViewHandler";
        break;
      case VIEW_INIT_HANDLER:
        CLASS = "CForum::Plugins::Plugin::ViewInitHandler";
        break;
      case VIEW_LIST_HANDLER:
        CLASS = "CForum::Plugins::Plugin::ViewListHandler";
        break;
      case POSTING_HANDLER:
        CLASS = "CForum::Plugins::Plugin::PostingHandler";
        break;
      case CONNECT_INIT_HANDLER:
        CLASS = "CForum::Plugins::Plugin::ConnectInitHandler";
        break;
      case AUTH_HANDLER:
        CLASS = "CForum::Plugins::Plugin::AuthHandler";
        break;
      case ARCHIVE_HANDLER:
        CLASS = "CForum::Plugins::Plugin::ArchiveHandler";
        break;
    }

    RETVAL=cfg;
  OUTPUT:
    RETVAL

MODULE = CForum::Plugins  PACKAGE = CForum::Plugins::Plugin

MODULE = CForum::Plugins  PACKAGE = CForum::Plugins::Plugin::InitHandler
int
run(class)
    t_handler_config *class
  PREINIT:
    t_filter_begin fkt;
  CODE:
    if((fkt = (t_filter_begin)class->func) == NULL) {
      XSRETURN_UNDEF;
    }
    
    RETVAL=fkt(NULL,&fo_default_conf,&fo_view_conf);
  OUTPUT:
    RETVAL

MODULE = CForum::Plugins  PACKAGE = CForum::Plugins::Plugin::ViewHandler
int
run(class,thread,mode)
    t_handler_config *class
    t_cl_thread *thread
    int mode
  PREINIT:
    t_filter_list fkt;
  CODE:
    if((fkt = (t_filter_list)class->func) == NULL) {
      XSRETURN_UNDEF;
    }

    RETVAL=fkt(NULL,&fo_default_conf,&fo_view_conf,thread,mode);
  OUTPUT:
    RETVAL

MODULE = CForum::Plugins  PACKAGE = CForum::Plugins::Plugin::ViewInitHandler
int
run(class,begin,end)
    t_handler_config *class
    t_cf_template *begin
    t_cf_template *end
  PREINIT:
    t_filter_init_view fkt;
  CODE:
    if((fkt = (t_filter_init_view)class->func) == NULL) {
      XSRETURN_UNDEF;
    }

    RETVAL=fkt(NULL,&fo_default_conf,&fo_view_conf,begin,end);
  OUTPUT:
    RETVAL

MODULE = CForum::Plugins  PACKAGE = CForum::Plugins::Plugin::ViewListHandler
int
run(class,post,ctid,mode)
    t_handler_config *class
    t_message *post
    const u_char *ctid
    int mode
  PREINIT:
    t_filter_list_posting fkt;
    u_int64_t tid;
  CODE:
    tid = strtoull(ctid,NULL,10);

    if((fkt = (t_filter_list_posting)class->func) == NULL) {
      XSRETURN_UNDEF;
    }

    RETVAL=fkt(NULL,&fo_default_conf,&fo_view_conf,post,tid,mode);
  OUTPUT:
    RETVAL

MODULE = CForum::Plugins  PACKAGE = CForum::Plugins::Plugin::PostingHandler
int
run(class,thread,template)
    t_handler_config *class
    t_cl_thread *thread
    t_cf_template *template
  PREINIT:
    t_filter_posting fkt;
  CODE:
    if((fkt = (t_filter_posting)class->func) == NULL) {
      XSRETURN_UNDEF;
    }

    RETVAL=fkt(NULL,&fo_default_conf,&fo_view_conf,thread,template);
  OUTPUT:
    RETVAL

MODULE = CForum::Plugins  PACKAGE = CForum::Plugins::Plugin::ConnectInitHandler
int
run(class,sock)
    t_handler_config *class
    t_cfxs_conn *sock
  PREINIT:
    t_filter_connect fkt;
  CODE:
    if((fkt = (t_filter_connect)class->func) == NULL) {
      XSRETURN_UNDEF;
    }
    
    RETVAL=fkt(NULL,&fo_default_conf,&fo_view_conf,sock->sock);
  OUTPUT:
    RETVAL

MODULE = CForum::Plugins  PACKAGE = CForum::Plugins::Plugin::AuthHandler
int
run(class)
    t_handler_config *class
  PREINIT:
    t_filter_begin fkt;
  CODE:
    if((fkt = (t_filter_begin)class->func) == NULL) {
      XSRETURN_UNDEF;
    }

    RETVAL=fkt(NULL,&fo_default_conf,&fo_view_conf);
  OUTPUT:
    RETVAL

MODULE = CForum::Plugins  PACKAGE = CForum::Plugins::Plugin::ArchiveHandler
void
run(class)
    t_handler_config *class
  CODE:
    croak("Sorry, cannot run archive handlers due to they're server plugins");

# eof
