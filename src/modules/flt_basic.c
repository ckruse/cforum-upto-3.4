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
} flt_basic_cfg = { NULL, NULL, NULL, 0, NULL, NULL, NULL };

int flt_basic_execute_filter(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_cf_template *begin,t_cf_template *end) {
  t_name_value *ubase       = cfg_get_first_value(dc,NULL,"UBaseURL");
  t_name_value *cs          = cfg_get_first_value(dc,NULL,"ExternCharset");
  u_char *UserName          = cf_hash_get(GlobalValues,"UserName",8);

  cf_set_variable(begin,cs,"ubase",ubase->values[0],strlen(ubase->values[0]),1);

  if(UserName) {
    t_name_value *ucfg       = cfg_get_first_value(dc,NULL,"UserConfig");

    tpl_cf_setvar(begin,"authed","1",1,0);
    if(ucfg) {
      cf_set_variable(begin,cs,"userconfig",ucfg->values[0],strlen(ucfg->values[0]),1);
    }

    if((flt_basic_cfg.FontColor && *flt_basic_cfg.FontColor) || (flt_basic_cfg.FontSize && *flt_basic_cfg.FontSize) || (flt_basic_cfg.FontFamily && *flt_basic_cfg.FontFamily)) {
      tpl_cf_setvar(begin,"font","1",1,0);

      if(flt_basic_cfg.FontColor && *flt_basic_cfg.FontColor) {
        cf_set_variable(begin,cs,"fontcolor",flt_basic_cfg.FontColor,strlen(flt_basic_cfg.FontColor),1);
      }
      if(flt_basic_cfg.FontSize && *flt_basic_cfg.FontSize) {
        cf_set_variable(begin,cs,"fontsize",flt_basic_cfg.FontSize,strlen(flt_basic_cfg.FontSize),1);
      }
      if(flt_basic_cfg.FontFamily && *flt_basic_cfg.FontFamily) {
        cf_set_variable(begin,cs,"fontfamily",flt_basic_cfg.FontFamily,strlen(flt_basic_cfg.FontFamily),1);
      }

    }

    if(flt_basic_cfg.AutoReload) {
      u_char buff[20];
      time_t tm  = time(NULL);
      int   len  = 0;
      int   n    = sprintf(buff,"%ld",flt_basic_cfg.AutoReload);
      u_char *time;

      tm += (time_t)flt_basic_cfg.AutoReload;
      time = get_time(vc,"DateFormatLoadTime",&len,&tm);

      cf_set_variable(begin,cs,"autoreload",buff,n,1);
      cf_set_variable(begin,cs,"autoreloadtime",time,len,1);

      free(time);
    }

    return FLT_OK;
  }
  else {
    t_name_value *ucfg = cfg_get_first_value(dc,NULL,"UserRegister");
    if(ucfg) {
      cf_set_variable(begin,cs,"userconfig",ucfg->values[0],strlen(ucfg->values[0]),1);
    }
  }

  return FLT_DECLINE;
}

int flt_basic_handle_posting(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_cl_thread *thr,t_cf_template *tpl) {
  t_name_value *cs = cfg_get_first_value(dc,NULL,"ExternCharset");
  u_char *UserName = cf_hash_get(GlobalValues,"UserName",8);

  if(UserName) {
    if((flt_basic_cfg.FontColor && *flt_basic_cfg.FontColor) || (flt_basic_cfg.FontSize && *flt_basic_cfg.FontSize) || (flt_basic_cfg.FontFamily && *flt_basic_cfg.FontFamily) || (flt_basic_cfg.QuoteColorF && *flt_basic_cfg.QuoteColorF) || (flt_basic_cfg.QuoteColorB && *flt_basic_cfg.QuoteColorB)) {
      tpl_cf_setvar(tpl,"font","1",1,0);

      if(flt_basic_cfg.FontColor && *flt_basic_cfg.FontColor) {
        cf_set_variable(tpl,cs,"fontcolor",flt_basic_cfg.FontColor,strlen(flt_basic_cfg.FontColor),1);
      }
      if(flt_basic_cfg.FontSize && *flt_basic_cfg.FontSize) {
        cf_set_variable(tpl,cs,"fontsize",flt_basic_cfg.FontSize,strlen(flt_basic_cfg.FontSize),1);
      }
      if(flt_basic_cfg.FontFamily && *flt_basic_cfg.FontFamily) {
        cf_set_variable(tpl,cs,"fontfamily",flt_basic_cfg.FontFamily,strlen(flt_basic_cfg.FontFamily),1);
      }
      if(flt_basic_cfg.QuoteColorF && *flt_basic_cfg.QuoteColorF) {
        cf_set_variable(tpl,cs,"qcolor",flt_basic_cfg.QuoteColorF,strlen(flt_basic_cfg.QuoteColorF),1);
      }
      if(flt_basic_cfg.QuoteColorB && *flt_basic_cfg.QuoteColorB) {
        cf_set_variable(tpl,cs,"qcolorback",flt_basic_cfg.QuoteColorB,strlen(flt_basic_cfg.QuoteColorB),1);
      }

    }

    return FLT_OK;
  }

  return FLT_DECLINE;
}

