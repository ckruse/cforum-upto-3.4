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

static u_char *CSSUri = NULL;
static u_char *JSFile = NULL;
static u_char **XSLTUri = NULL;
static int    CSS_Overwrite = 0;

int flt_include_exec_list(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_cf_template *begin,t_cf_template *end) {
  int rc = FLT_DECLINE;

  if(CSSUri) {
    tpl_cf_setvar(begin,"owncss",CSSUri,strlen(CSSUri),1);

    if(CSS_Overwrite) {
      tpl_cf_setvar(begin,"cssoverwrite","1",1,0);
    }

    rc = FLT_OK;
  }

  if(JSFile) {
    rc = FLT_OK;
    tpl_cf_setvar(begin,"ownjs",JSFile,strlen(JSFile),1);
  }

  if(XSLTUri) {
    rc = FLT_OK;

    /* list mode */
    if(end) {
      if(XSLTUri[0]) tpl_cf_setvar(begin,"ownxslt",XSLTUri[0],strlen(XSLTUri[0]),1);
    }
    /* posting mode */
    else {
      if(XSLTUri[1]) tpl_cf_setvar(begin,"ownxslt",XSLTUri[1],strlen(XSLTUri[1]),1);
    }
  }

  return rc;
}

int flt_include_exec_post(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_cl_thread *thread,t_cf_template *tpl) {
  return flt_include_exec_list(head,dc,vc,tpl,NULL);
}

int flt_include_handle(t_configfile *cf,t_conf_opt *opt,u_char **args,int argnum) {
  if(cf_strcmp(opt->name,"OwnCSSFile") == 0) {
    if(CSSUri) free(CSSUri);
    CSSUri = strdup(args[0]);
  }
  else if(cf_strcmp(opt->name,"OwnJSFile") == 0) {
    if(JSFile) free(JSFile);
    JSFile = strdup(args[0]);
  }
  else if(cf_strcmp(opt->name,"OwnXSLTFile") == 0) {
    if(XSLTUri) free(XSLTUri);
    XSLTUri = args;
    return -1;
  }
  else {
    CSS_Overwrite = cf_strcmp(args[0],"yes") == 0;
  }

  return 0;
}

void flt_include_finish(void) {
  if(CSSUri) free(CSSUri);
}

t_conf_opt config[] = {
  { "OwnCSSFile",           flt_include_handle, CFG_OPT_USER, NULL },
  { "OverwriteStandardCSS", flt_include_handle, CFG_OPT_USER, NULL },
  { "OwnJSFile",            flt_include_handle, CFG_OPT_USER, NULL },
  { "OwnXSLTFile",          flt_include_handle, CFG_OPT_USER, NULL },
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
