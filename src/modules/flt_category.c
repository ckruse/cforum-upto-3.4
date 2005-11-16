/**
 * \file flt_category.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
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

static cf_hash_t *flt_category_cats = NULL;
static int flt_category_hide_in_thread = 0;
static u_char *flt_category_fn = NULL;

/* {{{ flt_category_parse_list */
void flt_category_parse_list(u_char *vips,cf_hash_t *hash) {
  if(vips) {
    u_char *ptr = vips;
    u_char *pos = ptr,*pre = ptr;

    while((pos = strstr(pos,",")) != NULL) {
      *pos = 0;

      cf_hash_set(hash,pre,pos-pre,"1",1);

      pre    = pos+1;
      *pos++ = ',';
    }

    cf_hash_set(hash,pre,strlen(pre),"1",1); /* argh! This sucks */
  }
}
/* }}} */

/* {{{ flt_category_execute_filter */
int flt_category_execute_filter(cf_hash_t *head,cf_configuration_t *dc,cf_configuration_t *vc,message_t *msg,u_int64_t tid,int mode) {
  if(!flt_category_cats || (mode & CF_MODE_POST)) return FLT_DECLINE;
  if((mode & CF_MODE_THREADVIEW) && flt_category_hide_in_thread == 0) return FLT_DECLINE;

  if(cf_hash_get(flt_category_cats,msg->category.content,msg->category.len)) return FLT_OK;

  msg->may_show = 0;
  cf_msg_delete_subtree(msg);
  return FLT_OK;
}
/* }}} */

/* {{{ flt_category_handle_command */
int flt_category_handle_command(cf_configfile_t *cf,cf_conf_opt_t *opt,const u_char *context,u_char **args,size_t argnum) {
  if(flt_category_fn == NULL) flt_category_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(flt_category_fn,context) != 0) return 0;

  if(cf_strcmp(opt->name,"ShowCategories") == 0) {
    if(!flt_category_cats) flt_category_cats = cf_hash_new(NULL);

    flt_category_parse_list(args[0],flt_category_cats);
  }
  else {
    flt_category_hide_in_thread = cf_strcmp(args[0],"yes") == 0;
  }

  return 0;
}
/* }}} */

/* {{{ flt_category_finish */
void flt_category_finish(void) {
  if(flt_category_cats) {
    cf_hash_destroy(flt_category_cats);
    flt_category_cats = NULL;
  }
}
/* }}} */

cf_conf_opt_t flt_category_config[] = {
  { "ShowCategories",             flt_category_handle_command, CF_CFG_OPT_USER|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, NULL },
  { "HideCategoriesInThreadView", flt_category_handle_command, CF_CFG_OPT_USER|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, NULL },
  { NULL, NULL, 0, NULL }
};

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
  flt_category_finish
};

/* eof */
