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

t_cf_hash *Cats = NULL;

void parse_list(u_char *vips,t_cf_hash *hash) {
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

int execute_filter(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_message *msg,u_int64_t tid,int mode) {
  if(Cats && mode == 0) {
    if(cf_hash_get(Cats,msg->category,strlen(msg->category))) {
      return FLT_OK;
    }

    msg->may_show = 0;
    delete_subtree(msg);
    return FLT_OK;
  }

  return FLT_DECLINE;
}

int flt_cat_handle_command(t_configfile *cf,t_conf_opt *opt,u_char **args,int argnum) {
  if(!Cats) Cats = cf_hash_new(NULL);

  parse_list(args[0],Cats);

  return 0;
}

void flt_cat_finish(void) {
  if(Cats) {
    cf_hash_destroy(Cats);
    Cats = NULL;
  }
}

t_conf_opt config[] = {
  { "ShowCategories", flt_cat_handle_command, CFG_OPT_USER|CFG_OPT_CONFIG, NULL },
  { NULL, NULL, 0, NULL }
};

t_handler_config handlers[] = {
  { VIEW_LIST_HANDLER, execute_filter },
  { 0, NULL }
};

t_module_config flt_category = {
  config,
  handlers,
  NULL,
  NULL,
  NULL,
  flt_cat_finish
};

/* eof */
