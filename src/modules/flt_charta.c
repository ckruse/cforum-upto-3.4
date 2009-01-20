/**
 * \file flt_charta.c
 * \author Christian Seiler, <self@christian-seiler.de>
 *
 * This plugin handles acceptance of the charta
 */

/* {{{ Initial comments */
/*
 * $LastChangedDate: 2005-09-18 17:48:01 +0200 (Sun, 18 Sep 2005) $
 * $LastChangedRevision: 1337 $
 * $LastChangedBy: ckruse $
 *
 */
/* }}} */

/* {{{ Includes */
#include "config.h"
#include "defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <string.h>
#include <ctype.h>
#include <sys/types.h>

#include "readline.h"
#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "cfcgi.h"
#include "template.h"
#include "clientlib.h"
/* }}} */

static u_char *flt_charta_fn = NULL;
unsigned long flt_charta_charta_enabled = 0;

/* {{{ flt_charta_execute_filter */
int flt_charta_execute_filter(cf_hash_t *head,configuration_t *dc,configuration_t *vc,cl_thread_t *thread,cf_template_t *tpl) {
  u_char *UserName = cf_hash_get(GlobalValues,"UserName",8);

  if(flt_charta_charta_enabled) {
    cf_tpl_setvalue(tpl,"charta_showref",TPL_VARIABLE_INT,1);
    if(UserName) cf_tpl_setvalue(tpl,"charta_authed",TPL_VARIABLE_INT,1);

    return FLT_OK;
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_charta_post_display */
int flt_charta_post_display(cf_hash_t *head,configuration_t *dc,configuration_t *pc,cf_template_t *tpl,message_t *p) {
  u_char *UserName = cf_hash_get(GlobalValues,"UserName",8);

  if(flt_charta_charta_enabled) {
    cf_tpl_setvalue(tpl,"charta_showref",TPL_VARIABLE_INT,1);
    if(UserName) cf_tpl_setvalue(tpl,"charta_authed",TPL_VARIABLE_INT,1);

    return FLT_OK;
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_charta_handle_command */
int flt_charta_handle_command(configfile_t *cfile,conf_opt_t *opt,const u_char *context,u_char **args,size_t argnum) {
  if(flt_charta_fn == NULL) flt_charta_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(flt_charta_fn,context) != 0) return 0;

  if(cf_strcmp(opt->name,"ChartaEnable") == 0) {
    if(argnum != 1) return 0;
    flt_charta_charta_enabled = cf_strcmp(args[0],"yes") == 0;
  }

  return 0;
}
/* }}} */

conf_opt_t flt_charta_config[] = {
  { "ChartaEnable",            flt_charta_handle_command,     CFG_OPT_CONFIG|CFG_OPT_LOCAL, NULL},
  { NULL, NULL, 0, NULL }
};

handler_config_t flt_charta_handlers[] = {
  { POSTING_HANDLER,       flt_charta_execute_filter },
  { POST_DISPLAY_HANDLER,  flt_charta_post_display },
  { 0, NULL }
};

module_config_t flt_charta = {
  MODULE_MAGIC_COOKIE,
  flt_charta_config,
  flt_charta_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};

/* eof */
