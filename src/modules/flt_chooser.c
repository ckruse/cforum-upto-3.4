/**
 * \file flt_chooser.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
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
/* }}} */

static u_char *flt_chooser_template = NULL;
static int flt_chooser_activate = 0;

static u_char *flt_chooser_fn = NULL;

int flt_chooser_execute(t_cf_hash *head,t_configuration *dc,t_configuration *vc) {
  t_name_value *cs,*cats;
  u_char buff[512];
  t_cf_template tpl;
  t_cf_tpl_variable ary;
  size_t i;

  if(flt_chooser_activate == 0) return FLT_DECLINE;

  if(head) {
    if(cf_cgi_get(head,"cat") != NULL) return FLT_DECLINE; /* he already chose a category */
    if(cf_cgi_get(head,"t") != NULL) return FLT_DECLINE;   /* he wants to view a thread or a message */
  }

  /* generate page */
  cs = cfg_get_first_value(dc,flt_chooser_fn,"ExternCharset");
  cf_gen_tpl_name(buff,512,flt_chooser_template);

  if(cf_tpl_init(&tpl,buff) != 0) {
    printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
    cf_error_message("E_TPL_NOT_FOUND",NULL);
    return FLT_EXIT;
  }

  printf("Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);

  cf_tpl_var_init(&ary,TPL_VARIABLE_ARRAY);
  cats = cfg_get_first_value(dc,flt_chooser_fn,"Categories");

  for(i=0;i<cats->valnum;++i) cf_tpl_var_addvalue(&ary,TPL_VARIABLE_STRING,cats->values[i],strlen(cats->values[i]));

  cf_tpl_setvar(&tpl,"cats",&ary);
  cf_tpl_parse(&tpl);
  cf_tpl_finish(&tpl);

  return FLT_EXIT;
}

int flt_chooser_list(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_message *msg,u_int64_t tid,int mode) {
  u_char *val;

  if(head == NULL) return FLT_DECLINE;
  if(msg->category.len == 0) return FLT_DECLINE;
  if((val = cf_cgi_get(head,"cat")) == NULL) return FLT_DECLINE;

  if(cf_strcmp(val,msg->category.content) != 0) msg->may_show = 0;

  return FLT_OK;
}

int flt_chooser_handle(t_configfile *f,t_conf_opt *opt,const u_char *context,u_char **args,size_t argnum) {
  if(flt_chooser_fn == NULL) flt_chooser_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(context,flt_chooser_fn) != 0) return 0;

  if(cf_strcmp(opt->name,"ActivateChooser") == 0) flt_chooser_activate = cf_strcmp(args[0],"yes") == 0;
  else if(cf_strcmp(opt->name,"TemplateChooser") == 0) flt_chooser_template = strdup(args[0]);

  return 0;
}

void flt_chooser_finish(void) {
  if(flt_chooser_template) free(flt_chooser_template);
}

t_conf_opt flt_chooser_config[] = {
  { "ActivateChooser",     flt_chooser_handle, CFG_OPT_CONFIG|CFG_OPT_USER|CFG_OPT_LOCAL,   NULL },
  { "TemplateChooser",     flt_chooser_handle, CFG_OPT_CONFIG|CFG_OPT_NEEDED|CFG_OPT_LOCAL, NULL },
  { NULL, NULL, 0, NULL }
};

t_handler_config flt_chooser_handlers[] = {
  { INIT_HANDLER,      flt_chooser_execute },
  { VIEW_LIST_HANDLER, flt_chooser_list },
  { 0, NULL }
};

t_module_config flt_chooser = {
  MODULE_MAGIC_COOKIE,
  flt_chooser_config,
  flt_chooser_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  flt_chooser_finish
};
/* eof */
