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
#include "hashlib.h"
/* }}} */

t_cf_hash *flt_category_cats = NULL;

void flt_category_parse_list(u_char *vips,t_cf_hash *hash) {
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

int flt_category_execute_filter(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_message *msg,u_int64_t tid,int mode) {
  if(mode || !flt_category_cats) return FLT_DECLINE;

  if(cf_hash_get(flt_category_cats,msg->category,msg->category_len)) return FLT_OK;

  msg->may_show = 0;
  delete_subtree(msg);
  return FLT_OK;
}

int flt_category_handle_command(t_configfile *cf,t_conf_opt *opt,const u_char *context,u_char **args,size_t argnum) {
  if(!flt_category_cats) flt_category_cats = cf_hash_new(NULL);

  flt_category_parse_list(args[0],flt_category_cats);

  return 0;
}

void flt_category_finish(void) {
  if(flt_category_cats) {
    cf_hash_destroy(flt_category_cats);
    flt_category_cats = NULL;
  }
}

t_conf_opt flt_category_config[] = {
  { "ShowCategories", flt_category_handle_command, CFG_OPT_USER|CFG_OPT_CONFIG, NULL },
  { NULL, NULL, 0, NULL }
};

t_handler_config flt_category_handlers[] = {
  { VIEW_LIST_HANDLER, flt_category_execute_filter },
  { 0, NULL }
};

t_module_config flt_category = {
  flt_category_config,
  flt_category_handlers,
  NULL,
  NULL,
  NULL,
  flt_category_finish
};

/* eof */
