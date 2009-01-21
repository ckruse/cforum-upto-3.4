/**
 * \file flt_categorycheck.c
 * \author Christian Kruse, <cjk@wwwtech.de>
 *
 * This file is a plugin for fo_post. It checks if the category
 * the user posted is correct
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
#include <ctype.h>
#include <string.h>
#include <sys/types.h>

#include <errno.h>

#include "readline.h"
#include "hashlib.h"
#include "utils.h"
#include "cfgcomp.h"
#include "cfcgi.h"
#include "template.h"
#include "clientlib.h"
#include "charconvert.h"
#include "fo_post.h"
/* }}} */

#ifdef CF_SHARED_MEM
int flt_categorycheck_execute(cf_hash_t *head,cf_configuration_t *dc,cf_configuration_t *pc,message_t *p,cl_thread_t *thr,void *ptr,int sock,int mode)
#else
int flt_categorycheck_execute(cf_hash_t *head,cf_configuration_t *dc,cf_configuration_t *pc,message_t *p,cl_thread_t *thr,int sock,int mode)
#endif
{
  u_char *forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  cf_name_value_t *v = cf_cfg_get_first_value(&fo_default_conf,forum_name,"DF:Categories");
  size_t i;

  if(!p->category.len) return FLT_DECLINE;
  for(i=0;i<v->valnum;++i) {
    if(cf_strcmp(p->category.content,v->values[i]) == 0) return FLT_OK;
  }

  strcpy(ErrorString,"E_posting_category");
  display_posting_form(head,p,NULL);
  return FLT_EXIT;
}


cf_conf_opt_t flt_categorycheck_config[] = {
  { NULL, NULL, 0, NULL }
};

cf_handler_config_t flt_categorycheck_handlers[] = {
  { NEW_POST_HANDLER, flt_categorycheck_execute },
  { 0, NULL }
};

cf_module_config_t flt_categorycheck = {
  MODULE_MAGIC_COOKIE,
  flt_categorycheck_config,
  flt_categorycheck_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};

/* eof */
