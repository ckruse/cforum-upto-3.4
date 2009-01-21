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

/* {{{ flt_charta_execute_filter */
int flt_charta_execute_filter(cf_hash_t *head,cf_configuration_t *cfg,cf_cl_thread_t *thread,cf_template_t *tpl) {
  u_char *UserName = cf_hash_get(GlobalValues,"UserName",8);
  cf_cfg_config_value_t *enabled = cf_cfg_get_value(cfg,"Charta:Enable");

  if(enabled && enabled->ival) {
    cf_tpl_setvalue(tpl,"charta_showref",TPL_VARIABLE_INT,1);
    if(UserName) cf_tpl_setvalue(tpl,"charta_authed",TPL_VARIABLE_INT,1);

    return FLT_OK;
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_charta_post_display */
int flt_charta_post_display(cf_hash_t *head,cf_configuration_t *cfg,cf_template_t *tpl,cf_message_t *p) {
  u_char *UserName = cf_hash_get(GlobalValues,"UserName",8);
  cf_cfg_config_value_t *enabled = cf_cfg_get_value(cfg,"Charta:Enable");

  if(enabled && enabled->ival) {
    cf_tpl_setvalue(tpl,"charta_showref",TPL_VARIABLE_INT,1);
    if(UserName) cf_tpl_setvalue(tpl,"charta_authed",TPL_VARIABLE_INT,1);

    return FLT_OK;
  }

  return FLT_DECLINE;
}
/* }}} */

/**
 * config options:
 * Charta:Enable = Yes|No;
*/

cf_handler_config_t flt_charta_handlers[] = {
  { POSTING_HANDLER,       flt_charta_execute_filter },
  { POST_DISPLAY_HANDLER,  flt_charta_post_display },
  { 0, NULL }
};

cf_module_config_t flt_charta = {
  MODULE_MAGIC_COOKIE,
  flt_charta_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};

/* eof */
