/**
 * \file flt_noanonymous.c
 * \author Christian Kruse, <cjk@wwwtech.de>
 *
 * This plugin implements user authentification over the HTTP authentification
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

#define FLT_NA_POST 0x1
#define FLT_NA_READ 0x2

static u_char *flt_noanonymous_fn = NULL;
static int flt_noanonymous_cfg = 0;

/* {{{ flt_noanonymous_run */
int flt_noanonymous_run(cf_hash_t *cgi,configuration_t *dc,configuration_t *vc) {
  name_value_t *cs;
  if(flt_noanonymous_cfg == 0 || flt_noanonymous_cfg == FLT_NA_POST) return FLT_DECLINE;

  if(cf_hash_get(GlobalValues,"UserName",8) != NULL) return FLT_OK;

  cs = cfg_get_first_value(dc,flt_noanonymous_fn,"ExternCharset");
  printf("Status: 403 Forbidden\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
  cf_error_message("E_noanonymous_read",NULL);

  return FLT_EXIT;
}
/* }}} */

/* {{{ flt_noanonymous_post */
#ifdef CF_SHARED_MEM
int flt_noanonymous_post(cf_hash_t *head,configuration_t *dc,configuration_t *pc,message_t *p,void *ptr,int sock,int mode)
#else
int flt_noanonymous_post(cf_hash_t *head,configuration_t *dc,configuration_t *pc,message_t *p,int sock,int mode)
#endif
{
  name_value_t *cs;
  if(flt_noanonymous_cfg == 0) return FLT_DECLINE;

  if(cf_hash_get(GlobalValues,"UserName",8) != NULL) return FLT_OK;

  cs = cfg_get_first_value(dc,flt_noanonymous_fn,"ExternCharset");
  printf("Status: 403 Forbidden\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
  if(flt_noanonymous_cfg == FLT_NA_POST) cf_error_message("E_noanonymous_post",NULL);
  else cf_error_message("E_noanonymous_read",NULL);

  return FLT_EXIT;
}
/* }}} */

/* {{{ flt_noanonymous_handle */
int flt_noanonymous_handle(configfile_t *cfile,conf_opt_t *opt,const u_char *context,u_char **args,size_t argnum) {
  if(flt_noanonymous_fn == NULL) flt_noanonymous_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(flt_noanonymous_fn,context) != 0) return 0;

  if(cf_strcmp(args[0],"post") == 0) flt_noanonymous_cfg =  FLT_NA_POST;
  else if(cf_strcmp(args[0],"read") == 0) flt_noanonymous_cfg = FLT_NA_READ;
  return 0;
}
/* }}} */

conf_opt_t flt_noanonymous_config[] = {
  { "NoAnonymous",  flt_noanonymous_handle,  CFG_OPT_CONFIG|CFG_OPT_LOCAL, NULL },
  { NULL, NULL, 0, NULL }
};

handler_config_t flt_noanonymous_handlers[] = {
  { INIT_HANDLER,     flt_noanonymous_run },
  { NEW_POST_HANDLER, flt_noanonymous_post },
  { 0, NULL }
};

module_config_t flt_noanonymous = {
  MODULE_MAGIC_COOKIE,
  flt_noanonymous_config,
  flt_noanonymous_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};


/* eof */
