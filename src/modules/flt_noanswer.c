/**
 * \file flt_noanswer.c
 * \author Christian Kruse, <cjk@wwwtech.de>
 *
 * This plugin handles no-answer requests
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
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>

#include "hashlib.h"
#include "utils.h"
#include "cfgcomp.h"
#include "cfcgi.h"
#include "template.h"
#include "readline.h"
#include "clientlib.h"
/* }}} */

static int flt_na_204 = 0;

static int flt_na_errno = 0;
static u_char *flt_na_fn = NULL;

/* {{{ flt_noanswer_gogogo */
#ifndef CF_SHARED_MEM
int flt_noanswer_gogogo(cf_hash_t *cgi,cf_configuration_t *dc,cf_configuration_t *vc,int sock)
#else
int flt_noanswer_gogogo(cf_hash_t *cgi,cf_configuration_t *dc,cf_configuration_t *vc,void *ptr)
#endif
{
  #ifdef CF_SHARED_MEM
  int sock;
  #endif

  size_t len;
  rline_t rl;
  u_char buff[512],*uname,*fn,*answer;
  int si = cf_hash_get(GlobalValues,"ShowInvisible",13) != NULL,x = 0;
  cf_string_t *action = NULL,*tid,*mid,*mode,str;
  u_int64_t itid,imid;

  if(si == 0 || cgi == NULL) return FLT_DECLINE;

  if((action = cf_cgi_get(cgi,"a")) != NULL && (cf_strcmp(action->content,"set-na") == 0 || cf_strcmp(action->content,"remove-na") == 0)) {
    uname = cf_hash_get(GlobalValues,"UserName",8);
    fn    = cf_hash_get(GlobalValues,"FORUM_NAME",10);

    tid = cf_cgi_get(cgi,"t");
    mid = cf_cgi_get(cgi,"m");

    if(!tid || !mid) return FLT_DECLINE;
    if((itid = cf_str_to_uint64(tid->content)) == 0 || (imid = cf_str_to_uint64(mid->content)) == 0) return FLT_DECLINE;

    memset(&rl,0,sizeof(rl));

    #ifdef CF_SHARED_MEM
    /* if in shared memory mode, the sock parameter is a pointer to the shared mem segment */
    sock = cf_socket_setup();
    #endif

    len = snprintf(buff,512,"SELECT %s\n",fn);
    writen(sock,buff,len);

    answer = readline(sock,&rl);
    if(!answer || (x = atoi(answer)) != 200) {
      if(!answer) flt_na_errno = 500;
      else flt_na_errno = x;
    }
    if(answer) free(answer);

    if(x == 0 || x == 200) {
      cf_str_init_growth(&str,256);

      if(cf_strcmp(action->content,"set-na") == 0)         cf_str_char_set(&str,"FLAG SET ",9);
      else if(cf_strcmp(action->content,"remove-na") == 0) cf_str_char_set(&str,"FLAG REMOVE ",12);

      cf_str_char_append(&str,'t');
      cf_uint64_to_str(&str,itid);
      cf_str_chars_append(&str," m",2);
      cf_uint64_to_str(&str,imid);
      cf_str_chars_append(&str,"\n Flag: no-answer",16);
      if(cf_strcmp(action->content,"set-na") == 0) cf_str_chars_append(&str,"=yes",4);
      cf_str_chars_append(&str,"\n\n",2);

      writen(sock,str.content,str.len);

      cf_str_cleanup(&str);

      answer = readline(sock,&rl);
      if(!answer || ((x = atoi(answer)) != 200)) {
        if(!answer) flt_na_errno = 500;
        else flt_na_errno = x;
      }
      if(answer) free(answer);
    }

    #ifdef CF_SHARED_MEM
    writen(sock,"QUIT\n",5);
    close(sock);
    cf_reget_shm_ptr();
    #endif

    if((mode = cf_cgi_get(cgi,"mode")) == NULL || cf_strcmp(mode->content,"xmlhttp") != 0) {
      cf_hash_entry_delete(cgi,"t",1);
      cf_hash_entry_delete(cgi,"m",1);

      if(flt_na_204) {
        printf("Status: 204 No Content\015\012\015\012");
        return FLT_EXIT;
      }
    }

    return FLT_OK;
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_noanswer_posthandler */
int flt_noanswer_posthandler(cf_hash_t *cgi,cf_configuration_t *dc,cf_configuration_t *vc,message_t *msg,u_int64_t tid,int mode) {
  int si = cf_hash_get(GlobalValues,"ShowInvisible",13) != NULL;
  cf_readmode_t *rm = cf_hash_get(GlobalValues,"RM",2);
  cf_post_flag_t *flag;
  u_char *link;
  size_t l;

  if((flag = cf_flag_by_name(&msg->flags,"no-answer")) != NULL) cf_tpl_hashvar_setvalue(&msg->hashvar,"na",TPL_VARIABLE_INT,1);

  if(si) {
    if(flag) {
      link = cf_advanced_get_link(rm->posting_uri[1],tid,msg->mid,NULL,1,&l,"a","remove-na");
      cf_tpl_hashvar_setvalue(&msg->hashvar,"removena_link",TPL_VARIABLE_STRING,link,l);
      free(link);
    }
    else {
      link = cf_advanced_get_link(rm->posting_uri[1],tid,msg->mid,NULL,1,&l,"a","set-na");
      cf_tpl_hashvar_setvalue(&msg->hashvar,"setna_link",TPL_VARIABLE_STRING,link,l);
      free(link);
    }
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_noanswer_setvars */
int flt_noanswer_setvars(cf_hash_t *head,cf_configuration_t *dc,cf_configuration_t *vc,cl_thread_t *thread,message_t *msg,cf_tpl_variable_t *hash) {
  int si = cf_hash_get(GlobalValues,"ShowInvisible",13) != NULL;
  cf_post_flag_t *flag;

  if((flag = cf_flag_by_name(&msg->flags,"no-answer")) != NULL && si == 0) cf_tpl_hashvar_setvalue(hash,"na",TPL_VARIABLE_INT,1);

  return FLT_OK;
}
/* }}} */

/* {{{ flt_noanswer_post */
#ifdef CF_SHARED_MEM
int flt_noanswer_post(cf_hash_t *head,cf_configuration_t *dc,cf_configuration_t *pc,message_t *p,cl_thread_t *thr,void *ptr,int sock,int mode)
#else
int flt_noanswer_post(cf_hash_t *head,cf_configuration_t *dc,cf_configuration_t *pc,message_t *p,cl_thread_t *thr,int sock,int mode)
#endif
{
  cf_name_value_t *cs;
  u_char *fn;

  if(!thr) return FLT_DECLINE;
  if(cf_flag_by_name(&thr->threadmsg->flags,"no-answer") == NULL) return FLT_DECLINE;
  if(cf_hash_get(GlobalValues,"ShowInvisible",13) != NULL) return FLT_DECLINE;

  fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  cs = cf_cfg_get_first_value(dc,fn,"DF:ExternCharset");
  printf("Status: 403 Forbidden\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
  cf_error_message("E_noanswer",NULL);

  return FLT_EXIT;
}
/* }}} */

/* {{{ flt_na_handle */
int flt_na_handle(cf_configfile_t *cfile,cf_conf_opt_t *opt,const u_char *context,u_char **args,size_t argnum) {
  if(flt_na_fn == NULL) flt_na_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(flt_na_fn,context) != 0) return 0;

  flt_na_204 = cf_strcmp(args[0],"yes") == 0;

  return 0;
}
/* }}} */

cf_conf_opt_t flt_noanswer_config[] = {
  { "NASend204", flt_na_handle, CF_CFG_OPT_USER|CF_CFG_OPT_LOCAL, NULL },
  { NULL, NULL, 0, NULL }
};

cf_handler_config_t flt_noanswer_handlers[] = {
  { CONNECT_INIT_HANDLER, flt_noanswer_gogogo },
  { VIEW_LIST_HANDLER,    flt_noanswer_posthandler },
  { PERPOST_VAR_HANDLER,  flt_noanswer_setvars },
  { NEW_POST_HANDLER,     flt_noanswer_post },
  { 0, NULL }
};

cf_module_config_t flt_noanswer = {
  MODULE_MAGIC_COOKIE,
  flt_noanswer_config,
  flt_noanswer_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};

/* eof */
