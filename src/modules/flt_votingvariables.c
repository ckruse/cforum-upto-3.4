/**
 * \file flt_votingvariables.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
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
#include "config.h"
#include "defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <string.h>
#include <ctype.h>
#include <sys/types.h>

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

static struct {
  int activate;
  int show_votes;
} flt_vv_Config = { 0, 0 };

/* {{{ flt_votingvariables_execute_filter */
int flt_votingvariables_execute_filter(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_cl_thread *thread,t_cf_template *tpl) {
  u_char *UserName = cf_hash_get(GlobalValues,"UserName",8),*tmp,buff[512];
  u_char *forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  t_name_value *v = cfg_get_first_value(dc,forum_name,"VoteURL"),*cs = cfg_get_first_value(dc,forum_name,"ExternCharset");
  size_t len;

  if(flt_vv_Config.activate) {
    cf_tpl_setvalue(tpl,"votes",TPL_VARIABLE_STRING,"1",1);

    if(UserName) {
      cf_tpl_setvalue(tpl,"votes_link",TPL_VARIABLE_STRING,"1",1);

      tmp = cf_advanced_get_link(v->values[0],thread->tid,thread->threadmsg->mid,"a=good",6,&len);
      cf_set_variable(tpl,cs,"vote_link_good",tmp,len,1);

      tmp = cf_advanced_get_link(v->values[0],thread->tid,thread->threadmsg->mid,"a=bad",5,&len);
      cf_set_variable(tpl,cs,"vote_link_bad",tmp,len,1);
    }

    if(flt_vv_Config.show_votes) {
      cf_tpl_setvalue(tpl,"show_votes",TPL_VARIABLE_STRING,"1",1);

      len = snprintf(buff,512,"%lu",(unsigned long)thread->threadmsg->votes_good);
      cf_tpl_setvalue(tpl,"votes_good",TPL_VARIABLE_STRING,buff,len);

      len = snprintf(buff,512,"%lu",(unsigned long)thread->threadmsg->votes_bad);
      cf_tpl_setvalue(tpl,"votes_bad",TPL_VARIABLE_STRING,buff,len);
    }

    return FLT_OK;
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_votingvariables_handle */
int flt_votingvariables_handle(t_configfile *cfile,t_conf_opt *opt,const u_char *context,u_char **args,size_t argnum) {
  if(cf_strcmp(opt->name,"VotingActivate") == 0) flt_vv_Config.activate = cf_strcmp(args[0],"yes") == 0;
  else if(cf_strcmp(opt->name,"VotesShow") == 0) flt_vv_Config.show_votes = cf_strcmp(args[0],"yes") == 0;

  return 0;
}
/* }}} */

t_conf_opt flt_votingvariables_config[] = {
  { "VotingActivate", flt_votingvariables_handle, CFG_OPT_CONFIG, NULL },
  { "VotesShow",      flt_votingvariables_handle, CFG_OPT_CONFIG, NULL },
  { NULL, NULL, 0, NULL }
};

t_handler_config flt_votingvariables_handlers[] = {
  { POSTING_HANDLER,  flt_votingvariables_execute_filter },
  { 0, NULL }
};

t_module_config flt_votingvariables = {
  flt_votingvariables_config,
  flt_votingvariables_handlers,
  NULL,
  NULL,
  NULL,
  NULL
};

/* eof */
