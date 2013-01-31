/**
 * \file flt_tplmode.c
 * \author Christian Kruse, <cjk@wwwtech.de>
 *
 * This plugin implements user configurable template modes
 */

/* {{{ Initial comments */
/*
 * $LastChangedDate: 2009-01-16 14:32:24 +0100 (Fri, 16 Jan 2009) $
 * $LastChangedRevision: 1639 $
 * $LastChangedBy: ckruse $
 *
 */
/* }}} */

/* {{{ Includes */
#include "config.h"
#include "defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>

#include <time.h>

#include "readline.h"
#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "cfcgi.h"
#include "template.h"
#include "clientlib.h"
/* }}} */

static u_char *TPLMode = NULL;
static u_char *flt_tplmode_fn = NULL;

/* {{{ flt_tplmode_execute */
int flt_tplmode_execute(cf_hash_t *head,configuration_t *dc,configuration_t *vc) {
  name_value_t *v,*v1;
  u_char *forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);

  if(TPLMode) {
    v1 = cfg_get_first_value(dc,forum_name,"XHTMLMode");
    v  = cfg_get_first_value(dc,forum_name,"TemplateMode");

    free(v->values[0]);

    v->values[0] = strdup(TPLMode);

    if(strstr(TPLMode,"xhtml")) {
      free(v1->values[0]);
      v1->values[0] = strdup("yes");
    }
    else {
      free(v1->values[0]);
      v1->values[0] = strdup("no");
    }

    return FLT_OK;
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_tplmode_handle */
int flt_tplmode_handle(configfile_t *cfile,conf_opt_t *opt,const u_char *context,u_char **args,size_t argnum) {
  if(!flt_tplmode_fn) flt_tplmode_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(flt_tplmode_fn,context) != 0) return 0;

  if(TPLMode) free(TPLMode);
  TPLMode = strdup(args[0]);

  return 0;
}
/* }}} */

/* {{{ flt_tplmode_finish */
void flt_tplmode_finish(void) {
  if(TPLMode) free(TPLMode);
}
/* }}} */

conf_opt_t flt_tplmode_config[] = {
  { "TPLMode",        flt_tplmode_handle, CFG_OPT_USER|CFG_OPT_LOCAL, NULL },
  { NULL, NULL, 0, NULL }
};

handler_config_t flt_tplmode_handlers[] = {
  { INIT_HANDLER, flt_tplmode_execute },
  { 0, NULL }
};

module_config_t flt_tplmode = {
  MODULE_MAGIC_COOKIE,
  flt_tplmode_config,
  flt_tplmode_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  flt_tplmode_finish
};

/* eof */
