/**
 * \file flt_cgiconfig.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 *
 * This filter can changes several config options by cgi
 * parameters
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
#include <time.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>

#include <db.h>

#include "readline.h"
#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "cfcgi.h"
#include "template.h"
#include "clientlib.h"
/* }}} */

/* {{{ flt_cgiconfig_post */
int flt_cgiconfig_post(cf_hash_t *head,cf_configuration_t *dc,cf_configuration_t *vc,cl_thread_t *thread,cf_template_t *tpl) {
  u_char *forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  u_char *UserName = cf_hash_get(GlobalValues,"UserName",8);
  u_char *link,*tmp = NULL;

  size_t l;

  cf_name_value_t *cs = cf_cfg_get_first_value(dc,forum_name,"ExternCharset");
  cf_readmode_t *rm = cf_hash_get(GlobalValues,"RM",2);

  /* {{{ ShowThread links */
  if((tmp = cf_cgi_get(head,"showthread")) != NULL) cf_remove_static_uri_flag("showthread");

  link = cf_advanced_get_link(rm->posting_uri[UserName?1:0],thread->tid,thread->threadmsg->mid,NULL,1,&l,"showthread","part");
  cf_set_variable(tpl,cs,"showthread_part",link,l,1);
  free(link);

  link = cf_advanced_get_link(rm->posting_uri[UserName?1:0],thread->tid,thread->threadmsg->mid,NULL,1,&l,"showthread","none");
  cf_set_variable(tpl,cs,"showthread_none",link,l,1);
  free(link);

  link = cf_advanced_get_link(rm->posting_uri[UserName?1:0],thread->tid,thread->threadmsg->mid,NULL,1,&l,"showthread","full");
  cf_set_variable(tpl,cs,"showthread_full",link,l,1);
  free(link);

  if(tmp) cf_add_static_uri_flag("showthread",tmp,0);
  /* }}} */

  /* {{{ ReadMode links */
  if((tmp = cf_cgi_get(head,"readmode")) != NULL) cf_remove_static_uri_flag("readmode");

  link = cf_advanced_get_link(rm->posting_uri[UserName?1:0],thread->tid,thread->threadmsg->mid,NULL,1,&l,"readmode","list");
  cf_set_variable(tpl,cs,"readmode_list",link,l,1);
  free(link);

  link = cf_advanced_get_link(rm->posting_uri[UserName?1:0],thread->tid,thread->threadmsg->mid,NULL,1,&l,"readmode","nested");
  cf_set_variable(tpl,cs,"readmode_nested",link,l,1);
  free(link);

  link = cf_advanced_get_link(rm->posting_uri[UserName?1:0],thread->tid,thread->threadmsg->mid,NULL,1,&l,"readmode","thread");
  cf_set_variable(tpl,cs,"readmode_thread",link,l,1);
  free(link);

  if(tmp) cf_add_static_uri_flag("readmode",tmp,0);
  /* }}} */

  return FLT_OK;
}
/* }}} */

/* {{{ flt_cgiconfig_init_handler */
int flt_cgiconfig_init_handler(cf_hash_t *head,cf_configuration_t *dc,cf_configuration_t *vc) {
  cf_name_value_t *v;
  u_char *val,*forum_name;

  if(head) {
    forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);

    if((val = cf_cgi_get(head,"showthread")) != NULL) {
      v = cf_cfg_get_first_value(vc,forum_name,"ShowThread");

      if(cf_strcmp(val,"part") == 0) {
        free(v->values[0]);
        v->values[0] = strdup("partitial");
      }
      else if(cf_strcmp(val,"none") == 0) {
        free(v->values[0]);
        v->values[0] = strdup("none");
      }
      else if(cf_strcmp(val,"full") == 0) {
        free(v->values[0]);
        v->values[0] = strdup("full");
      }

      cf_add_static_uri_flag("showthread",val,0);
    }

    if((val = cf_cgi_get(head,"readmode")) != NULL) {
      v = cf_cfg_get_first_value(vc,forum_name,"ReadMode");

      if(cf_strcmp(val,"list") == 0) {
        free(v->values[0]);
        v->values[0] = strdup("list");
      }
      else if(cf_strcmp(val,"nested") == 0) {
        free(v->values[0]);
        v->values[0] = strdup("nested");
      }
      else if(cf_strcmp(val,"thread") == 0) {
        free(v->values[0]);
        v->values[0] = strdup("thread");
      }

      cf_add_static_uri_flag("readmode",val,0);
    }
  }

  return FLT_DECLINE;
}
/* }}} */

cf_conf_opt_t flt_cgiconfig_config[] = {
  { NULL, NULL, 0, NULL }
};

cf_handler_config_t flt_cgiconfig_handlers[] = {
  { POSTING_HANDLER,      flt_cgiconfig_post },
  { INIT_HANDLER,         flt_cgiconfig_init_handler },
  { 0, NULL }
};

cf_module_config_t flt_cgiconfig = {
  MODULE_MAGIC_COOKIE,
  flt_cgiconfig_config,
  flt_cgiconfig_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};



/* eof */
