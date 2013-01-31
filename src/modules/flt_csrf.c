/**
 * \file flt_csrf.c
 * \author Christian Seiler, <webmaster@christian-seiler.de>
 *
 * This plug-in provides CSRF protection
 */

/* {{{ Initial comments */
/*
 * $LastChangedDate: 2009-02-10 10:41:42 +0100 (Tue, 10 Feb 2009) $
 * $LastChangedRevision: 1693 $
 * $LastChangedBy: anitsch $
 *
 * Basic idea: Usually, CSRF protection is done via a unique token in the
 * session that must match the user's token in order to accept non-idemponent
 * requests. However, we don't necessarily have a session, just HTTP auth.
 * Thus, we use a HMAC to encode the following information: Name of the user
 * and current timestamp divided through validity period. Thus, requests will
 * only be valid for a certain period of time (in order to avoid roll-over
 * issues, we will also accept the HMAC of the time period immediately before
 * the current one, but not more).
 *
 * This will only be done when the user is authenticated. If the user is not
 * authenticated, the only non-idemponent action he may take is to write a new
 * anonymous posting and that the attacker already can do - worst case, he could
 * spoof his IP address with CSRF, but there's no way of protecting from that,
 * other than requiring a session, which we don't want to do (and then the
 * login form will have similar problems).
 *
 * Due to the cryptographic properties of a HMAC, it could be considered adding
 * additional information (e.g. what thread the user wants to delete etc.) to
 * "message" in order to ensure that even if a specific token is leaked, it
 * could not be used for another action than the leaked one. However, this could
 * have a major performance impact and thus would have to be investigated.
 *
 * MD5 is considered broken by the cryptographic community as of today (Jan
 * 2009). It may still suffice for an HMAC and thus for our purposes, but we
 * will implement this with SHA-1 instead which is still considered reasonably
 * secure.
 *
 * We use OpenSSL in order to generate the HMAC. This ensures that we don't do
 * anything wrong regarding the cryptographic primitives.
 *
 * FIXME: This currently only works with fo_view and fo_post. Other scripts
 * have to be included too...
 */
/* }}} */

/* {{{ Includes */
#include "config.h"
#include "defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>

#include <openssl/evp.h>
#include <openssl/hmac.h>

#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "cfcgi.h"
#include "template.h"
#include "readline.h"
#include "clientlib.h"
#include "htmllib.h"
#include "fo_post.h"
/* }}} */

static long flt_csrf_validtime = 3600;
static u_char *flt_csrf_key = NULL;

static u_char *flt_csrf_fn = NULL;

static u_char *flt_csrf_currenttoken = NULL;

#define FLT_CSRF_CURRENTTOKEN(user)     (flt_csrf_currenttoken?flt_csrf_currenttoken:(flt_csrf_currenttoken = generate_csrf_key(user,0)))

/* {{{ generate_csrf_key */
static u_char *generate_csrf_key(u_char *user,int previous) {
  unsigned long current_time_section = time(NULL) / flt_csrf_validtime - (previous?1:0);

  ssize_t maxlen = strlen(user)+31;
  u_char *buf = fo_alloc(NULL,1,maxlen,FO_ALLOC_MALLOC);
  u_char *result_buf = fo_alloc(NULL,1,EVP_MAX_MD_SIZE,FO_ALLOC_MALLOC);

  int result_len = 0, i;

  if(snprintf(buf,maxlen,"%s%ld",user,current_time_section) >= maxlen) {
    free(buf);
    free(result_buf);
    return NULL;
  }

  if(HMAC(EVP_sha1(),flt_csrf_key,strlen(flt_csrf_key),buf,strlen(buf),result_buf,&result_len) == NULL) {
    free(buf);
    free(result_buf);
    return NULL;
  }

  // Hex encode the result
  free(buf);
  buf = fo_alloc(NULL,1,2*result_len+1,FO_ALLOC_MALLOC);
  for(i = 0; i < result_len; i++) snprintf(buf + 2*i, 3, "%02x", result_buf[i]); // sprintf automatically adds the final \0!

  free(result_buf);
  return buf;
}
/* }}} */

/* {{{ check_csrf_key */
static int check_csrf_key(u_char *user,char *csrf_token_user) {
  u_char *csrf_token_generated = generate_csrf_key(user,0);

  if(!csrf_token_generated) return -1;

  if(strcmp(csrf_token_generated,csrf_token_user) == 0) {
    free(csrf_token_generated);
    return 0;
  }

  free(csrf_token_generated);
  csrf_token_generated = generate_csrf_key(user,1);

  if(!csrf_token_generated) return -1;

  if(strcmp(csrf_token_generated,csrf_token_user) == 0) {
    free(csrf_token_generated);
    return 0;
  }

  free(csrf_token_generated);
  return 1;
}
/* }}} */

