/**
 * \file flt_threadlist.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 *
 * This filter can changes a threadlist view option on
 * runtime
 *
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
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>

#include <db.h>

#include "readline.h"
#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "cfcgi.h"
#include "template.h"
#include "clientlib.h"
/* }}} */

/* {{{ flt_threadlist_init_handler */
int flt_threadlist_init_handler(t_cf_hash *head,t_configuration *dc,t_configuration *vc) {
  t_name_value *v;
  u_char *val,*forum_name;

  if(head) {
    if((val = cf_cgi_get(head,"showthread")) != NULL) {
      forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);
      v = cfg_get_first_value(vc,forum_name,"ShowThread");

      if(cf_strcmp(val,"p") == 0) {
        free(v->values[0]);
        v->values[0] = strdup("partitial");
      }
      else if(cf_strcmp(val,"n") == 0) {
        free(v->values[0]);
        v->values[0] = strdup("none");
      }
      else if(cf_strcmp(val,"f") == 0) {
        free(v->values[0]);
        v->values[0] = strdup("full");
      }

      return FLT_OK;
    }
  }

  return FLT_DECLINE;
}
/* }}} */

t_conf_opt flt_threadlist_config[] = {
  { NULL, NULL, 0, NULL }
};

t_handler_config flt_threadlist_handlers[] = {
  { INIT_HANDLER,         flt_threadlist_init_handler },
  { 0, NULL }
};

t_module_config flt_threadlist = {
  flt_threadlist_config,
  flt_threadlist_handlers,
  NULL,
  NULL,
  NULL,
  NULL
};



/* eof */
