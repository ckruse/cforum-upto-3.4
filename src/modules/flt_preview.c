/*
 * \file flt_preview.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 * \brief Posting preview
 *
 * This file is a plugin for fo_post. It gives the user the
 * ability to preview his postings.
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
#include "fo_post.h"
/* }}} */

static u_char *flt_preview_datefmt = NULL;

int flt_preview_execute(t_cf_hash *head,t_configuration *dc,t_configuration *pc,t_message *p,int sock,int mode) {
  u_char *date;
  size_t len;
  t_name_value *v;

  if(head) {
    if(cf_cgi_get(head,"preview")) {
      v = cfg_get_first_value(dc,NULL,"DateLocale");

      date = general_get_time(flt_preview_datefmt,v->values[0],&len,&p->date);
      _cf_cgi_save_param(head,"date",4,date);

      free(date);
    }
  }

  return FLT_DECLINE;
}

int flt_preview_cmd(t_configfile *cfile,t_conf_opt *opt,const u_char *context,u_char **args,size_t argnum) {
  if(flt_preview_datefmt) free(flt_preview_datefmt);
  flt_preview_datefmt = strdup(args[0]);

  return 0;
}

t_conf_opt flt_preview_config[] = {
  { "PreviewDateFormat", flt_preview_cmd, CFG_OPT_CONFIG, NULL },
  { NULL, NULL, 0, NULL }
};

t_handler_config flt_preview_handlers[] = {
  { NEW_POST_HANDLER, flt_preview_execute },
  { 0, NULL }
};

t_module_config flt_preview = {
  flt_preview_config,
  flt_preview_handlers,
  NULL,
  NULL,
  NULL,
  NULL
};


/* eof */
