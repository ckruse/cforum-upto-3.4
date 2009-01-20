/**
 * \file flt_stdreplacements.c
 * \author Christian Kruse, <cjk@wwwtech.de>
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
#include "cfconfig.h"
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
#include "cfgcomp.h"
#include "cfcgi.h"
#include "template.h"
#include "clientlib.h"
#include "charconvert.h"
#include "validate.h"
#include "htmllib.h"
/* }}} */

static int flt_stdrepl_smileys   = 0;
static u_char *flt_stdrepl_theme = NULL;

static u_char *flt_stdrepl_fname = NULL;

/* {{{ smileys */
static const u_char *flt_stdrepl_smiles[] = {
  ":)",":-)",":o)","*g*",

  /* ironic */
  ";)",";-)",

  /* unhappy */
  ":(",":-(",

  /* tounge grin */
  ":-P",":P",":-p",":p",
    
  /* broad grinning */
  ":D",":-D",":-))",";-))",

  /* crying */
  ":'-(",":'(",";-(",";(",

  /* surprised */
  ":-O",":O",":-o",":o",

  /* sleeping */
  ":-Z",":Z",":-z",":z",

  /* confused */
  ":-S",":S",":-s",":s",

  /* kiss */
  ":-X",":X",":-x",":x",

  /* devil */
  "&gt;:-)","&gt;:)","}:-&gt;","}:&gt;",":-&gt;", "&gt;;-)","*fg*",

  /* angel */
  "O:-)","O:)","o:-)","o:)",

  /* sunglasses */
  "8-)","8)",

  /* scream */
  ":-@",":@",

  /* smoking */
  ":-Q",":Q",":-q",":q",

  /* angry */
  "&gt;:-|",":-[",

  "*lol*","lol","*rotfl*","rotfl",

  NULL
};
/* }}} */

/* {{{ smiley replacements */
static u_char *flt_stdrepl_smiley_replacements[] = {
  "smile","smile","smile","smile",

  /* ironic */
  "ironic","ironic",

  /* unhappy */
  "unhappy","unhappy",

  /* tounge grin */
  "tongue","tongue","tongue","tongue",
    
  /* broad grinning */
  "biggrin","biggrin","biggrin","biggrin",

  /* crying */
  "cry","cry","cry","cry",

  /* surprised */
  "oh","oh","oh","oh",

  /* sleeping */
  "sleep","sleep","sleep","sleep",

  /* confused */
  "confused","confused","confused","confused",

  /* kiss */
  "kiss","kiss","kiss","kiss",

  /* devil */
  "devil","devil","devil","devil","devil","devil","devil",

  /* angel */
  "angel","angel","angel","angel",

  /* sunglasses */
  "sunglasses","sunglasses",

  /* scream */
  "scream","scream",

  /* smoking */
  "smoke","smoke","smoke","smoke",

  /* angry */
  "angry","angry",

  "lol","lol","lol","lol",

  NULL
};
/* }}} */


/* {{{ flt_stdreplacements_smileys */
int flt_stdreplacements_smileys(cf_configuration_t *fdc,cf_configuration_t *fvc,cl_thread_t *thread,const u_char *directive,const u_char **parameters,size_t plen,cf_string_t *bco,cf_string_t *bci,cf_string_t *content,cf_string_t *cite,const u_char *qchars,int sig) {
  int i;
  cf_name_value_t *xhtml;

  if(!flt_stdrepl_theme) return FLT_DECLINE;
  if(flt_stdrepl_fname == NULL) flt_stdrepl_fname = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  xhtml = cf_cfg_get_first_value(fdc,flt_stdrepl_fname,"XHTMLMode");

  for(i=0;flt_stdrepl_smiles[i];++i) {
    if(cf_strcmp(directive,flt_stdrepl_smiles[i]) == 0) {
      cf_str_chars_append(content,"<img src=\"",10);
      cf_str_chars_append(content,flt_stdrepl_theme,strlen(flt_stdrepl_theme));
      cf_str_chars_append(content,flt_stdrepl_smiley_replacements[i],strlen(flt_stdrepl_smiley_replacements[i]));
      cf_str_chars_append(content,".png\" alt=\"",11);
      cf_str_chars_append(content,flt_stdrepl_smiley_replacements[i],strlen(flt_stdrepl_smiley_replacements[i]));
      cf_str_char_append(content,'"');
      if(xhtml && cf_strcmp(xhtml->values[0],"yes") == 0) cf_str_chars_append(content," /",2);
      cf_str_char_append(content,'>');

      if(cite && sig == 0) cf_str_chars_append(cite,directive,strlen(directive));
      return FLT_OK;
    }
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_stdreplacements_filter */
int flt_stdreplacements_filter(cf_configuration_t *fdc,cf_configuration_t *fvc,cl_thread_t *thread,const u_char *directive,const u_char **parameters,size_t plen,cf_string_t *bco,cf_string_t *bci,cf_string_t *content,cf_string_t *cite,const u_char *qchars,int sig) {
  if(cf_strcmp(directive,"...") == 0) {
    cf_str_chars_append(content,"&#8230;",7);
    if(cite && sig == 0) cf_str_chars_append(cite,"...",3);
    return FLT_OK;
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_directives_init */
int flt_stdreplacements_init(cf_hash_t *cgi,cf_configuration_t *dc,cf_configuration_t *vc) {
  int i;

  /* {{{ smiley-definitions */
  if(flt_stdrepl_smileys) {
    for(i=0;flt_stdrepl_smiles[i];++i) cf_html_register_textfilter(flt_stdrepl_smiles[i],flt_stdreplacements_smileys);
  }
  /* }}} */

  cf_html_register_textfilter("...",flt_stdreplacements_filter);

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_stdreplacements_handle */
int flt_stdreplacements_handle(cf_configfile_t *cfile,cf_conf_opt_t *opt,const u_char *context,u_char **args,size_t argnum) {
  if(flt_stdrepl_fname == NULL) flt_stdrepl_fname = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(context,flt_stdrepl_fname) != 0) return 0;

  if(cf_strcmp(opt->name,"ReplaceSmileys") == 0) flt_stdrepl_smileys = cf_strcmp(args[0],"yes") == 0;
  else {
    if(flt_stdrepl_theme) free(flt_stdrepl_theme);
    flt_stdrepl_theme = strdup(args[0]);
  }

  return 0;
}
/* }}} */

cf_conf_opt_t flt_stdreplacements_config[] = {
  { "ReplaceSmileys",   flt_stdreplacements_handle,  CF_CFG_OPT_CONFIG|CF_CFG_OPT_USER|CF_CFG_OPT_LOCAL,  NULL },
  { "SmileyTheme",      flt_stdreplacements_handle,  CF_CFG_OPT_CONFIG|CF_CFG_OPT_USER|CF_CFG_OPT_LOCAL,  NULL },

  { NULL, NULL, 0, NULL }
};

cf_handler_config_t flt_stdreplacements_handlers[] = {
  { INIT_HANDLER,        flt_stdreplacements_init },
  { 0, NULL }
};

cf_module_config_t flt_stdreplacements = {
  MODULE_MAGIC_COOKIE,
  flt_stdreplacements_config,
  flt_stdreplacements_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};


/* eof */
