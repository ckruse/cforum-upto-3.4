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

int flt_tplmode_execute(t_cf_hash *head,t_configuration *dc,t_configuration *vc) {
  t_name_value *v,*v1;

  if(TPLMode) {
    v1 = cfg_get_first_value(dc,"XHTMLMode");
    v  = cfg_get_first_value(dc,"TemplateMode");

    free(v->values[0]);

    v->values[0] = strdup(TPLMode);

    if(strstr(TPLMode,"xhtml")) {
      free(v1->values[0]);
      v1->values[0] = strdup("xhtml");
    }
    else {
      free(v1->values[0]);
      v1->values[0] = strdup("html");
    }

    return FLT_OK;
  }

  return FLT_DECLINE;
}

int flt_tplmode_handle(t_configfile *cfile,t_conf_opt *opt,u_char **args,int argnum) {
  if(TPLMode) free(TPLMode);
  TPLMode = strdup(args[0]);

  return 0;
}

void flt_tplmode_finish(void) {
  if(TPLMode) free(TPLMode);
}

t_conf_opt flt_tplmode_config[] = {
  { "TPLMode",        flt_tplmode_handle, CFG_OPT_USER, NULL },
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
