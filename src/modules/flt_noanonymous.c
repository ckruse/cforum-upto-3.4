/**
 * \file flt_noanonymous.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
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

int flt_noanonymous_run(t_cf_hash *cgi,t_configuration *dc,t_configuration *vc) {
  t_name_value *cs;
  if(flt_noanonymous_cfg == 0 || flt_noanonymous_cfg == FLT_NA_POST) return FLT_DECLINE;

  if(cf_hash_get(GlobalValues,"UserName",8) != NULL) return FLT_OK;

  cs = cfg_get_first_value(dc,flt_noanonymous_fn,"ExternCharset");
  printf("Status: 403 Forbidden\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
  cf_error_message("E_noanonymous_read",NULL);

  return FLT_EXIT;
}

#ifdef CF_SHARED_MEM
int flt_noanonymous_post(t_cf_hash *head,t_configuration *dc,t_configuration *pc,t_message *p,void *ptr,int sock,int mode)
#else
int flt_noanonymous_post(t_cf_hash *head,t_configuration *dc,t_configuration *pc,t_message *p,int sock,int mode)
#endif
{
  t_name_value *cs;
  if(flt_noanonymous_cfg == 0) return FLT_DECLINE;

  if(cf_hash_get(GlobalValues,"UserName",8) != NULL) return FLT_OK;

  cs = cfg_get_first_value(dc,flt_noanonymous_fn,"ExternCharset");
  printf("Status: 403 Forbidden\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
  if(flt_noanonymous_cfg == FLT_NA_POST) cf_error_message("E_noanonymous_post",NULL);
  else cf_error_message("E_noanonymous_read",NULL);

  return FLT_EXIT;
}

int flt_noanonymous_handle(t_configfile *cfile,t_conf_opt *opt,const u_char *context,u_char **args,size_t argnum) {
  if(flt_noanonymous_fn == NULL) flt_noanonymous_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(flt_noanonymous_fn,context) != 0) return 0;

  if(cf_strcmp(args[0],"post") == 0) flt_noanonymous_cfg =  FLT_NA_POST;
  else if(cf_strcmp(args[0],"read") == 0) flt_noanonymous_cfg = FLT_NA_READ;
  return 0;
}

t_conf_opt flt_noanonymous_config[] = {
  { "NoAnonymous",  flt_noanonymous_handle,  CFG_OPT_CONFIG|CFG_OPT_LOCAL, NULL },
  { NULL, NULL, 0, NULL }
};

t_handler_config flt_noanonymous_handlers[] = {
  { INIT_HANDLER,     flt_noanonymous_run },
  { NEW_POST_HANDLER, flt_noanonymous_post },
  { 0, NULL }
};

t_module_config flt_noanonymous = {
  flt_noanonymous_config,
  flt_noanonymous_handlers,
  NULL,
  NULL,
  NULL,
  NULL
};


/* eof */
