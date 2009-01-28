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

typedef struct {
  u_char *question;
  u_char *answer;
} flt_captcha_question_t;

static u_char *flt_captcha_fn = NULL;
static int flt_captcha_enabled = 0;
static array_t flt_captcha_questions;
static int flt_captcha_must_init = 1;

#ifdef CF_SHARED_MEM
int flt_captcha_new_posting(cf_hash_t *head,configuration_t *dc,configuration_t *pc,message_t *p,cl_thread_t *thr,void *ptr,int sock,int mode)
#else
int flt_captcha_new_posting(cf_hash_t *head,configuration_t *dc,configuration_t *pc,message_t *p,cl_thread_t *thr,int sock,int mode)
#endif
{
  u_char *anum,*answer;
  size_t num;
  flt_captcha_question_t *q;

  if(flt_captcha_must_init) return FLT_DECLINE;
  if((flt_captcha_enabled & FLT_CAPTCHA_ENABLED) == 0) return FLT_DECLINE;
  if(cf_hash_get(GlobalValues,"UserName",8) != NULL && (flt_captcha_enabled & FLT_CAPTCHA_ENABLED_AUTH) == 0) return FLT_DECLINE;

  if(head && (anum = cf_cgi_get(head,"captcha_num")) != NULL) {
    num = atoi(anum);
    answer = cf_cgi_get(head,"captcha_answer");
    q = array_element_at(&flt_captcha_questions,num);

    if(answer != NULL && q != NULL && cf_strcmp(answer,q->answer) == 0) return FLT_OK;
  }

  strcpy(ErrorString,"E_captcha");
  display_posting_form(head,p,NULL);
  return FLT_EXIT;
}

/* {{{ flt_captcha_post_display */
int flt_captcha_post_display(cf_hash_t *head,configuration_t *dc,configuration_t *pc,cf_template_t *tpl,message_t *p) {
  u_char *anum,*UserName;
  size_t num;
  flt_captcha_question_t *q;

  if(flt_captcha_must_init) return FLT_DECLINE;

  if(flt_captcha_enabled & FLT_CAPTCHA_ENABLED) {
    if(((UserName = cf_hash_get(GlobalValues,"UserName",8)) != NULL && flt_captcha_enabled & FLT_CAPTCHA_ENABLED_AUTH) || UserName == NULL) {
      if(head && (anum = cf_cgi_get(head,"captcha_num")) != NULL) {
        num = atoi(anum);
        if(num >= flt_captcha_questions.elements) num = flt_captcha_questions.elements - 1;
      }
      else {
        num = (size_t)(((float)flt_captcha_questions.elements) * rand() / (RAND_MAX + 1.0));
        if(num >= flt_captcha_questions.elements) num = flt_captcha_questions.elements - 1;
      }

      q = array_element_at(&flt_captcha_questions,num);
      cf_tpl_setvalue(tpl,"captcha",TPL_VARIABLE_INT,(int)num);
      cf_tpl_setvalue(tpl,"captcha_question",TPL_VARIABLE_STRING,q->question,strlen(q->question));
      cf_tpl_setvalue(tpl,"captcha_answer",TPL_VARIABLE_STRING,q->answer,strlen(q->answer));
    }
  }

  return FLT_DECLINE;
}
/* }}} */

int flt_captcha_posting(cf_hash_t *head,configuration_t *dc,configuration_t *vc,cl_thread_t *thread,cf_template_t *tpl) {
  return flt_captcha_post_display(head,dc,vc,tpl,NULL);
}

/* {{{ flt_captcha_destroy */
void flt_captcha_destroy(void *a) {
  flt_captcha_question_t *el = (flt_captcha_question_t *)a;
  free(el->question);
  free(el->answer);
}
/* }}} */

/* {{{ flt_captcha_handle_command */
int flt_captcha_handle_command(configfile_t *cfile,conf_opt_t *opt,const u_char *context,u_char **args,size_t argnum) {
  flt_captcha_question_t q;

  if(flt_captcha_fn == NULL) flt_captcha_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(flt_captcha_fn,context) != 0) return 0;

  if(flt_captcha_must_init == 1) {
    flt_captcha_must_init = 0;
    array_init(&flt_captcha_questions,sizeof(flt_captcha_question_t),flt_captcha_destroy);
  }

  if(cf_strcmp(opt->name,"CaptchaEnable") == 0) {
    if(cf_strcmp(args[0],"auth") == 0) flt_captcha_enabled = FLT_CAPTCHA_ENABLED|FLT_CAPTCHA_ENABLED_AUTH;
    else if(cf_strcmp(args[0],"yes") == 0) flt_captcha_enabled = FLT_CAPTCHA_ENABLED;
  }
  else if(cf_strcmp(opt->name,"CaptchaQuestion") == 0) {
    if(argnum != 2) {
      fprintf(stderr,"CaptchaQuestion requires two arguments\n");
      return 1;
    }

    q.question = args[0];
    q.answer = args[1];
    free(args);

    array_push(&flt_captcha_questions,&q);

    return -1;
  }

  return 0;
}
/* }}} */


conf_opt_t flt_captcha_config[] = {
  { "CaptchaEnable",   flt_captcha_handle_command, CFG_OPT_CONFIG|CFG_OPT_LOCAL, NULL},
  { "CaptchaQuestion", flt_captcha_handle_command, CFG_OPT_CONFIG|CFG_OPT_LOCAL, NULL},
  { NULL, NULL, 0, NULL }
};

handler_config_t flt_captcha_handlers[] = {
  { NEW_POST_HANDLER,      flt_captcha_new_posting },
  { POSTING_HANDLER,       flt_captcha_posting },
  { POST_DISPLAY_HANDLER,  flt_captcha_post_display },
  { 0, NULL }
};

module_config_t flt_captcha = {
  MODULE_MAGIC_COOKIE,
  flt_captcha_config,
  flt_captcha_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};


/* eof */
