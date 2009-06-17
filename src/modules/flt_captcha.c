/**
 *
 * \file flt_captcha.c
 * \author Christian Kruse, <cjk@wwwtech.de>
 *
 * This plugin implements a captcha
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

#include "fo_post.h"
/* }}} */



#define FLT_CAPTCHA_ENABLED 1
#define FLT_CAPTCHA_ENABLED_AUTH 2

#ifdef CF_SHARED_MEM
int flt_captcha_new_posting(cf_hash_t *head,cf_configuration_t *cfg,cf_message_t *p,cf_cl_thread_t *thr,void *ptr,int sock,int mode)
#else
int flt_captcha_new_posting(cf_hash_t *head,cf_configuration_t *cfg,cf_message_t *p,cf_cl_thread_t *thr,int sock,int mode)
#endif
{
  u_char *anum,*answer;
  size_t num;
  cf_cfg_config_value_t *enabled,*questions;
  u_char *UserName;
  int auth = 0;
  int enable = 0;

  if((enabled = cf_cfg_get_value(cfg,"Captcha:Enable")) == NULL) return FLT_DECLINE;

  enable = enabled->type == CF_ASM_ARG_NUM && enabled->ival == 0;
  auth = enabled->type == CF_ASM_ARG_STR && cf_strcmp(enabled->sval,"auth") == 0;

  if(enable == 0) return FLT_DECLINE;
  if((UserName = cf_hash_get(GlobalValues,"UserName",8)) != NULL && auth == 0) return FLT_DECLINE;

  if(head && (anum = cf_cgi_get(head,"captcha_num")) != NULL) {
    if((questions = cf_cfg_get_value(cfg,"Captcha:Questions")) != NULL) {
      num = atoi(anum);
      answer = cf_cgi_get(head,"captcha_answer");

      if(num >= 0 && num < questions->alen) {
        if(answer != NULL && cf_strcmp(answer,qestions->avals[num].avals[1].sval) == 0) return FLT_OK;
      }
    }
  }

  strcpy(ErrorString,"E_captcha");
  display_posting_form(head,p,NULL);
  return FLT_EXIT;
}

/* {{{ flt_captcha_post_display */
int flt_captcha_post_display(cf_hash_t *head,cf_configuration_t *cfg,cf_template_t *tpl,cf_message_t *p) {
  u_char *anum,*UserName;
  size_t num;
  flt_captcha_question_t *q;

  cf_cfg_config_value_t *enabled,*questions;

  int auth = 0;
  int enable = 0;

  if((enabled = cf_cfg_get_value(cfg,"Captcha:Enable")) == NULL) return FLT_DECLINE;

  if(enabled->type == CF_ASM_ARG_NUM && enabled->ival == 0) return FLT_DECLINE;
  if((UserName = cf_hash_get(GlobalValues,"UserName",8)) != NULL && enabled->type == CF_ASM_ARG_STR && cf_strcmp(enabled->sval,"auth") == 0) return FLT_DECLINE;

  if((questions = cf_cfg_get_value(cfg,"Captcha:Questions")) != NULL) {
    if(head && (anum = cf_cgi_get(head,"captcha_num")) != NULL) num = atoi(anum);
    else num = (size_t)(((float)questions->alen) * rand() / (RAND_MAX + 1.0));

    if(num >= questions->alen) num = questions->alen - 1;

    cf_tpl_setvalue(tpl,"captcha",TPL_VARIABLE_INT,(int)num);
    cf_tpl_setvalue(tpl,"captcha_question",TPL_VARIABLE_STRING,questions->avals[num].avals[0].sval,strlen(questions->avals[num].avals[0].sval));
    cf_tpl_setvalue(tpl,"captcha_answer",TPL_VARIABLE_STRING,question->avals[num].avals[1].sval,strlen(question->avals[num].avals[1].sval));
    return FLT_OK;
  }

  return FLT_DECLINE;
}
/* }}} */

int flt_captcha_posting(cf_hash_t *head,configuration_t *dc,configuration_t *vc,cl_thread_t *thread,cf_template_t *tpl) {
  return flt_captcha_post_display(head,dc,vc,tpl,NULL);
}

/**
 * Config options:
 * Captcha:Enable = Yes|No;
 * Captcha:Questions = (
 *   ("Question1","Answer1")
 *   ("Question2","Answer2")
 * );
*/

cf_handler_config_t flt_captcha_handlers[] = {
  { NEW_POST_HANDLER,      flt_captcha_new_posting },
  { POSTING_HANDLER,       flt_captcha_posting },
  { POST_DISPLAY_HANDLER,  flt_captcha_post_display },
  { 0, NULL }
};

cf_module_config_t flt_captcha = {
  MODULE_MAGIC_COOKIE,
  flt_captcha_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};


/* eof */
