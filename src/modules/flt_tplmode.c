/**
 * \file flt_tplmode.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 *
 * This plugin implements user configurable template modes
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
int flt_tplmode_execute(t_cf_hash *head,t_configuration *dc,t_configuration *vc) {
  t_name_value *v,*v1;
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
int flt_tplmode_handle(t_configfile *cfile,t_conf_opt *opt,const u_char *context,u_char **args,size_t argnum) {
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

t_conf_opt flt_tplmode_config[] = {
  { "TPLMode",        flt_tplmode_handle, CFG_OPT_USER|CFG_OPT_LOCAL, NULL },
  { NULL, NULL, 0, NULL }
};

t_handler_config flt_tplmode_handlers[] = {
  { INIT_HANDLER, flt_tplmode_execute },
  { 0, NULL }
};

t_module_config flt_tplmode = {
  flt_tplmode_config,
  flt_tplmode_handlers,
  NULL,
  NULL,
  NULL,
  flt_tplmode_finish
};

/* eof */
