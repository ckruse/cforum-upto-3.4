/**
 * \file flt_preview.c
 * \author Christian Kruse, <cjk@wwwtech.de>
 * \brief Posting preview
 *
 * This file is a plugin for fo_post. It gives the user the
 * ability to preview his postings.
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

#include <time.h>

#include "readline.h"
#include "hashlib.h"
#include "utils.h"
#include "cfgcomp.h"
#include "cfcgi.h"
#include "template.h"
#include "clientlib.h"
#include "htmllib.h"
#include "fo_post.h"
/* }}} */

static u_char *flt_preview_datefmt = NULL;
static int flt_preview_gen_prev = 0;
static int flt_preview_sw_type = 0;
static int flt_preview_is_preview = 0;

static u_char *flt_preview_fn = NULL;

/* {{{ flt_preview_execute */
#ifdef CF_SHARED_MEM
int flt_preview_execute(cf_hash_t *head,cf_configuration_t *dc,cf_configuration_t *pc,message_t *p,cl_thread_t *thr,void *ptr,int sock,int mode)
#else
int flt_preview_execute(cf_hash_t *head,cf_configuration_t *dc,cf_configuration_t *pc,message_t *p,cl_thread_t *thr,int sock,int mode)
#endif
{
  u_char *date;
  size_t len;
  cf_name_value_t *v;
  u_char *forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  cf_string_t cnt;

  if(head) {
    if(cf_cgi_get(head,"preview")) {
      v = cf_cfg_get_first_value(dc,forum_name,"DF:DateLocale");

      date = cf_general_get_time(flt_preview_datefmt,v->values[0],&len,&p->date);
      cf_cgi_set(head,"date",date,len);
      free(date);

      v = cf_cfg_get_first_value(pc,forum_name,"DF:QuotingChars");
      cf_str_init(&cnt);
      msg_to_html(NULL,p->content.content,&cnt,NULL,v->values[0],-1,1);

      cf_cgi_set(head,"ne_message",cnt.content,cnt.len);
      cf_str_cleanup(&cnt);

      if(cf_cgi_get(head,"preview") != NULL) {
        cf_cgi_set(head,"genprev","1",2);
        cf_hash_entry_delete(head,"preview",7);
      }
      else cf_cgi_set(head,"preview","1",2);

      flt_preview_is_preview = 1;

      display_posting_form(head,p,NULL);
      return FLT_EXIT;
    }
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_preview_variables */
int flt_preview_variables(cf_hash_t *head,cf_configuration_t *dc,cf_configuration_t *vc,cl_thread_t *thread,cf_template_t *tpl) {

  if(flt_preview_gen_prev) {
    if(flt_preview_is_preview == 0) cf_tpl_setvalue(tpl,"preview",TPL_VARIABLE_STRING,"1",1);
  }

  if(flt_preview_sw_type == 0) cf_tpl_setvalue(tpl,"previewswitchtype",TPL_VARIABLE_STRING,"checkbox",8);
  else if(flt_preview_sw_type == 1) cf_tpl_setvalue(tpl,"previewswitchtype",TPL_VARIABLE_STRING,"button",6);

  return FLT_OK;
}
/* }}} */

int flt_preview_variables_posting(cf_hash_t *head,cf_configuration_t *dc,cf_configuration_t *pc,cf_template_t *tpl) {
  return flt_preview_variables(NULL,NULL,NULL,NULL,tpl);
}

/* {{{ flt_preview_cmd */
int flt_preview_cmd(cf_configfile_t *cfile,cf_conf_opt_t *opt,const u_char *context,u_char **args,size_t argnum) {
  if(flt_preview_fn == NULL) flt_preview_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(flt_preview_fn,context) != 0) return 0;

  if(cf_strcmp(opt->name,"PreviewDateFormat") == 0) {
    if(flt_preview_datefmt) free(flt_preview_datefmt);
    flt_preview_datefmt = strdup(args[0]);
  }
  else if(cf_strcmp(opt->name,"GeneratePreview") == 0) {
    flt_preview_gen_prev = cf_strcmp(args[0],"yes") == 0;
  }
  else {
    if(cf_strcmp(args[0],"button") == 0) flt_preview_sw_type = 1;
    else if(cf_strcmp(args[0],"checkbox") == 0) flt_preview_sw_type = 0;
  }

  return 0;
}
/* }}} */

cf_conf_opt_t flt_preview_config[] = {
  { "Preview:DateFormat", flt_preview_cmd, CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, NULL },
  { "Preview:Generate",   flt_preview_cmd, CF_CFG_OPT_CONFIG|CF_CFG_OPT_USER|CF_CFG_OPT_LOCAL, NULL },
  { "Preview:SwitchType", flt_preview_cmd, CF_CFG_OPT_CONFIG|CF_CFG_OPT_USER|CF_CFG_OPT_LOCAL, NULL },
  { NULL, NULL, 0, NULL }
};

cf_handler_config_t flt_preview_handlers[] = {
  { NEW_POST_HANDLER,     flt_preview_execute },
  { POSTING_HANDLER,      flt_preview_variables },
  { POST_DISPLAY_HANDLER, flt_preview_variables_posting },
  { 0, NULL }
};

cf_module_config_t flt_preview = {
  MODULE_MAGIC_COOKIE,
  flt_preview_config,
  flt_preview_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};


/* eof */