/* {{{ flt_csrf_newpost */
#ifdef CF_SHARED_MEM
int flt_csrf_newpost(cf_hash_t *head,configuration_t *dc,configuration_t *pc,message_t *p,cl_thread_t *thr,void *ptr,int sock,int mode)
#else
int flt_csrf_newpost(cf_hash_t *head,configuration_t *dc,configuration_t *pc,message_t *p,cl_thread_t *thr,int sock,int mode)
#endif
{
  u_char *UserName = cf_hash_get(GlobalValues,"UserName",8);
  u_char *csrf_token_user;
  int ret = 0;

  if(!UserName) return FLT_DECLINE;

  if(!head) {
    strcpy(ErrorString,"E_CSRF_VIOLATION");
    display_posting_form(head,p,NULL);
    return FLT_EXIT;
  }

  if((csrf_token_user = cf_cgi_get(head,"csrftoken")) == NULL) ret = 1;

  if(ret == 0) ret = check_csrf_key(UserName,csrf_token_user);

  if(ret == 1) {
    strcpy(ErrorString,"E_CSRF_VIOLATION");
    display_posting_form(head,p,NULL);
    return FLT_EXIT;
  }
  else if (ret == 0) return FLT_DECLINE;
  else {
    cf_error_message("E_CSRF_INTERNAL",NULL);
    return FLT_EXIT;
  }
}
/* }}} */

/* {{{ flt_csrf_posthandler */
int flt_csrf_posthandler(cf_hash_t *cgi,configuration_t *dc,configuration_t *vc,message_t *msg,u_int64_t tid,int mode) {
  u_char *UserName = cf_hash_get(GlobalValues,"UserName",8);
  u_char *csrf_token;

  if(!UserName) return FLT_DECLINE;

  csrf_token = FLT_CSRF_CURRENTTOKEN(UserName);
  if(!csrf_token) return FLT_DECLINE;

  cf_tpl_hashvar_setvalue(&msg->hashvar,"csrftoken",TPL_VARIABLE_STRING,csrf_token,strlen(csrf_token));
  return FLT_OK;
}
/* }}} */

/* {{{ flt_csrf_setvars */
int flt_csrf_setvars(cf_hash_t *cgi,configuration_t *dc,configuration_t *vc,cf_template_t *top,cf_template_t *down) {
  u_char *UserName = cf_hash_get(GlobalValues,"UserName",8);
  u_char *csrf_token;

  if(!UserName) return FLT_DECLINE;

  csrf_token = FLT_CSRF_CURRENTTOKEN(UserName);
  if(!csrf_token) return FLT_DECLINE;

  cf_tpl_setvalue(top,"csrftoken",TPL_VARIABLE_STRING,csrf_token,strlen(csrf_token));
  cf_tpl_setvalue(down,"csrftoken",TPL_VARIABLE_STRING,csrf_token,strlen(csrf_token));

  return FLT_OK;
}
/* }}} */

/* {{{ flt_csrf_setvars_thread */
int flt_csrf_setvars_thread(cf_hash_t *head,configuration_t *dc,configuration_t *vc,cl_thread_t *thread,message_t *msg,cf_tpl_variable_t *hash) {
  u_char *UserName = cf_hash_get(GlobalValues,"UserName",8);
  u_char *csrf_token;

  if(!UserName) return FLT_DECLINE;

  csrf_token = FLT_CSRF_CURRENTTOKEN(UserName);
  if(!csrf_token) return FLT_DECLINE;

  cf_tpl_hashvar_setvalue(hash,"csrftoken",TPL_VARIABLE_STRING,csrf_token,strlen(csrf_token));
  return FLT_OK;
}
/* }}} */

/* {{{ flt_csrf_posting_setvars */
int flt_csrf_posting_setvars(cf_hash_t *head,configuration_t *dc,configuration_t *vc,cl_thread_t *thread,cf_template_t *tpl) {
  u_char *UserName = cf_hash_get(GlobalValues,"UserName",8);
  u_char *csrf_token;

  if(!UserName) return FLT_DECLINE;

  csrf_token = FLT_CSRF_CURRENTTOKEN(UserName);
  if(!csrf_token) return FLT_DECLINE;

  cf_tpl_setvalue(tpl,"csrftoken",TPL_VARIABLE_STRING,csrf_token,strlen(csrf_token));
  return FLT_OK;
}
/* }}} */

