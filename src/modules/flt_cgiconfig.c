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
int flt_cgiconfig_post(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_cl_thread *thread,t_cf_template *tpl) {
  u_char *forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  u_char *UserName = cf_hash_get(GlobalValues,"UserName",8);
  u_char *link;

  size_t l;

  t_name_value *v = cfg_get_first_value(dc,forum_name,UserName ? "UPostingURL" : "PostingURL"),
    *cs = cfg_get_first_value(dc,forum_name,"ExternCharset");

  /* {{{ ShowThread links */
  link = cf_advanced_get_link(v->values[0],thread->tid,thread->threadmsg->mid,NULL,1,&l,"showthread","part");
  cf_set_variable(tpl,cs,"showthread_part",link,l,1);
  free(link);

  link = cf_advanced_get_link(v->values[0],thread->tid,thread->threadmsg->mid,NULL,1,&l,"showthread","none");
  cf_set_variable(tpl,cs,"showthread_none",link,l,1);
  free(link);

  link = cf_advanced_get_link(v->values[0],thread->tid,thread->threadmsg->mid,NULL,1,&l,"showthread","full");
  cf_set_variable(tpl,cs,"showthread_full",link,l,1);
  free(link);
  /* }}} */

  /* {{{ ReadMode links */
  link = cf_advanced_get_link(v->values[0],thread->tid,thread->threadmsg->mid,NULL,1,&l,"readmode","list");
  cf_set_variable(tpl,cs,"readmode_list",link,l,1);
  free(link);

  link = cf_advanced_get_link(v->values[0],thread->tid,thread->threadmsg->mid,NULL,1,&l,"readmode","nested");
  cf_set_variable(tpl,cs,"readmode_nested",link,l,1);
  free(link);

  link = cf_advanced_get_link(v->values[0],thread->tid,thread->threadmsg->mid,NULL,1,&l,"readmode","thread");
  cf_set_variable(tpl,cs,"readmode_thread",link,l,1);
  free(link);
  /* }}} */

  return FLT_OK;
}
/* }}} */

/* {{{ flt_cgiconfig_init_handler */
int flt_cgiconfig_init_handler(t_cf_hash *head,t_configuration *dc,t_configuration *vc) {
  t_name_value *v;
  u_char *val,*forum_name;

  if(head) {
    forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);

    if((val = cf_cgi_get(head,"showthread")) != NULL) {
      v = cfg_get_first_value(vc,forum_name,"ShowThread");

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
    }

    if((val = cf_cgi_get(head,"readmode")) != NULL) {
      v = cfg_get_first_value(vc,forum_name,"ReadMode");

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
    }
  }

  return FLT_DECLINE;
}
/* }}} */

t_conf_opt flt_cgiconfig_config[] = {
  { NULL, NULL, 0, NULL }
};

t_handler_config flt_cgiconfig_handlers[] = {
  { POSTING_HANDLER,      flt_cgiconfig_post },
  { INIT_HANDLER,         flt_cgiconfig_init_handler },
  { 0, NULL }
};

t_module_config flt_cgiconfig = {
  flt_cgiconfig_config,
  flt_cgiconfig_handlers,
  NULL,
  NULL,
  NULL,
  NULL
};



/* eof */
