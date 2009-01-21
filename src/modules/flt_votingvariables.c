/**
 * \file flt_votingvariables.c
 * \author Christian Kruse, <cjk@wwwtech.de>
 *
 * This plugin sets the voting variables
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
#include <inttypes.h>

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

static struct {
  int activate;
  int show_votes;
  int use_js;
} flt_vv_Config = { 0, 0, 0 };

static u_char *flt_vv_fn = NULL;

/* {{{ flt_votingvariables_execute_filter */
int flt_votingvariables_execute_filter(cf_hash_t *head,cf_configuration_t *dc,cf_configuration_t *vc,cl_thread_t *thread,cf_template_t *tpl) {
  u_char *UserName = cf_hash_get(GlobalValues,"UserName",8);

  if(flt_vv_Config.activate) {
    cf_tpl_setvalue(tpl,"votes",TPL_VARIABLE_INT,1);

    if(UserName) cf_tpl_setvalue(tpl,"votes_link",TPL_VARIABLE_INT,1);

    if(flt_vv_Config.show_votes) cf_tpl_setvalue(tpl,"show_votes",TPL_VARIABLE_INT,1);
    if(flt_vv_Config.use_js) cf_tpl_setvalue(tpl,"VotingUseJS",TPL_VARIABLE_INT,1);

    return FLT_OK;
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_votingvariables_setvar */
int flt_votingvariables_setvars(cf_hash_t *head,cf_configuration_t *dc,cf_configuration_t *vc,cl_thread_t *thread,message_t *msg,cf_tpl_variable_t *hash) {
  u_char *UserName = cf_hash_get(GlobalValues,"UserName",8),*tmp,buff[512];
  u_char *forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);

  cf_name_value_t *v = cf_cfg_get_first_value(dc,forum_name,"DF:VoteURL"),*cs = cf_cfg_get_first_value(dc,forum_name,"ExternCharset");

  size_t len;

  if(flt_vv_Config.activate) {
    len = snprintf(buff,512,"%"PRIu64,msg->mid);
    cf_tpl_hashvar_setvalue(hash,"mid",TPL_VARIABLE_STRING,buff,len);

    len = snprintf(buff,512,"%"PRIu64,thread->tid);
    cf_tpl_hashvar_setvalue(hash,"tid",TPL_VARIABLE_STRING,buff,len);

    if(UserName) {
      if(flt_vv_Config.use_js) cf_tpl_hashvar_setvalue(hash,"VotingUseJS",TPL_VARIABLE_INT,1);

      tmp = cf_advanced_get_link(v->values[0],thread->tid,msg->mid,NULL,1,&len,"a","good");
      cf_set_variable_hash(hash,cs,"vote_link_good",tmp,len,1);

      tmp = cf_advanced_get_link(v->values[0],thread->tid,msg->mid,NULL,1,&len,"a","bad");
      cf_set_variable_hash(hash,cs,"vote_link_bad",tmp,len,1);
    }

    if(flt_vv_Config.show_votes) {
      len = snprintf(buff,512,"%"PRIu32,msg->votes_good);
      cf_tpl_hashvar_setvalue(hash,"votes_good",TPL_VARIABLE_STRING,buff,len);

      len = snprintf(buff,512,"%"PRIu32,msg->votes_bad);
      cf_tpl_hashvar_setvalue(hash,"votes_bad",TPL_VARIABLE_STRING,buff,len);
    }

    return FLT_OK;
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_votingvariables_handle */
int flt_votingvariables_handle(cf_configfile_t *cfile,cf_conf_opt_t *opt,const u_char *context,u_char **args,size_t argnum) {
  if(!flt_vv_fn) flt_vv_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(flt_vv_fn,context) != 0) return 0;

  if(cf_strcmp(opt->name,"VotingActivate") == 0) flt_vv_Config.activate = cf_strcmp(args[0],"yes") == 0;
  else if(cf_strcmp(opt->name,"VotesShow") == 0) flt_vv_Config.show_votes = cf_strcmp(args[0],"yes") == 0;
  else if(cf_strcmp(opt->name,"VotingUseJS") == 0) flt_vv_Config.use_js = cf_strcmp(args[0],"yes") == 0;

  return 0;
}
/* }}} */

cf_conf_opt_t flt_votingvariables_config[] = {
  { "VotingActivate", flt_votingvariables_handle, CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, NULL },
  { "VotesShow",      flt_votingvariables_handle, CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, NULL },
  { "VotingUseJS",    flt_votingvariables_handle, CF_CFG_OPT_CONFIG|CF_CFG_OPT_USER|CF_CFG_OPT_LOCAL, NULL },
  { NULL, NULL, 0, NULL }
};

cf_handler_config_t flt_votingvariables_handlers[] = {
  { POSTING_HANDLER,     flt_votingvariables_execute_filter },
  { PERPOST_VAR_HANDLER, flt_votingvariables_setvars },
  { 0, NULL }
};

cf_module_config_t flt_votingvariables = {
  MODULE_MAGIC_COOKIE,
  flt_votingvariables_config,
  flt_votingvariables_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};

/* eof */
