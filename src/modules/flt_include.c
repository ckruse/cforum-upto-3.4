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
#include "cfconfig.h"
#include "defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

static u_char *CSSUri = NULL;
static u_char *JSFile = NULL;
static u_char **XSLTUri = NULL;
static int    CSS_Overwrite = 0;
static u_char *InlineCSS = NULL;

static u_char *flt_include_fn = NULL;

/* {{{ flt_include_exec_list */
int flt_include_exec_list(cf_hash_t *head,cf_configuration_t *dc,cf_configuration_t *vc,cf_template_t *begin,cf_template_t *end) {
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

  if(InlineCSS) {
    cf_tpl_setvalue(begin,"inlinecss",TPL_VARIABLE_STRING,InlineCSS,strlen(InlineCSS));
  }

  return rc;
}
/* }}} */

/* {{{ flt_include_exec_post */
int flt_include_exec_post(cf_hash_t *head,cf_configuration_t *dc,cf_configuration_t *vc,cl_thread_t *thread,cf_template_t *tpl) {
  return flt_include_exec_list(head,dc,vc,tpl,NULL);
}
/* }}} */

/* {{{ flt_incldue_handle */
int flt_include_handle(cf_configfile_t *cf,cf_conf_opt_t *opt,const u_char *context,u_char **args,size_t argnum) {
  if(flt_include_fn == NULL) flt_include_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(flt_include_fn,context) != 0) return 0;

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
  else if(cf_strcmp(opt->name,"InlineCSS") == 0) {
    if(InlineCSS) free(InlineCSS);
    InlineCSS = strdup(args[0]);
  }
  else CSS_Overwrite = cf_strcmp(args[0],"yes") == 0;

  return 0;
}
/* }}} */

void flt_include_finish(void) {
  if(CSSUri) free(CSSUri);
  if(InlineCSS) free(InlineCSS);
  if(JSFile) free(JSFile);

  if(XSLTUri) {
    if(XSLTUri[0]) free(XSLTUri[0]);
    if(XSLTUri[1]) free(XSLTUri[1]);
    free(XSLTUri);
  }
}

cf_conf_opt_t flt_include_config[] = {
  { "OwnCSSFile",           flt_include_handle, CF_CFG_OPT_USER|CF_CFG_OPT_LOCAL, NULL },
  { "OverwriteStandardCSS", flt_include_handle, CF_CFG_OPT_USER|CF_CFG_OPT_LOCAL, NULL },
  { "OwnJSFile",            flt_include_handle, CF_CFG_OPT_USER|CF_CFG_OPT_LOCAL, NULL },
  { "OwnXSLTFile",          flt_include_handle, CF_CFG_OPT_USER|CF_CFG_OPT_LOCAL, NULL },
  { "InlineCSS",            flt_include_handle, CF_CFG_OPT_USER|CF_CFG_OPT_LOCAL, NULL },
  { NULL, NULL, 0, NULL }
};

cf_handler_config_t flt_include_handlers[] = {
  { VIEW_INIT_HANDLER, flt_include_exec_list },
  { POSTING_HANDLER,   flt_include_exec_post },
  { 0, NULL }
};

cf_module_config_t flt_include = {
  MODULE_MAGIC_COOKIE,
  flt_include_config,
  flt_include_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  flt_include_finish
};

/* eof */
