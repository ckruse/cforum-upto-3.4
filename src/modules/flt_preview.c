/*
 * \file flt_preview.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
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
#include "config.h"
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
#include "configparser.h"
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

/* {{{ flt_preview_execute */
int flt_preview_execute(t_cf_hash *head,t_configuration *dc,t_configuration *pc,t_message *p,int sock,int mode) {
  u_char *date;
  size_t len;
  t_name_value *v;
  u_char *forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  t_string cnt;

  if(head) {
    if(cf_cgi_get(head,"preview")) {
      v = cfg_get_first_value(dc,forum_name,"DateLocale");

      date = cf_general_get_time(flt_preview_datefmt,v->values[0],&len,&p->date);
      cf_cgi_set(head,"date",date);
      free(date);

      v = cfg_get_first_value(pc,forum_name,"QuotingChars");
      str_init(&cnt);
      msg_to_html(NULL,p->content.content,&cnt,NULL,v->values[0],-1,1);

      cf_cgi_set(head,"ne_message",cnt.content);
      str_cleanup(&cnt);

      if(cf_cgi_get(head,"preview") != NULL) {
        cf_cgi_set(head,"genprev","1");
        cf_hash_entry_delete(head,"preview",7);
      }
      else cf_cgi_set(head,"preview","1");

      flt_preview_is_preview = 1;

      display_posting_form(head,p);
      return FLT_EXIT;
    }
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_preview_variables */
int flt_preview_variables(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_cl_thread *thread,t_cf_template *tpl) {

  if(flt_preview_gen_prev) {
    if(flt_preview_is_preview == 0) cf_tpl_setvalue(tpl,"preview",TPL_VARIABLE_STRING,"1",1);
  }

  if(flt_preview_sw_type == 0) cf_tpl_setvalue(tpl,"previewswitchtype",TPL_VARIABLE_STRING,"checkbox",8);
  else if(flt_preview_sw_type == 1) cf_tpl_setvalue(tpl,"previewswitchtype",TPL_VARIABLE_STRING,"button",6);

  return FLT_OK;
}
/* }}} */

int flt_preview_variables_posting(t_cf_hash *head,t_configuration *dc,t_configuration *pc,t_cf_template *tpl) {
  return flt_preview_variables(NULL,NULL,NULL,NULL,tpl);
}

/* {{{ flt_preview_cmd */
int flt_preview_cmd(t_configfile *cfile,t_conf_opt *opt,const u_char *context,u_char **args,size_t argnum) {
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

t_conf_opt flt_preview_config[] = {
  { "PreviewDateFormat", flt_preview_cmd, CFG_OPT_CONFIG|CFG_OPT_LOCAL, NULL },
  { "GeneratePreview",   flt_preview_cmd, CFG_OPT_CONFIG|CFG_OPT_USER|CFG_OPT_LOCAL, NULL },
  { "PreviewSwitchType", flt_preview_cmd, CFG_OPT_CONFIG|CFG_OPT_USER|CFG_OPT_LOCAL, NULL },
  { NULL, NULL, 0, NULL }
};

t_handler_config flt_preview_handlers[] = {
  { NEW_POST_HANDLER,     flt_preview_execute },
  { POSTING_HANDLER,      flt_preview_variables },
  { POST_DISPLAY_HANDLER, flt_preview_variables_posting },
  { 0, NULL }
};

t_module_config flt_preview = {
  flt_preview_config,
  flt_preview_handlers,
  NULL,
  NULL,
  NULL,
  NULL
};


/* eof */
