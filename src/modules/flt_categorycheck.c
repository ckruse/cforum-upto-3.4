/**
 * \file flt_categorycheck.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
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
#include "config.h"
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
#include "configparser.h"
#include "cfcgi.h"
#include "template.h"
#include "clientlib.h"
#include "charconvert.h"
#include "fo_post.h"
/* }}} */

#ifdef CF_SHARED_MEM
int flt_categorycheck_execute(t_cf_hash *head,t_configuration *dc,t_configuration *pc,t_message *p,void *ptr,int sock,int mode)
#else
int flt_categorycheck_execute(t_cf_hash *head,t_configuration *dc,t_configuration *pc,t_message *p,int sock,int mode)
#endif
{
  u_char *forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  t_name_value *v = cfg_get_first_value(&fo_default_conf,forum_name,"Categories");
  size_t i;

  if(!p->category.len) return FLT_DECLINE;
  for(i=0;i<v->valnum;++i) {
    if(cf_strcmp(p->category.content,v->values[i]) == 0) return FLT_OK;
  }

  strcpy(ErrorString,"E_posting_category");
  display_posting_form(head,p);
  return FLT_EXIT;
}


t_conf_opt flt_categorycheck_config[] = {
  { NULL, NULL, 0, NULL }
};

t_handler_config flt_categorycheck_handlers[] = {
  { NEW_POST_HANDLER, flt_categorycheck_execute },
  { 0, NULL }
};

t_module_config flt_categorycheck = {
  flt_categorycheck_config,
  flt_categorycheck_handlers,
  NULL,
  NULL,
  NULL,
  NULL
};

/* eof */
