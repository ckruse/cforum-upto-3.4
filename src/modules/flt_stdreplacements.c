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

/* {{{ smileys */
static const u_char *flt_stdrepl_smiles[] = {
    ":)",":-)",":o)",

    /* ironic */
    ";)",";-)",

    /* unhappy */
    ":(",":-(",

    /* tounge grin */
    ":-P",":P",":-p",":p",
    
    /* broad grinning */
    ":D",":-D",

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
    "&gt;:-)","&gt;:)","}:-&gt;","}:&gt;",":-&gt;", "&gt;;-)",

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

    NULL
};
/* }}} */

/* {{{ smiley replacements */
static u_char *flt_stdrepl_smiley_replacements[] = {
    "smile","smile","smile",

    /* ironic */
    "ironic","ironic",

    /* unhappy */
    "unhappy","unhappy",

    /* tounge grin */
    "tongue","tongue","tongue","tongue",
    
    /* broad grinning */
    "biggrin","biggrin",

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
    "devil","devil","devil","devil","devil","devil",

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

    NULL
};
/* }}} */


/* {{{ flt_stdreplacements_smileys */
int flt_stdreplacements_smileys(t_configuration *fdc,t_configuration *fvc,t_cl_thread *thread,const u_char *directive,const u_char **parameters,size_t plen,t_string *bco,t_string *bci,t_string *content,t_string *cite,const u_char *qchars,int sig) {
  int i;

  if(!flt_stdrepl_theme) return FLT_DECLINE;

  for(i=0;flt_stdrepl_smiles[i];++i) {
    if(cf_strcmp(directive,flt_stdrepl_smiles[i]) == 0) {
      str_chars_append(content,"<img src=\"",10);
      str_chars_append(content,flt_stdrepl_theme,strlen(flt_stdrepl_theme));
      str_chars_append(content,flt_stdrepl_smiley_replacements[i],strlen(flt_stdrepl_smiley_replacements[i]));
      str_chars_append(content,".png\" alt=\"",11);
      str_chars_append(content,flt_stdrepl_smiley_replacements[i],strlen(flt_stdrepl_smiley_replacements[i]));
      str_chars_append(content,"\">",2);

      if(cite) str_chars_append(cite,directive,strlen(directive));
      return FLT_OK;
    }
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_stdreplacements_filter */
int flt_stdreplacements_filter(t_configuration *fdc,t_configuration *fvc,t_cl_thread *thread,const u_char *directive,const u_char **parameters,size_t plen,t_string *bco,t_string *bci,t_string *content,t_string *cite,const u_char *qchars,int sig) {
  if(cf_strcmp(directive,"...") == 0) {
    str_chars_append(content,"&#8230;",7);
    if(cite) str_chars_append(cite,"...",3);
    return FLT_OK;
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_directives_init */
int flt_stdreplacements_init(t_cf_hash *cgi,t_configuration *dc,t_configuration *vc) {
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
int flt_stdreplacements_handle(t_configfile *cfile,t_conf_opt *opt,const u_char *context,u_char **args,size_t argnum) {
  if(cf_strcmp(opt->name,"ReplaceSmileys") == 0) flt_stdrepl_smileys = cf_strcmp(args[0],"yes") == 0;
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
