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

static int    flt_threadreturnanchor_threadreturnanchor = 0;

int flt_threadreturnanchor_post(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_cl_thread *thread,t_cf_template *tpl) {
  const t_cf_tpl_variable *path;
  t_string new_path;
  char buf[20];
  
  if(flt_threadreturnanchor_threadreturnanchor) {
    path = tpl_cf_getvar(tpl, "forumbase");
    if (!path) return 1;

    str_init(&new_path);
    str_str_set(&new_path,path->data);
    str_char_append(&new_path,'#');
    snprintf(buf, 20, "t%ld", (long)thread->tid);
    str_chars_append(&new_path,buf,strlen(buf));
    tpl_cf_setvar(tpl,"forumreturn",new_path.content,new_path.len,0);
    str_cleanup(&new_path);

    return FLT_OK;
  }

  return FLT_DECLINE;
}

int flt_threadreturnanchor_handle(t_configfile *cf,t_conf_opt *opt,const u_char *context,u_char **args,size_t argnum) {
  flt_threadreturnanchor_threadreturnanchor = cf_strcmp(args[0],"yes") == 0;

  return 0;
}

t_conf_opt flt_thread_return_anchor_config[] = {
  { "ThreadReturnAnchor", flt_threadreturnanchor_handle, CFG_OPT_CONFIG|CFG_OPT_USER, NULL },
  { NULL, NULL, 0, NULL }
};

t_handler_config flt_thread_return_anchor_handlers[] = {
  { POSTING_HANDLER,   flt_threadreturnanchor_post },
  { 0, NULL }
};

t_module_config flt_threadreturnanchor = {
  flt_thread_return_anchor_config,
  flt_thread_return_anchor_handlers,
  NULL,
  NULL,
  NULL,
  NULL 
};

/* eof */