/* {{{ flt_csrf_post_display */
int flt_csrf_post_display(cf_hash_t *head,configuration_t *dc,configuration_t *pc,cf_template_t *tpl,message_t *p) {
  u_char *UserName = cf_hash_get(GlobalValues,"UserName",8);
  u_char *csrf_token;

  if(!UserName) return FLT_DECLINE;

  csrf_token = FLT_CSRF_CURRENTTOKEN(UserName);
  if(!csrf_token) return FLT_DECLINE;

  cf_tpl_setvalue(tpl,"csrftoken",TPL_VARIABLE_STRING,csrf_token,strlen(csrf_token));
  return FLT_OK;
}
/* }}} */

/* {{{ flt_csrf_init */
int flt_csrf_init(cf_hash_t *cgi,configuration_t *dc,configuration_t *vc) {
  u_char *forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  u_char *UserName = cf_hash_get(GlobalValues,"UserName",8);
  u_char *csrf_token_user;
  name_value_t *cs = cfg_get_first_value(&fo_default_conf,forum_name,"ExternCharset");
  u_char *content_type = cf_hash_get(GlobalValues,"OutputContentType",17);
  u_char *value;
  int res;

  if(!UserName) return FLT_DECLINE;

  if(!cgi) return FLT_DECLINE;

  // We react to the following
  //   ?faa=... -> admin action
  //   ?a=...   -> hide
  //   ?mv=...  -> mark as visited
  //   ?mav=... -> mark all visited
  if(cf_cgi_get(cgi,"faa") != NULL || ((value = (u_char *)cf_cgi_get(cgi,"a")) != NULL && strcmp(value,"nd") != 0 && strcmp(value,"answer") != 0) || cf_cgi_get(cgi,"mv") != NULL || cf_cgi_get(cgi,"mav") != NULL) {
    csrf_token_user = cf_cgi_get(cgi,"csrftoken");
    if(!csrf_token_user) {
      printf("Status: 500 Internal Server Error\015\012Content-Type: %s; charset=%s\015\012\015\012",content_type?content_type:(u_char *)"text/html",cs->values[0]);
      cf_error_message("E_CSRF_VIOLATION",NULL);
      return FLT_EXIT;
    }
    if((res = check_csrf_key(UserName,csrf_token_user)) == 1) {
      printf("Status: 500 Internal Server Error\015\012Content-Type: %s; charset=%s\015\012\015\012",content_type?content_type:(u_char *)"text/html",cs->values[0]);
      cf_error_message("E_CSRF_VIOLATION",NULL);
      return FLT_EXIT;
    }
    else if(res == 0) return FLT_DECLINE;

    printf("Status: 500 Internal Server Error\015\012Content-Type: %s; charset=%s\015\012\015\012",content_type?content_type:(u_char *)"text/html",cs->values[0]);
    cf_error_message("E_CSRF_INTERNAL",NULL);
    return FLT_EXIT;
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_csrf_handle */
int flt_csrf_handle(configfile_t *cfile,conf_opt_t *opt,const u_char *context,u_char **args,size_t argnum) {
  u_char *ptr;

  if(flt_csrf_fn == NULL) flt_csrf_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(flt_csrf_fn,context) != 0) return 0;

  if(cf_strcmp(opt->name,"CSRFValidTime") == 0) {
    flt_csrf_validtime = strtol(args[0],(char **)&ptr,10);
  }
  else if(cf_strcmp(opt->name,"CSRFKey") == 0) {
    if(flt_csrf_key) free(flt_csrf_key);
    flt_csrf_key = strdup(args[0]);
  }

  return 0;
}
/* }}} */

conf_opt_t flt_csrf_config[] = {
  { "CSRFValidTime", flt_csrf_handle, CFG_OPT_CONFIG|CFG_OPT_LOCAL|CFG_OPT_NEEDED, NULL },
  { "CSRFKey",       flt_csrf_handle, CFG_OPT_CONFIG|CFG_OPT_LOCAL|CFG_OPT_NEEDED, NULL },
  { NULL, NULL, 0, NULL }
};

handler_config_t flt_csrf_handlers[] = {
  { INIT_HANDLER,         flt_csrf_init },
  { NEW_POST_HANDLER,     flt_csrf_newpost },
  { VIEW_INIT_HANDLER,    flt_csrf_setvars },
  { VIEW_LIST_HANDLER,    flt_csrf_posthandler },
  { POST_DISPLAY_HANDLER, flt_csrf_post_display },
  { POSTING_HANDLER,      flt_csrf_posting_setvars },
  { PERPOST_VAR_HANDLER,  flt_csrf_setvars_thread },
  { 0, NULL }
};

module_config_t flt_csrf = {
  MODULE_MAGIC_COOKIE,
  flt_csrf_config,
  flt_csrf_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};

/* eof */
