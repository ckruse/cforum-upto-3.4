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

static u_char **CSSUri   = NULL;
static size_t CSSLen     = 0;
static u_char **JSFile   = NULL;
static size_t JSLen      = 0;

static u_char **XSLTUri  = NULL;
static int CSS_Overwrite = 0;
static u_char *InlineCSS = NULL;

static u_char *flt_include_fn = NULL;

/* {{{ flt_include_exec_list */
int flt_include_exec_list(cf_hash_t *head,configuration_t *dc,configuration_t *vc,cf_template_t *begin,cf_template_t *end) {
  int rc = FLT_DECLINE;
  cf_tpl_variable_t ary;
  size_t i;

  if(CSSLen) {
    cf_tpl_var_init(&ary,TPL_VARIABLE_ARRAY);

    for(i=0;i<CSSLen;++i) cf_tpl_var_addvalue(&ary,TPL_VARIABLE_STRING,CSSUri[i],strlen(CSSUri[i]));
    //cf_tpl_setvalue(begin,"owncss",TPL_VARIABLE_STRING,CSSUri,strlen(CSSUri));
    cf_tpl_setvar(begin,"owncss",&ary);

    if(CSS_Overwrite) cf_tpl_setvalue(begin,"cssoverwrite",TPL_VARIABLE_STRING,"1",1);

    rc = FLT_OK;
  }

  if(JSFile) {
    cf_tpl_var_init(&ary,TPL_VARIABLE_ARRAY);

    for(i=0;i<JSLen;++i) cf_tpl_var_addvalue(&ary,TPL_VARIABLE_STRING,JSFile[i],strlen(JSFile[i]));
    //cf_tpl_setvalue(begin,"ownjs",TPL_VARIABLE_STRING,JSFile,strlen(JSFile));
    cf_tpl_setvar(begin,"ownjs",&ary);

    rc = FLT_OK;
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

  if(InlineCSS) cf_tpl_setvalue(begin,"inlinecss",TPL_VARIABLE_STRING,InlineCSS,strlen(InlineCSS));

  return rc;
}
/* }}} */

/* {{{ flt_include_exec_post */
int flt_include_exec_post(cf_hash_t *head,configuration_t *dc,configuration_t *vc,cl_thread_t *thread,cf_template_t *tpl) {
  return flt_include_exec_list(head,dc,vc,tpl,NULL);
}
/* }}} */

/* {{{ flt_incldue_handle */
int flt_include_handle(configfile_t *cf,conf_opt_t *opt,const u_char *context,u_char **args,size_t argnum) {
  if(flt_include_fn == NULL) flt_include_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(flt_include_fn,context) != 0) return 0;

  if(cf_strcmp(opt->name,"OwnCSSFile") == 0) CSSLen = split(args[0],",",&CSSUri);
  else if(cf_strcmp(opt->name,"OwnJSFile") == 0) JSLen = split(args[0],",",&JSFile);
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

/* {{{ flt_include_finish */
void flt_include_finish(void) {
  size_t i;
  if(CSSUri) {
    for(i=0;i<CSSLen;++i) free(CSSUri[i]);
    free(CSSUri);
    CSSLen = 0;
  }
  if(InlineCSS) free(InlineCSS);
  if(JSFile) {
    for(i=0;i<JSLen;++i) free(JSFile[i]);
    free(JSFile);
    JSLen = 0;
  }

  if(XSLTUri) {
    if(XSLTUri[0]) free(XSLTUri[0]);
    if(XSLTUri[1]) free(XSLTUri[1]);
    free(XSLTUri);
  }
}
/* }}} */

conf_opt_t flt_include_config[] = {
  { "OwnCSSFile",           flt_include_handle, CFG_OPT_USER|CFG_OPT_LOCAL, NULL },
  { "OverwriteStandardCSS", flt_include_handle, CFG_OPT_USER|CFG_OPT_LOCAL, NULL },
  { "OwnJSFile",            flt_include_handle, CFG_OPT_USER|CFG_OPT_LOCAL, NULL },
  { "OwnXSLTFile",          flt_include_handle, CFG_OPT_USER|CFG_OPT_LOCAL, NULL },
  { "InlineCSS",            flt_include_handle, CFG_OPT_USER|CFG_OPT_LOCAL, NULL },
  { NULL, NULL, 0, NULL }
};

handler_config_t flt_include_handlers[] = {
  { VIEW_INIT_HANDLER, flt_include_exec_list },
  { POSTING_HANDLER,   flt_include_exec_post },
  { 0, NULL }
};

module_config_t flt_include = {
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
