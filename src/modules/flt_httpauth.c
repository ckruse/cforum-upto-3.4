/**
 * \file flt_httpauth.c
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

/* {{{ flt_httpauth_run */
int flt_httpauth_run(cf_hash_t *head,cf_configuration_t *dc,cf_configuration_t *vc) {
  u_char *forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  cf_name_value_t *v = cf_cfg_get_first_value(dc,forum_name,"DF:AuthMode");
  u_char *name,*path;

  if(!v || !v->values[0] || cf_strcmp(v->values[0],"http") != 0) return FLT_DECLINE;

  name = getenv("REMOTE_USER");
  if(name) {
    path = cf_get_uconf_name(name);

    if(path) {
      free(path);
      cf_hash_set(GlobalValues,"UserName",8,name,strlen(name)+1);
    }
  }

  return FLT_OK;
}
/* }}} */

cf_conf_opt_t flt_httpauth_config[] = {
  { NULL, NULL, 0, NULL }
};

cf_handler_config_t flt_httpauth_handlers[] = {
  { AUTH_HANDLER, flt_httpauth_run },
  { 0, NULL }
};

cf_module_config_t flt_httpauth = {
  MODULE_MAGIC_COOKIE,
  flt_httpauth_config,
  flt_httpauth_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};


/* eof */
