/**
 * \file flt_include.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 *
 * Let the user include some own files (e.g. CSS or JS)
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

static u_char *flt_include_cssuri = NULL;
static u_char *flt_include_jsfile = NULL;
static int    flt_include_css_overwrite = 0;

int flt_include_exec_list(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_cf_template *begin,t_cf_template *end) {
  int rc = FLT_DECLINE;

  if(flt_include_cssuri) {
    tpl_cf_setvar(begin,"owncss",flt_include_cssuri,strlen(flt_include_cssuri),1);

    if(flt_include_css_overwrite) {
      tpl_cf_setvar(begin,"cssoverwrite","1",1,0);
    }

    rc = FLT_OK;
  }

  if(flt_include_jsfile) {
    rc = FLT_OK;
    tpl_cf_setvar(begin,"ownjs",flt_include_jsfile,strlen(flt_include_jsfile),1);
  }


  return rc;
}

int flt_include_exec_post(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_cl_thread *thread,t_cf_template *tpl) {
  return flt_include_exec_list(head,dc,vc,tpl,NULL);
}

int flt_include_handle(t_configfile *cf,t_conf_opt *opt,const u_char *context,u_char **args,size_t argnum) {
  if(cf_strcmp(opt->name,"OwnCSSFile") == 0) {
    if(flt_include_cssuri) free(flt_include_cssuri);
    flt_include_cssuri = strdup(args[0]);
  }
  else if(cf_strcmp(opt->name,"Ownflt_include_jsfile") == 0) {
    if(flt_include_jsfile) free(flt_include_jsfile);
    flt_include_jsfile = strdup(args[0]);
  }
  else {
    flt_include_css_overwrite = cf_strcmp(args[0],"yes") == 0;
  }

  return 0;
}

void flt_include_finish(void) {
  if(flt_include_cssuri) free(flt_include_cssuri);
}

t_conf_opt config[] = {
  { "OwnCSSFile",           flt_include_handle, CFG_OPT_USER, NULL },
  { "OverwriteStandardCSS", flt_include_handle, CFG_OPT_USER, NULL },
  { "OwnJSFile",            flt_include_handle, CFG_OPT_USER, NULL },
  { NULL, NULL, 0, NULL }
};

t_handler_config handlers[] = {
  { VIEW_INIT_HANDLER, flt_include_exec_list },
  { POSTING_HANDLER,   flt_include_exec_post },
  { 0, NULL }
};

t_module_config flt_include = {
  config,
  handlers,
  NULL,
  NULL,
  NULL,
  flt_include_finish
};

/* eof */
