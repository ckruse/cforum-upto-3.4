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

/* {{{ flt_include_exec_list */
int flt_include_exec_list(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_cf_template *begin,t_cf_template *end) {
  int rc = FLT_DECLINE;

  if(CSSUri) {
    cf_tpl_setvalue(begin,"owncss",TPL_VARIABLE_STRING,CSSUri,strlen(CSSUri));

    if(CSS_Overwrite) cf_tpl_setvalue(begin,"cssoverwrite",TPL_VARIABLE_STRING,"1",1);

    rc = FLT_OK;
  }

  if(JSFile) {
    rc = FLT_OK;
    cf_tpl_setvalue(begin,"ownjs",TPL_VARIABLE_STRING,JSFile,strlen(JSFile));
  }

  if(XSLTUri) {
    rc = FLT_OK;

    /* list mode */
    if(end) {
      if(XSLTUri[0]) cf_tpl_setvalue(begin,"ownxslt",TPL_VARIABLE_STRING,XSLTUri[0],strlen(XSLTUri[0]));
    }
    /* posting mode */
    else {
      if(XSLTUri[1]) cf_tpl_setvalue(begin,"ownxslt",TPL_VARIABLE_STRING,XSLTUri[1],strlen(XSLTUri[1]));
    }
  }

  return rc;
}
/* }}} */

/* {{{ flt_include_exec_post */
int flt_include_exec_post(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_cl_thread *thread,t_cf_template *tpl) {
  return flt_include_exec_list(head,dc,vc,tpl,NULL);
}
/* }}} */

/* {{{ flt_incldue_handle */
int flt_include_handle(t_configfile *cf,t_conf_opt *opt,const u_char *context,u_char **args,size_t argnum) {
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
  else CSS_Overwrite = cf_strcmp(args[0],"yes") == 0;

  return 0;
}
/* }}} */

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
