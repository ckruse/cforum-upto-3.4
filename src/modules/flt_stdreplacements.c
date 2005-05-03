/**
 * \file flt_stdreplacements.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 *
 * This plugin handles some standard text replacements
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
#include <time.h>

#include <string.h>
#include <ctype.h>
#include <sys/types.h>

#include <pcre.h>

#include "readline.h"
#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "cfcgi.h"
#include "template.h"
#include "clientlib.h"
#include "charconvert.h"
#include "validate.h"
#include "htmllib.h"
/* }}} */

static int flt_stdrepl_smileys   = 0;
static u_char *flt_stdrepl_theme = NULL;

int flt_stdreplacements_smileys(t_configuration *fdc,t_configuration *fvc,t_cl_thread *thread,const u_char *directive,const u_char **parameters,size_t plen,t_string *bco,t_string *bci,t_string *content,t_string *cite,const u_char *qchars,int sig) {
  return FLT_DECLINE;
}

int flt_stdreplacements_filter(t_configuration *fdc,t_configuration *fvc,t_cl_thread *thread,const u_char *directive,const u_char **parameters,size_t plen,t_string *bco,t_string *bci,t_string *content,t_string *cite,const u_char *qchars,int sig) {
  if(cf_strcmp(directive,"...") == 0) {
    str_chars_append(content,"&#8230;",7);
    if(cite) str_chars_append(cite,"...",3);
    return FLT_OK;
  }
  else {
    return FLT_OK;
  }

  return FLT_DECLINE;
}

/* {{{ flt_directives_init */
int flt_stdreplacements_init(t_cf_hash *cgi,t_configuration *dc,t_configuration *vc) {
  if(flt_stdrepl_smileys) {
    /* broad grinning */
    cf_html_register_textfilter(":D",flt_stdreplacements_smileys);
    cf_html_register_textfilter(":P",flt_stdreplacements_smileys);

    /* smiling */
    cf_html_register_textfilter(":)",flt_stdreplacements_smileys);
    cf_html_register_textfilter(":-)",flt_stdreplacements_smileys);

    /* unhappy */
    cf_html_register_textfilter(":(",flt_stdreplacements_smileys);
    cf_html_register_textfilter(":-(",flt_stdreplacements_smileys);

    /* surprised */
    cf_html_register_textfilter(":o",flt_stdreplacements_smileys);

    /* don't know */
    cf_html_register_textfilter(":?",flt_stdreplacements_smileys);
    cf_html_register_textfilter(":-/",flt_stdreplacements_smileys);


    /* neutral */
    cf_html_register_textfilter(":|",flt_stdreplacements_smileys);

    /* evil neutral */
    cf_html_register_textfilter("&gt;:|",flt_stdreplacements_smileys);

    /* evil grin */
    cf_html_register_textfilter("&gt;:P",flt_stdreplacements_smileys);
    cf_html_register_textfilter("&gt;:)",flt_stdreplacements_smileys);
    cf_html_register_textfilter(":-&gt;",flt_stdreplacements_smileys);
  }

  cf_html_register_textfilter("...",flt_stdreplacements_filter);

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_stdreplacements_handle */
int flt_stdreplacements_handle(t_configfile *cfile,t_conf_opt *opt,const u_char *context,u_char **args,size_t argnum) {
  if(cf_strcmp(args[0],"ReplaceSmileys") == 0) flt_stdrepl_smileys = cf_strcmp(args[0],"yes") == 0;
  else flt_stdrepl_theme = strdup(args[0]);

  return 0;
}
/* }}} */

t_conf_opt flt_stdreplacements_config[] = {
  { "ReplaceSmileys",   flt_stdreplacements_handle,  CFG_OPT_CONFIG|CFG_OPT_USER|CFG_OPT_LOCAL,  NULL },
  { "SmileyTheme",      flt_stdreplacements_handle,  CFG_OPT_CONFIG|CFG_OPT_USER|CFG_OPT_LOCAL,  NULL },

  { NULL, NULL, 0, NULL }
};

t_handler_config flt_stdreplacements_handlers[] = {
  { INIT_HANDLER,        flt_stdreplacements_init },
  { 0, NULL }
};

t_module_config flt_stdreplacements = {
  flt_stdreplacements_config,
  flt_stdreplacements_handlers,
  NULL,
  NULL,
  NULL,
  NULL
};


/* eof */
