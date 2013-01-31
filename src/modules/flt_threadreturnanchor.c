/**
 * \file flt_owncss.c
 * \author Christian Seiler, <christian.seiler@selfhtml.org>
 *
 * Jump to the last visited thread using an html anchor when leaving a thread using the main forum link
 */

/* {{{ Initial comments */
/*
 * $LastChangedDate: 2005-10-28 18:10:43 +0200 (Fri, 28 Oct 2005) $
 * $LastChangedRevision: 1481 $
 * $LastChangedBy: ckruse $
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
#include <inttypes.h>

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
int flt_threadreturnanchor_post(cf_hash_t *head,configuration_t *dc,configuration_t *vc,cl_thread_t *thread,cf_template_t *tpl) {
  const cf_tpl_variable_t *path;
  string_t new_path;
  char buf[50];
  size_t len;

  if(ThreadReturnAnchor) {
    path = cf_tpl_getvar(tpl, "forumbase");
    if (!path) return 1;

    str_init(&new_path);
    str_str_set(&new_path,(string_t *)&path->data.d_string);
    str_char_append(&new_path,'#');
    len = snprintf(buf, 50, "t%"PRIu64, thread->tid);
    str_chars_append(&new_path,buf,len);
    cf_tpl_setvalue(tpl,"forumreturn",TPL_VARIABLE_STRING,new_path.content,new_path.len);
    str_cleanup(&new_path);

    return FLT_OK;
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_threadreturnanchor_handle */
int flt_threadreturnanchor_handle(configfile_t *cf,conf_opt_t *opt,const u_char *context,u_char **args,size_t argnum) {
  if(!flt_tra_fn) flt_tra_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(flt_tra_fn,context) != 0) return 0;

  ThreadReturnAnchor = cf_strcmp(args[0],"yes") == 0;

  return 0;
}
/* }}} */

conf_opt_t flt_threadreturnanchor_config[] = {
  { "ThreadReturnAnchor", flt_threadreturnanchor_handle, CFG_OPT_CONFIG|CFG_OPT_USER|CFG_OPT_LOCAL, NULL },
  { NULL, NULL, 0, NULL }
};

handler_config_t flt_threadreturnanchor_handlers[] = {
  { POSTING_HANDLER,   flt_threadreturnanchor_post },
  { 0, NULL }
};

module_config_t flt_threadreturnanchor = {
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
