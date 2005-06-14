/**
 * \file flt_basic.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
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
#include "config.h"
#include "defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/types.h>

#include "readline.h"
#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "cfcgi.h"
#include "template.h"
#include "clientlib.h"
/* }}} */

struct {
  u_char *FontColor;
  u_char *FontSize;
  u_char *FontFamily;
  long  AutoReload;
  u_char *BaseTarget;
  u_char *QuoteColorF;
  u_char *QuoteColorB;
} Cfg = { NULL, NULL, NULL, 0, NULL, NULL, NULL };

static u_char *flt_basic_fn = NULL;

/* {{{ flt_basic_execute */
int flt_basic_execute(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_cf_template *begin,t_cf_template *end) {
  u_char *forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  t_name_value *ubase = cfg_get_first_value(dc,forum_name,"UBaseURL");
  t_name_value *cs = cfg_get_first_value(dc,forum_name,"ExternCharset");
  u_char *UserName = cf_hash_get(GlobalValues,"UserName",8);
  t_name_value *dflt = cfg_get_first_value(vc,forum_name,"DateFormatLoadTime");
  t_name_value *loc = cfg_get_first_value(dc,forum_name,"DateLocale");
  t_name_value *ucfg;

  u_char buff[20];
  time_t tm  = time(NULL);
  int   len  = 0;
  size_t n;
  u_char *time;

  cf_set_variable(begin,cs,"ubase",ubase->values[0],strlen(ubase->values[0]),1);
  cf_set_variable(end,cs,"ubase",ubase->values[0],strlen(ubase->values[0]),1);

  if(UserName) {
    ucfg = cfg_get_first_value(dc,forum_name,"UserConfig");

    cf_tpl_setvalue(begin,"authed",TPL_VARIABLE_STRING,"1",1);
    cf_tpl_setvalue(end,"authed",TPL_VARIABLE_STRING,"1",1);
    cf_set_variable(begin,cs,"userconfig",ucfg->values[0],strlen(ucfg->values[0]),1);
    cf_set_variable(end,cs,"userconfig",ucfg->values[0],strlen(ucfg->values[0]),1);

    if((Cfg.FontColor && *Cfg.FontColor) || (Cfg.FontSize && *Cfg.FontSize) || (Cfg.FontFamily && *Cfg.FontFamily)) {
      cf_tpl_setvalue(begin,"font",TPL_VARIABLE_STRING,"1",1);

      if(Cfg.FontColor && *Cfg.FontColor) {
        cf_set_variable(begin,cs,"fontcolor",Cfg.FontColor,strlen(Cfg.FontColor),1);
      }
      if(Cfg.FontSize && *Cfg.FontSize) {
        cf_set_variable(begin,cs,"fontsize",Cfg.FontSize,strlen(Cfg.FontSize),1);
      }
      if(Cfg.FontFamily && *Cfg.FontFamily) {
        cf_set_variable(begin,cs,"fontfamily",Cfg.FontFamily,strlen(Cfg.FontFamily),1);
      }
    }

    if(Cfg.AutoReload) {
      n    = sprintf(buff,"%ld",Cfg.AutoReload);
      tm  += (time_t)Cfg.AutoReload;
      time = cf_general_get_time(dflt->values[0],loc->values[0],&len,&tm);

      cf_set_variable(begin,cs,"autoreload",buff,n,1);
      cf_set_variable(begin,cs,"autoreloadtime",time,len,1);

      free(time);
    }

    return FLT_OK;
  }
  else {
    ucfg = cfg_get_first_value(dc,forum_name,"UserRegister");
    cf_set_variable(begin,cs,"userconfig",ucfg->values[0],strlen(ucfg->values[0]),1);
    cf_set_variable(end,cs,"userconfig",ucfg->values[0],strlen(ucfg->values[0]),1);
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_basic_handle_posting */
int flt_basic_handle_posting(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_cl_thread *thr,t_cf_template *tpl) {
  u_char *forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  t_name_value *cs = cfg_get_first_value(dc,forum_name,"ExternCharset");
  u_char *UserName = cf_hash_get(GlobalValues,"UserName",8);

  if(UserName) {
    if((Cfg.FontColor && *Cfg.FontColor) || (Cfg.FontSize && *Cfg.FontSize) || (Cfg.FontFamily && *Cfg.FontFamily) || (Cfg.QuoteColorF && *Cfg.QuoteColorF) || (Cfg.QuoteColorB && *Cfg.QuoteColorB)) {
      cf_tpl_setvalue(tpl,"font",TPL_VARIABLE_STRING,"1",1);

      if(Cfg.FontColor && *Cfg.FontColor)     cf_set_variable(tpl,cs,"fontcolor",Cfg.FontColor,strlen(Cfg.FontColor),1);
      if(Cfg.FontSize && *Cfg.FontSize)       cf_set_variable(tpl,cs,"fontsize",Cfg.FontSize,strlen(Cfg.FontSize),1);
      if(Cfg.FontFamily && *Cfg.FontFamily)   cf_set_variable(tpl,cs,"fontfamily",Cfg.FontFamily,strlen(Cfg.FontFamily),1);
      if(Cfg.QuoteColorF && *Cfg.QuoteColorF) cf_set_variable(tpl,cs,"qcolor",Cfg.QuoteColorF,strlen(Cfg.QuoteColorF),1);
      if(Cfg.QuoteColorB && *Cfg.QuoteColorB) cf_set_variable(tpl,cs,"qcolorback",Cfg.QuoteColorB,strlen(Cfg.QuoteColorB),1);
    }

    return FLT_OK;
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_basic_set_target */
int flt_basic_set_target(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_message *msg,u_int64_t tid,int mode) {
  u_char *forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  t_name_value *cs = cfg_get_first_value(dc,forum_name,"ExternCharset");

  if(Cfg.BaseTarget && *Cfg.BaseTarget && mode == 0) {
    cf_set_variable_hash(&msg->hashvar,cs,"target",Cfg.BaseTarget,strlen(Cfg.BaseTarget),1);
    return FLT_OK;
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_basic_handle_command */
int flt_basic_handle_command(t_configfile *cfile,t_conf_opt *opt,const u_char *context,u_char **args,size_t argnum) {
  if(flt_basic_fn == NULL) flt_basic_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(flt_basic_fn,context) != 0) return 0;

  if(cf_strcmp(opt->name,"FontColor") == 0) {
    if(Cfg.FontColor) free(Cfg.FontColor);
    Cfg.FontColor = strdup(args[0]);
  }
  else if(cf_strcmp(opt->name,"FontSize") == 0) {
    if(Cfg.FontSize) free(Cfg.FontSize);
    Cfg.FontSize  = strdup(args[0]);
  }
  else if(cf_strcmp(opt->name,"FontFamily") == 0) {
    if(Cfg.FontFamily) free(Cfg.FontFamily);
    Cfg.FontFamily = strdup(args[0]);
  }
  else if(cf_strcmp(opt->name,"AutoReload") == 0) {
    Cfg.AutoReload = strtol(args[0],NULL,10) * 60L;
  }
  else if(cf_strcmp(opt->name,"BaseTarget") == 0) {
    if(Cfg.BaseTarget) free(Cfg.BaseTarget);
    Cfg.BaseTarget = strdup(args[0]);
  }
  else if(cf_strcmp(opt->name,"QuoteColor") == 0) {
    if(Cfg.QuoteColorF) free(Cfg.QuoteColorF);
    if(Cfg.QuoteColorB) free(Cfg.QuoteColorB);
    Cfg.QuoteColorF = strdup(args[0]);
    Cfg.QuoteColorB = strdup(args[1]);
  }

  return 0;
}
/* }}} */

/* {{{ flt_basic_cleanup */
void flt_basic_cleanup(void) {
  if(Cfg.FontColor)   free(Cfg.FontColor);
  if(Cfg.FontSize)    free(Cfg.FontSize);
  if(Cfg.FontFamily)  free(Cfg.FontFamily);
  if(Cfg.BaseTarget)  free(Cfg.BaseTarget);
  if(Cfg.QuoteColorF) free(Cfg.QuoteColorF);
  if(Cfg.QuoteColorB) free(Cfg.QuoteColorB);
}
/* }}} */

t_conf_opt flt_basic_config[] = {
  { "FontColor",  flt_basic_handle_command, CFG_OPT_USER|CFG_OPT_CONFIG|CFG_OPT_LOCAL, NULL },
  { "FontSize",   flt_basic_handle_command, CFG_OPT_USER|CFG_OPT_CONFIG|CFG_OPT_LOCAL, NULL },
  { "FontFamily", flt_basic_handle_command, CFG_OPT_USER|CFG_OPT_CONFIG|CFG_OPT_LOCAL, NULL },
  { "AutoReload", flt_basic_handle_command, CFG_OPT_USER|CFG_OPT_CONFIG|CFG_OPT_LOCAL, NULL },
  { "BaseTarget", flt_basic_handle_command, CFG_OPT_USER|CFG_OPT_CONFIG|CFG_OPT_LOCAL, NULL },
  { "QuoteColor", flt_basic_handle_command, CFG_OPT_USER|CFG_OPT_CONFIG|CFG_OPT_LOCAL, NULL },
  { NULL, NULL, 0, NULL }
};

t_handler_config flt_basic_handlers[] = {
  { VIEW_INIT_HANDLER, flt_basic_execute        },
  { POSTING_HANDLER,   flt_basic_handle_posting },
  { VIEW_LIST_HANDLER, flt_basic_set_target     },
  { 0, NULL }
};

t_module_config flt_basic = {
  MODULE_MAGIC_COOKIE,
  flt_basic_config,
  flt_basic_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  flt_basic_cleanup
};


/* eof */
