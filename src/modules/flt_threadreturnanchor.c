/**
 * \file flt_owncss.c
 * \author Christian Seiler, <christian.seiler@selfhtml.org>
 *
 * Jump to the last visited thread using an html anchor when leaving a thread using the main forum link
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

#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "cfcgi.h"
#include "template.h"
#include "readline.h"
#include "clientlib.h"
/* }}} */

static int    ThreadReturnAnchor = 0;
static u_char *flt_tra_fn = NULL;

/* {{{ flt_threadreturnanchor_post */
int flt_threadreturnanchor_post(cf_hash_t *head,cf_configuration_t *dc,cf_configuration_t *vc,cl_thread_t *thread,cf_template_t *tpl) {
  const cf_tpl_variable_t *path;
  cf_string_t new_path;
  char buf[50];
  size_t len;

  if(ThreadReturnAnchor) {
    path = cf_tpl_getvar(tpl, "forumbase");
    if (!path) return 1;

    cf_str_init(&new_path);
    cf_str_str_set(&new_path,(cf_string_t *)&path->data.d_string);
    cf_str_char_append(&new_path,'#');
    len = snprintf(buf, 50, "t%llu", thread->tid);
    cf_str_chars_append(&new_path,buf,len);
    cf_tpl_setvalue(tpl,"forumreturn",TPL_VARIABLE_STRING,new_path.content,new_path.len);
    cf_str_cleanup(&new_path);

    return FLT_OK;
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_threadreturnanchor_handle */
int flt_threadreturnanchor_handle(cf_configfile_t *cf,cf_conf_opt_t *opt,const u_char *context,u_char **args,size_t argnum) {
  if(!flt_tra_fn) flt_tra_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(flt_tra_fn,context) != 0) return 0;

  ThreadReturnAnchor = cf_strcmp(args[0],"yes") == 0;

  return 0;
}
/* }}} */

cf_conf_opt_t flt_threadreturnanchor_config[] = {
  { "ThreadReturnAnchor", flt_threadreturnanchor_handle, CF_CFG_OPT_CONFIG|CF_CFG_OPT_USER|CF_CFG_OPT_LOCAL, NULL },
  { NULL, NULL, 0, NULL }
};

cf_handler_config_t flt_threadreturnanchor_handlers[] = {
  { POSTING_HANDLER,   flt_threadreturnanchor_post },
  { 0, NULL }
};

cf_module_config_t flt_threadreturnanchor = {
  MODULE_MAGIC_COOKIE,
  flt_threadreturnanchor_config,
  flt_threadreturnanchor_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL 
};

/* eof */
