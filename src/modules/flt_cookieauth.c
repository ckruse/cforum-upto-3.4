/**
 * \file flt_cookieauth.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 *
 * This plugin implements user authentification over cookies
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

static u_char *flt_cookieauth_name  = NULL;
static u_char *flt_cookieauth_fn = NULL;

/* {{{ flt_cookieauth_run */
int flt_cookieauth_run(t_cf_hash *head,t_configuration *dc,t_configuration *vc) {
  u_char *forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  t_name_value *v = cfg_get_first_value(dc,forum_name,"AuthMode");
  u_char *name,*path;
  t_cf_hash *cookies;

  if(!v || !v->values[0] || cf_strcmp(v->values[0],"cookie") != 0 || flt_cookieauth_name == NULL) return FLT_DECLINE;

  cookies = cf_hash_new(cf_cgi_destroy_entry);
  cf_cgi_parse_cookies(cookies);

  if((name = cf_cgi_get(cookies,flt_cookieauth_name)) != NULL) {
    if((path = cf_get_uconf_name(name)) != NULL) {
      free(path);
      cf_hash_set(GlobalValues,"UserName",8,name,strlen(name)+1);
    }
  }

  cf_hash_destroy(cookies);

  return FLT_OK;
}
/* }}} */

/* {{{ flt_cookieauth_handle */
int flt_cookieauth_handle(t_configfile *cfile,t_conf_opt *opt,const u_char *context,u_char **args,size_t argnum) {
  if(flt_cookieauth_fn == NULL) flt_cookieauth_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(flt_cookieauth_fn,context) != 0) return 0;

  if(flt_cookieauth_name) free(flt_cookieauth_name);
  flt_cookieauth_name = strdup(args[0]);

  return 0;
}
/* }}} */

void flt_cookieauth_cleanup(void) {
  if(flt_cookieauth_name) free(flt_cookieauth_name);
}

t_conf_opt flt_cookieauth_config[] = {
  { "CookieName", flt_cookieauth_handle, CFG_OPT_CONFIG|CFG_OPT_LOCAL, NULL },
  { NULL, NULL, 0, NULL }
};

t_handler_config flt_cookieauth_handlers[] = {
  { AUTH_HANDLER, flt_cookieauth_run },
  { 0, NULL }
};

t_module_config flt_cookieauth = {
  MODULE_MAGIC_COOKIE,
  flt_cookieauth_config,
  flt_cookieauth_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  flt_cookieauth_cleanup
};


/* eof */
