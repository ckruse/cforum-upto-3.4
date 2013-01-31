/**
 * \file flt_noanswer.c
 * \author Christian Kruse, <cjk@wwwtech.de>
 *
 * This plugin handles no-answer requests
 */

/* {{{ Initial comments */
/*
 * $LastChangedDate: 2009-01-16 14:32:24 +0100 (Fri, 16 Jan 2009) $
 * $LastChangedRevision: 1639 $
 * $LastChangedBy: ckruse $
 *
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

#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
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
int flt_noanswer_gogogo(cf_hash_t *cgi,configuration_t *dc,configuration_t *vc,int sock)
#else
int flt_noanswer_gogogo(cf_hash_t *cgi,configuration_t *dc,configuration_t *vc,void *ptr)
#endif
{
  #ifdef CF_SHARED_MEM
  int sock;
  #endif

  size_t len;
  rline_t rl;
  u_char *action = NULL,*tid,*mid,buff[512],*uname,*fn,*answer,*mode;
  int si = cf_hash_get(GlobalValues,"ShowInvisible",13) != NULL,x = 0;
  u_int64_t itid,imid;
  string_t str;

  if(si == 0 || cgi == NULL) return FLT_DECLINE;

  if((action = cf_cgi_get(cgi,"a")) != NULL && (cf_strcmp(action,"set-na") == 0 || cf_strcmp(action,"remove-na") == 0)) {
    uname = cf_hash_get(GlobalValues,"UserName",8);
    fn    = cf_hash_get(GlobalValues,"FORUM_NAME",10);

    tid = cf_cgi_get(cgi,"t");
    mid = cf_cgi_get(cgi,"m");

    if(!tid || !mid) return FLT_DECLINE;
    if((itid = str_to_u_int64(tid)) == 0 || (imid = str_to_u_int64(mid)) == 0) return FLT_DECLINE;

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
      str_init_growth(&str,256);

      if(cf_strcmp(action,"set-na") == 0)         str_char_set(&str,"FLAG SET ",9);
      else if(cf_strcmp(action,"remove-na") == 0) str_char_set(&str,"FLAG REMOVE ",12);

      str_char_append(&str,'t');
      u_int64_to_str(&str,itid);
      str_chars_append(&str," m",2);
      u_int64_to_str(&str,imid);
      if(cf_strcmp(action,"set-na") == 0)
        str_chars_append(&str,"\nFlag: no-answer=yes",20);
      else
        str_chars_append(&str,"\nFlags: no-answer",17);
      str_chars_append(&str,"\n\n",2);

      writen(sock,str.content,str.len);

      str_cleanup(&str);

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

    if((mode = cf_cgi_get(cgi,"mode")) == NULL || cf_strcmp(mode,"xmlhttp") != 0) {
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
int flt_noanswer_posthandler(cf_hash_t *cgi,configuration_t *dc,configuration_t *vc,message_t *msg,u_int64_t tid,int mode) {
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
int flt_noanswer_setvars(cf_hash_t *head,configuration_t *dc,configuration_t *vc,cl_thread_t *thread,message_t *msg,cf_tpl_variable_t *hash) {
  int si = cf_hash_get(GlobalValues,"ShowInvisible",13) != NULL;
  cf_post_flag_t *flag;

  if((flag = cf_flag_by_name(&msg->flags,"no-answer")) != NULL && si == 0) cf_tpl_hashvar_setvalue(hash,"na",TPL_VARIABLE_INT,1);

  return FLT_OK;
}
/* }}} */

/* {{{ flt_noanswer_post */
#ifdef CF_SHARED_MEM
int flt_noanswer_post(cf_hash_t *head,configuration_t *dc,configuration_t *pc,message_t *p,cl_thread_t *thr,void *ptr,int sock,int mode)
#else
int flt_noanswer_post(cf_hash_t *head,configuration_t *dc,configuration_t *pc,message_t *p,cl_thread_t *thr,int sock,int mode)
#endif
{
  name_value_t *cs;
  u_char *fn;

  if(!thr) return FLT_DECLINE;
  if(cf_flag_by_name(&thr->threadmsg->flags,"no-answer") == NULL) return FLT_DECLINE;
  if(cf_hash_get(GlobalValues,"ShowInvisible",13) != NULL) return FLT_DECLINE;

  fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  cs = cfg_get_first_value(dc,fn,"ExternCharset");
  printf("Status: 403 Forbidden\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
  cf_error_message("E_noanswer",NULL);

  return FLT_EXIT;
}
/* }}} */

/* {{{ flt_na_handle */
int flt_na_handle(configfile_t *cfile,conf_opt_t *opt,const u_char *context,u_char **args,size_t argnum) {
  if(flt_na_fn == NULL) flt_na_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(flt_na_fn,context) != 0) return 0;

  flt_na_204 = cf_strcmp(args[0],"yes") == 0;

  return 0;
}
/* }}} */

conf_opt_t flt_noanswer_config[] = {
  { "NASend204", flt_na_handle, CFG_OPT_USER|CFG_OPT_LOCAL, NULL },
  { NULL, NULL, 0, NULL }
};

handler_config_t flt_noanswer_handlers[] = {
  { CONNECT_INIT_HANDLER, flt_noanswer_gogogo },
  { VIEW_LIST_HANDLER,    flt_noanswer_posthandler },
  { PERPOST_VAR_HANDLER,  flt_noanswer_setvars },
  { NEW_POST_HANDLER,     flt_noanswer_post },
  { 0, NULL }
};

module_config_t flt_noanswer = {
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
