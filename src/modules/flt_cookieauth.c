/**
 * \file flt_cookieauth.c
 * \author Christian Kruse, <cjk@wwwtech.de>
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
#include "cfconfig.h"
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
#include "cfgcomp.h"
#include "cfcgi.h"
#include "template.h"
#include "clientlib.h"
/* }}} */

/* {{{ flt_cookieauth_run */
int flt_cookieauth_run(cf_hash_t *head,cf_configuration_t *cfg) {
  cf_cfg_config_value_t *v = cf_cfg_get_value(cfg,"DF:AuthMode"),*cfg_path;
  u_char *path;
  cf_hash_t *cookies;
  cf_string_t *name;

  if(!v || !v->values[0] || cf_strcmp(v->values[0],"cookie") != 0 || flt_cookieauth_name == NULL) return FLT_DECLINE;

  cookies = cf_hash_new(cf_cgi_destroy_entry);
  cf_cgi_parse_cookies(cookies);

  if((name = cf_cgi_get(cookies,flt_cookieauth_name)) != NULL) {
    if((cfg_path = cf_cfg_get_value(cfg,"DF:ConfigDirectory")) != NULL && (path = cf_get_uconf_name(cfg_path->sval,name->content)) != NULL) {
      free(path);
      cf_hash_set(GlobalValues,"UserName",8,name->content,name->len+1);
    }
  }

  cf_hash_destroy(cookies);

  return FLT_OK;
}
/* }}} */

  /**
   * Config options:
   * Cookie:Name = "name";
   */

cf_handler_config_t flt_cookieauth_handlers[] = {
  { AUTH_HANDLER, flt_cookieauth_run },
  { 0, NULL }
};

cf_module_config_t flt_cookieauth = {
  MODULE_MAGIC_COOKIE,
  flt_cookieauth_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};


/* eof */
