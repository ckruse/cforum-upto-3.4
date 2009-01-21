/**
 * \file flt_basic.c
 * \author Christian Kruse, <cjk@wwwtech.de>
 *
 * This plugin implements basic user features
 */

/* {{{ Initial comments */
/*
 * $LastChangedDate$
 * $LastChangedRevision$
 * $LastChangedBy$
 *
 */
/* }}} */

/* {{{ Includes */
#include "cfconfig.h"
#include "defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/types.h>
#include <inttypes.h>

#include "readline.h"
#include "hashlib.h"
#include "utils.h"
#include "cfgcomp.h"
#include "cfcgi.h"
#include "template.h"
#include "clientlib.h"
/* }}} */

/* {{{ flt_basic_execute */
int flt_basic_execute(cf_hash_t *head,cf_configuration_t *cfg,cf_template_t *begin,cf_template_t *end) {
  cf_cfg_config_value_t *base = cf_cfg_get_value(cfg,"DF:BaseURL"),
  *cs = cf_cfg_get_value(cfg,"DF:ExternCharset"),
    *dflt = cf_cfg_get_value(cfg,"FV:DateFormatLoadTime"),
    *loc = cf_cfg_get_value(cfg,"DF:DateLocale"),
    *cats = cfg_get_value(cfg,"DF:Categories"),
    *ucfg = cf_cfg_get_value(cfg,"UserManage"),
    *tmp;
  cf_tpl_variable_t array;

  u_char buff[20],*UserName = cf_hash_get(GlobalValues,"UserName",8);
  time_t tm = time(NULL);
  size_t len = 0,n;
  int set = 0;
  u_char *time;

  cf_set_variable(begin,cs->sval,"ubase",base->avals[1].sval,strlen(ubase->avals[1].sval),1); // TODO: user-base-url
  cf_set_variable(end,cs->sval,"ubase",base->avals[1].sval,strlen(ubase->avals[1].sval),1); // TODO: user-base-url

  /* {{{ set categories */
  cf_tpl_var_init(&array,TPL_VARIABLE_ARRAY);

  for(i=0;i<cats->alen;++i) {
    tmp   = charset_convert_entities(cats->avals[i].sval,strlen(cats->avals[i].sval),"UTF-8",cs->sval,&len);
    cf_tpl_var_addvalue(&array,TPL_VARIABLE_STRING,tmp,len);
    free(tmp);
  }
  cf_tpl_setvar(begin,"cats",&array);
  /* }}} */

  if(UserName) {
    cf_tpl_setvalue(begin,"authed",TPL_VARIABLE_STRING,"1",1); // TODO: user-authed
    cf_tpl_setvalue(end,"authed",TPL_VARIABLE_STRING,"1",1); // TODO: user-authed
    cf_set_variable(begin,cs->sval,"userconfig",ucfg->avals[0].sval,strlen(ucfg->avals[0].sval),1); // TODO: user-config (or something like that)
    cf_set_variable(end,cs->sval,"userconfig",ucfg->avals[0].sval,strlen(ucfg->avals[0].sval),1); // TODO: user-config (see above)

    if((tmp = cf_cfg_get_value(cfg,"Basic:FontColor")) != NULL) {
      cf_tpl_setvalue(begin,"font",TPL_VARIABLE_STRING,"1",1); // TODO: basic-font
      set = 1;
      cf_set_variable(begin,cs->sval,"fontcolor",tmp->sval,strlen(tmp->sval),1); // TODO: basic-font-color
    }

    if((tmp = cf_cfg_get_value(cfg,"Basic:FontSize")) != NULL) {
      if(set == 0) {
        cf_tpl_setvalue(begin,"font",TPL_VARIABLE_STRING,"1",1); // TODO: basic-font
        set = 1;
      }
      cf_set_variable(begin,cs->sval,"fontsize",tmp->sval,strlen(tmp->sval),1); // TODO: basic-font-size
    }

    if((tmp = cf_cfg_get_value(cfg,"Basic:FontFamily")) != NULL) {
      if(set == 0) {
        cf_tpl_setvalue(begin,"font",TPL_VARIABLE_STRING,"1",1); // TODO: basic-font
        set = 1;
      }

      cf_set_variable(begin,cs->sval,"fontfamily",tmp->sval,strlen(tmp->sval),1); // TODO: basic-font-family
    }

    if((tmp = cf_cfg_get_value(cfg,"Basic:AutoReload")) != NULL) {
      n    = sprintf(buff,"%ld",tmp->ival);
      tm  += (time_t)tmp->ival;
      time = cf_general_get_time(dflt->sval,loc->sval,&len,&tm);

      cf_set_variable(begin,cs->sval,"autoreload",buff,n,1); // TODO: basic-auto-reload
      cf_set_variable(begin,cs->sval,"autoreloadtime",time,len,1); // TODO: basic-auto-reload-time

      free(time);
    }

    return FLT_OK;
  }
  else {
    cf_set_variable(begin,cs->sval,"userconfig",ucfg->avals[1].sval,strlen(ucfg->avals[1].sval),1); // TODO: user-register
    cf_set_variable(end,cs->sval,"userconfig",ucfg->avals[1].sval,strlen(ucfg->avals[1].sval),1); // TODO: user-register
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_basic_handle_posting */
int flt_basic_handle_posting(cf_hash_t *head,cf_configuration_t *cfg,cf_cl_thread_t *thr,cf_template_t *tpl) {
  cf_cfg_config_value_t *cs = cf_cfg_get_first_value(dc,forum_name,"DF:ExternCharset"),*tmp;
  u_char *UserName = cf_hash_get(GlobalValues,"UserName",8);
  int set = 0;

  if(UserName) {
    if((tmp = cf_cfg_get_value(cfg,"Basic:FontColor")) != NULL) {
      cf_tpl_setvalue(begin,"font",TPL_VARIABLE_STRING,"1",1); // TODO: basic-font
      set = 1;
      cf_set_variable(begin,cs->sval,"fontcolor",tmp->sval,strlen(tmp->sval),1); // TODO: basic-font-color
    }

    if((tmp = cf_cfg_get_value(cfg,"Basic:FontSize")) != NULL) {
      if(set == 0) {
        cf_tpl_setvalue(begin,"font",TPL_VARIABLE_STRING,"1",1); // TODO: basic-font
        set = 1;
      }
      cf_set_variable(begin,cs->sval,"fontsize",tmp->sval,strlen(tmp->sval),1); // TODO: basic-font-size
    }

    if((tmp = cf_cfg_get_value(cfg,"Basic:FontFamily")) != NULL) {
      if(set == 0) {
        cf_tpl_setvalue(begin,"font",TPL_VARIABLE_STRING,"1",1); // TODO: basic-font
        set = 1;
      }

      cf_set_variable(begin,cs->sval,"fontfamily",tmp->sval,strlen(tmp->sval),1); // TODO: basic-font-family
    }

    if((tmp = cf_cfg_get_value(cfg,"Basic:FontFamily")) != NULL) {
      if(set == 0) {
        cf_tpl_setvalue(begin,"font",TPL_VARIABLE_STRING,"1",1); // TODO: basic-font
        set = 1;
      }

      cf_set_variable(begin,cs->sval,"qcolor",tmp->avals[0].sval,strlen(tmp->avals[0].sval),1); // TODO: basic-quote-color
      cf_set_variable(begin,cs->sval,"qcolorback",tmp->avals[1].sval,strlen(tmp->avals[1].sval),1); // TODO: basic-quote-background-color
    }

    return set ? FLT_OK : FLT_DECLINE;
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_basic_set_target */
int flt_basic_set_target(cf_hash_t *head,cf_configuration_t *cfg,cf_message_t *msg,u_int64_t tid,int mode) {
  cf_name_value_t *cs = cf_cfg_get_first_value(dc,forum_name,"DF:ExternCharset"),*target;

  if((target = cf_cfg_get_value(cfg,"Basic:BaseTarget")) != NULL && mode == 0) {
    cf_set_variable_hash(&msg->hashvar,cs->sval,"target",target->sval,strlen(target->sval),1); // TODO: basic-base-target
    return FLT_OK;
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_basic_handle_command */
int flt_basic_handle_command(cf_configfile_t *cfile,cf_conf_opt_t *opt,const u_char *context,u_char **args,size_t argnum) {
  if(flt_basic_fn == NULL) flt_basic_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(flt_basic_fn,context) != 0) return 0;

  if(cf_strcmp(opt->name,"Basic:FontColor") == 0) {
    if(Cfg.FontColor) free(Cfg.FontColor);
    Cfg.FontColor = strdup(args[0]);
  }
  else if(cf_strcmp(opt->name,"Basic:FontSize") == 0) {
    if(Cfg.FontSize) free(Cfg.FontSize);
    Cfg.FontSize  = strdup(args[0]);
  }
  else if(cf_strcmp(opt->name,"Basic:FontFamily") == 0) {
    if(Cfg.FontFamily) free(Cfg.FontFamily);
    Cfg.FontFamily = strdup(args[0]);
  }
  else if(cf_strcmp(opt->name,"Basic:AutoReload") == 0) {
    Cfg.AutoReload = strtol(args[0],NULL,10) * 60L;
  }
  else if(cf_strcmp(opt->name,"Basic:BaseTarget") == 0) {
    if(Cfg.BaseTarget) free(Cfg.BaseTarget);
    Cfg.BaseTarget = strdup(args[0]);
  }
  else if(cf_strcmp(opt->name,"Basic:QuoteColor") == 0) {
    if(Cfg.QuoteColorF) free(Cfg.QuoteColorF);
    if(Cfg.QuoteColorB) free(Cfg.QuoteColorB);
    Cfg.QuoteColorF = strdup(args[0]);
    Cfg.QuoteColorB = strdup(args[1]);
  }

  return 0;
}
/* }}} */

/**
 * config values:
 * Basic:FontColor = "#col";
 * Basic:FontSize = "size";
 * Basic:FontFamily = "family";
 * Basic:AutoReload = <num minutes>;
 * Basic:BaseTarget = "target";
 * Basic:QuoteColor = ("#fore-col","#back-col");
*/

cf_handler_config_t flt_basic_handlers[] = {
  { VIEW_INIT_HANDLER, flt_basic_execute        },
  { POSTING_HANDLER,   flt_basic_handle_posting },
  { VIEW_LIST_HANDLER, flt_basic_set_target     },
  { 0, NULL }
};

cf_module_config_t flt_basic = {
  MODULE_MAGIC_COOKIE,
  flt_basic_config,
  flt_basic_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};


/* eof */
