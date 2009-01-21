/**
 * \file flt_category.c
 * \author Christian Kruse, <cjk@wwwtech.de>
 *
 * This plugin filters threads in user view
 *
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

#include "readline.h"
#include "hashlib.h"
#include "utils.h"
#include "cfgcomp.h"
#include "cfcgi.h"
#include "template.h"
#include "clientlib.h"
#include "hashlib.h"
/* }}} */

/* {{{ flt_category_execute_filter */
int flt_category_execute_filter(cf_hash_t *head,cf_configuration_t *cfg,message_t *msg,u_int64_t tid,int mode) {
  int i;

  cf_cfg_config_value_t *cats = cf_cfg_get_value(cfg,"Categories:Show"),*threadview;
  if(cats == NULL || (mode & CF_MODE_POST)) return FLT_DECLINE;
  if((mode & CF_MODE_THREADVIEW) && ((threadview = cf_cfg_get_value(cfg,"Categories:HideInThreadView")) == NULL || threadview->ival == 0)) return FLT_DECLINE;

  for(i=0;i<cats->alen;++i) {
    if(cf_strcmp(cats->avals[i].sval,msg->category.content) == 0) return FLT_OK;
  }

  msg->may_show = 0;
  cf_msg_delete_subtree(msg);
  return FLT_OK;
}
/* }}} */

/**
 * config values:
 * Categories:Show = ("Cate","gories");
 * Categories:HideInThreadView = Yes|No;
*/

cf_handler_config_t flt_category_handlers[] = {
  { VIEW_LIST_HANDLER, flt_category_execute_filter },
  { 0, NULL }
};

cf_module_config_t flt_category = {
  MODULE_MAGIC_COOKIE,
  flt_category_config,
  flt_category_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};

/* eof */
