/**
 * \file flt_chooser.c
 * \author Christian Kruse, <cjk@wwwtech.de>
 *
 * This plugin implements a board-like category chooser
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
/* }}} */

int flt_chooser_execute(cf_hash_t *head,cf_configuration_t *cfg) {
  cf_cfg_config_value_t *cs,*cats,*chsr_tpl,*activate = cf_cfg_get_value(cfg,"Chooser:Enable");
  u_char buff[512];
  cf_template_t tpl;
  cf_tpl_variable_t ary;
  size_t i;

  if(activate || activate->ival == 0) return FLT_DECLINE;

  if(head) {
    if(cf_cgi_get(head,"cat") != NULL) return FLT_DECLINE; /* he already chose a category */
    if(cf_cgi_get(head,"t") != NULL) return FLT_DECLINE;   /* he wants to view a thread or a message */
  }

  cs = cf_cfg_get_value(cfg,"DF:ExternCharset");
  if((chsr_tpl = cf_cfg_get_value(cfg,"Chooser:Template")) == NULL || chsr_tpl->type != CF_ASM_ARG_STR) return FLT_DECLINE;

  /* generate page */
  cf_gen_tpl_name(buff,512,);

  if(cf_tpl_init(&tpl,buff) != 0) {
    printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
    cf_error_message("E_TPL_NOT_FOUND",NULL);
    return FLT_EXIT;
  }

  printf("Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);

  cf_tpl_var_init(&ary,TPL_VARIABLE_ARRAY);
  cats = cf_cfg_get_value(cfg,"DF:Categories");

  for(i=0;i<cats->valnum;++i) cf_tpl_var_addvalue(&ary,TPL_VARIABLE_STRING,cats->values[i],strlen(cats->values[i]));

  cf_tpl_setvar(&tpl,"cats",&ary);
  cf_tpl_parse(&tpl);
  cf_tpl_finish(&tpl);

  return FLT_EXIT;
}

int flt_chooser_list(cf_hash_t *head,cf_configuration_t *dc,cf_configuration_t *vc,message_t *msg,u_int64_t tid,int mode) {
  cf_string_t *val;

  if(head == NULL) return FLT_DECLINE;
  if(msg->category.len == 0) return FLT_DECLINE;
  if((val = cf_cgi_get(head,"cat")) == NULL) return FLT_DECLINE;

  if(cf_strcmp(val->content,msg->category.content) != 0) msg->may_show = 0;

  return FLT_OK;
}

/**
 * Config options:
 * Chooser:Activate = yes|no;
 * Chooser:Template = "template";
 */

cf_handler_config_t flt_chooser_handlers[] = {
  { INIT_HANDLER,      flt_chooser_execute },
  { VIEW_LIST_HANDLER, flt_chooser_list },
  { 0, NULL }
};

cf_module_config_t flt_chooser = {
  MODULE_MAGIC_COOKIE,
  flt_chooser_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};
/* eof */