int flt_basic_set_target(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_message *msg,u_int64_t tid,int mode) {
  t_name_value *cs = cfg_get_first_value(dc,NULL,"ExternCharset");

  if(flt_basic_cfg.BaseTarget && *flt_basic_cfg.BaseTarget && mode == 0) {
    cf_set_variable(&msg->tpl,cs,"target",flt_basic_cfg.BaseTarget,strlen(flt_basic_cfg.BaseTarget),1);
    return FLT_OK;
  }

  return FLT_DECLINE;
}

int flt_basic_handle_command(t_configfile *cfile,t_conf_opt *opt,const u_char *context,u_char **args,size_t argnum) {
  if(cf_strcmp(opt->name,"FontColor") == 0) {
    if(flt_basic_cfg.FontColor) free(flt_basic_cfg.FontColor);
    flt_basic_cfg.FontColor = strdup(args[0]);
  }
  else if(cf_strcmp(opt->name,"FontSize") == 0) {
    if(flt_basic_cfg.FontSize) free(flt_basic_cfg.FontSize);
    flt_basic_cfg.FontSize  = strdup(args[0]);
  }
  else if(cf_strcmp(opt->name,"FontFamily") == 0) {
    if(flt_basic_cfg.FontFamily) free(flt_basic_cfg.FontFamily);
    flt_basic_cfg.FontFamily = strdup(args[0]);
  }
  else if(cf_strcmp(opt->name,"AutoReload") == 0) {
    flt_basic_cfg.AutoReload = strtol(args[0],NULL,10) * 60L;
  }
  else if(cf_strcmp(opt->name,"BaseTarget") == 0) {
    if(flt_basic_cfg.BaseTarget) free(flt_basic_cfg.BaseTarget);
    flt_basic_cfg.BaseTarget = strdup(args[0]);
  }
  else if(cf_strcmp(opt->name,"QuoteColor") == 0) {
    if(flt_basic_cfg.QuoteColorF) free(flt_basic_cfg.QuoteColorF);
    if(flt_basic_cfg.QuoteColorB) free(flt_basic_cfg.QuoteColorB);
    flt_basic_cfg.QuoteColorF = strdup(args[0]);
    flt_basic_cfg.QuoteColorB = strdup(args[1]);
  }

  return 0;
}

void flt_basic_cleanup(void) {
  if(flt_basic_cfg.FontColor)   free(flt_basic_cfg.FontColor);
  if(flt_basic_cfg.FontSize)    free(flt_basic_cfg.FontSize);
  if(flt_basic_cfg.FontFamily)  free(flt_basic_cfg.FontFamily);
  if(flt_basic_cfg.BaseTarget)  free(flt_basic_cfg.BaseTarget);
  if(flt_basic_cfg.QuoteColorF) free(flt_basic_cfg.QuoteColorF);
  if(flt_basic_cfg.QuoteColorB) free(flt_basic_cfg.QuoteColorB);
}

t_conf_opt flt_basic_config[] = {
  { "FontColor",  flt_basic_handle_command, CFG_OPT_USER|CFG_OPT_CONFIG, NULL },
  { "FontSize",   flt_basic_handle_command, CFG_OPT_USER|CFG_OPT_CONFIG, NULL },
  { "FontFamily", flt_basic_handle_command, CFG_OPT_USER|CFG_OPT_CONFIG, NULL },
  { "AutoReload", flt_basic_handle_command, CFG_OPT_USER|CFG_OPT_CONFIG, NULL },
  { "BaseTarget", flt_basic_handle_command, CFG_OPT_USER|CFG_OPT_CONFIG, NULL },
  { "QuoteColor", flt_basic_handle_command, CFG_OPT_USER|CFG_OPT_CONFIG, NULL },
  { NULL, NULL, 0, NULL }
};

t_handler_config flt_basic_handlers[] = {
  { VIEW_INIT_HANDLER, flt_basic_execute_filter },
  { POSTING_HANDLER,   flt_basic_handle_posting },
  { VIEW_LIST_HANDLER, flt_basic_set_target     },
  { 0, NULL }
};

t_module_config flt_basic = {
  flt_basic_config,
  flt_basic_handlers,
  NULL,
  NULL,
  NULL,
  flt_basic_cleanup
};


/* eof */
