/**
 * \file flt_owncss.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 *
 * Let the user choose an own css file
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

#include "readline.h"
#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "cfcgi.h"
#include "template.h"
#include "clientlib.h"
/* }}} */

static u_char *CSSUri = NULL;
static int    CSS_Overwrite = 0;

int exec_owncss_list(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_cf_template *begin,t_cf_template *end) {
  if(CSSUri) {
    tpl_cf_setvar(begin,"owncss",CSSUri,strlen(CSSUri),1);

    if(CSS_Overwrite) {
      tpl_cf_setvar(begin,"cssoverwrite","1",1,0);
    }

    return FLT_OK;
  }

  return FLT_DECLINE;
}

int flt_owncss_post(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_cl_thread *thread,t_cf_template *tpl) {
  if(CSSUri) {
    tpl_cf_setvar(tpl,"owncss",CSSUri,strlen(CSSUri),1);

    if(CSS_Overwrite) {
      tpl_cf_setvar(tpl,"cssoverwrite","1",1,0);
    }

    return FLT_OK;
  }

  return FLT_DECLINE;
}

int flt_owncss_handle(t_configfile *cf,t_conf_opt *opt,u_char **args,int argnum) {
  if(cf_strcmp(opt->name,"OwnCSSFile") == 0) {
    if(CSSUri) free(CSSUri);
    CSSUri = strdup(args[0]);
  }
  else {
    CSS_Overwrite = cf_strcmp(args[0],"yes") == 0;
  }

  return 0;
}

void flt_owncss_finish(void) {
  if(CSSUri) free(CSSUri);
}

t_conf_opt config[] = {
  { "OwnCSSFile",           flt_owncss_handle, CFG_OPT_USER, NULL },
  { "OverwriteStandardCSS", flt_owncss_handle, CFG_OPT_USER, NULL },
  { NULL, NULL, 0, NULL }
};

t_handler_config handlers[] = {
  { VIEW_INIT_HANDLER, exec_owncss_list },
  { POSTING_HANDLER,   flt_owncss_post },
  { 0, NULL }
};

t_module_config flt_owncss = {
  config,
  handlers,
  NULL,
  NULL,
  NULL,
  flt_owncss_finish
};

/* eof */
