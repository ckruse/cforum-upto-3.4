/**
 * \file flt_admin.c
 * \author Christian Kruse, <cjk@wwwtech.de>
 *
 * This plug-in provides administrator functions
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
#include "htmllib.h"
/* }}} */

u_char **flt_admin_Admins = NULL;
static size_t flt_admin_AdminNum = 0;
static int my_errno      = 0;
static int is_admin      = -1;
static int flt_admin_204 = 0;

static u_char *flt_admin_fn = NULL;

/* {{{ flt_admin_is_admin */
int flt_admin_is_admin(const u_char *name) {
  size_t i;

  if(!name) return 0;
  if(is_admin != -1) return is_admin;

  if(flt_admin_Admins) {
    for(i=0;i<flt_admin_AdminNum;i++) {
      if(cf_strcmp(flt_admin_Admins[i],name) == 0) {
        is_admin = 1;
        return 1;
      }
    }
  }

  if(cf_hash_get(GlobalValues,"is_admin",8) != NULL) {
    is_admin = 1;
    return 1;
  }

  is_admin = 0;
  return 0;
}
/* }}} */

/* {{{ flt_admin_gogogo */
#ifndef CF_SHARED_MEM
int flt_admin_gogogo(cf_hash_t *cgi,configuration_t *dc,configuration_t *vc,int sock)
#else
int flt_admin_gogogo(cf_hash_t *cgi,configuration_t *dc,configuration_t *vc,void *ptr)
#endif
{
  #ifdef CF_SHARED_MEM
  int sock;
  #endif
  u_char *action = NULL,*tid,*mid,buff[512],*answer,*mode;
  size_t len;
  rline_t rl;
  int x = 0;
  string_t str;

  u_int64_t itid,imid;

  u_char *UserName = cf_hash_get(GlobalValues,"UserName",8);
  u_char *forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);

  if(!flt_admin_is_admin(UserName)) return FLT_DECLINE;

  if(cgi) action = cf_cgi_get(cgi,"faa");
  if(action) {
    tid = cf_cgi_get(cgi,"t");
    mid = cf_cgi_get(cgi,"m");

    if(!tid || !mid) return FLT_DECLINE;
    if((itid = str_to_u_int64(tid)) == 0 || (imid = str_to_u_int64(mid)) == 0) return FLT_DECLINE;

    memset(&rl,0,sizeof(rl));

    #ifdef CF_SHARED_MEM
    /* if in shared memory mode, the sock parameter is a pointer to the shared mem segment */
    sock = cf_socket_setup();
    #endif

    len = snprintf(buff,512,"SELECT %s\n",forum_name);
    writen(sock,buff,len);

    answer = readline(sock,&rl);
    if(!answer || ((x = atoi(answer)) != 200)) {
      if(!answer) my_errno = 500;
      else my_errno = x;
    }
    if(answer) free(answer);

    str_init_growth(&str,256);

    if(x == 0 || x == 200) {
      if(cf_strcmp(action,"del") == 0)          str_char_set(&str,"DELETE ",7);
      else if(cf_strcmp(action,"undel") == 0)   str_char_set(&str,"UNDELETE ",9);
      else if(cf_strcmp(action,"archive") == 0) str_char_set(&str,"ARCHIVE THREAD ",15);

      str_char_append(&str,'t');
      u_int64_to_str(&str,itid);
      str_chars_append(&str," m",2);
      u_int64_to_str(&str,imid);
      str_chars_append(&str,"\nUser-Name: ",11);
      str_cstr_append(&str,UserName);
      str_char_append(&str,'\n');

      writen(sock,str.content,str.len);

      str_cleanup(&str);

      answer = readline(sock,&rl);
      if(!answer || ((x = atoi(answer)) != 200)) {
        if(!answer) my_errno = 500;
        else my_errno = x;
      }
      if(answer) free(answer);
    }

    #ifdef CF_SHARED_MEM
    ptr = cf_reget_shm_ptr();
    #endif

    itid = str_to_u_int64(tid);

    #ifdef CF_SHARED_MEM
    writen(sock,"QUIT\n",5);
    close(sock);
    #endif

    if((mode = cf_cgi_get(cgi,"mode")) == NULL || cf_strcmp(mode,"xmlhttp") != 0) {
      cf_hash_entry_delete(cgi,"t",1);
      cf_hash_entry_delete(cgi,"m",1);

      if(flt_admin_204) {
        printf("Status: 204 No Content\015\012\015\012");
        return FLT_EXIT;
      }
    }

    return FLT_OK;
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_admin_setvars */
int flt_admin_setvars(cf_hash_t *cgi,configuration_t *dc,configuration_t *vc,cf_template_t *top,cf_template_t *down) {
  u_char *msg,buff[256];
  size_t len,len1;
  u_char *UserName = cf_hash_get(GlobalValues,"UserName",8);
  int ShowInvisible = cf_hash_get(GlobalValues,"ShowInvisible",13) != NULL;
  u_char *fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  name_value_t *usejs = cfg_get_first_value(&fo_view_conf,fn,"AdminUseJS");

  if(flt_admin_is_admin(UserName)) {
    cf_tpl_setvalue(top,"admin",TPL_VARIABLE_INT,1);

    if(ShowInvisible) {
      cf_tpl_setvalue(top,"aaf",TPL_VARIABLE_INT,1);
      if(usejs && cf_strcmp(usejs->values[0],"yes") == 0) cf_tpl_setvalue(top,"AdminJS",TPL_VARIABLE_INT,1);
    }

    if(my_errno) {
      len = snprintf(buff,256,"E_FO_%d",my_errno);
      msg = cf_get_error_message(buff,&len1);
      if(msg) {
        cf_tpl_setvalue(top,"flt_admin_errmsg",TPL_VARIABLE_STRING,msg,len1);
        free(msg);
      }
    }

    return FLT_OK;
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_admin_posting_setvars */
int flt_admin_posting_setvars(cf_hash_t *head,configuration_t *dc,configuration_t *vc,cl_thread_t *thread,cf_template_t *tpl) {
  u_char *UserName = cf_hash_get(GlobalValues,"UserName",8);
  int ShowInvisible = cf_hash_get(GlobalValues,"ShowInvisible",13) != NULL;

  if(flt_admin_is_admin(UserName)) {
    cf_tpl_setvalue(tpl,"admin",TPL_VARIABLE_INT,1);

    if(ShowInvisible) cf_tpl_setvalue(tpl,"aaf",TPL_VARIABLE_INT,1);

    return FLT_OK;
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_admin_setvars_thread */
int flt_admin_setvars_thread(cf_hash_t *head,configuration_t *dc,configuration_t *vc,cl_thread_t *thread,message_t *msg,cf_tpl_variable_t *hash) {
  u_char *UserName = cf_hash_get(GlobalValues,"UserName",8);
  int si = cf_hash_get(GlobalValues,"ShowInvisible",13) != NULL;

  cf_post_flag_t *flag;

  if(flt_admin_is_admin(UserName) && si) {
    cf_tpl_hashvar_setvalue(hash,"ip",TPL_VARIABLE_STRING,msg->remote_addr.content,msg->remote_addr.len);
    if((flag = cf_flag_by_name(&msg->flags,"UserName")) != NULL) cf_tpl_hashvar_setvalue(hash,"uname",TPL_VARIABLE_STRING,flag->val,strlen(flag->val));
    return FLT_OK;
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_admin_init */
int flt_admin_init(cf_hash_t *cgi,configuration_t *dc,configuration_t *vc) {
  u_char *forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  name_value_t *v = cfg_get_first_value(dc,forum_name,"Administrators");
  u_char *val = NULL;
  u_char *UserName = cf_hash_get(GlobalValues,"UserName",8);

  cf_register_mod_api_ent("flt_admin","is_admin",(mod_api_t)flt_admin_is_admin);

  if(!UserName) return FLT_DECLINE;
  if(v) flt_admin_AdminNum = split(v->values[0],",",&flt_admin_Admins);

  if(!cgi)      return FLT_DECLINE;

  val = cf_cgi_get(cgi,"aaf");
  if(!val) return FLT_DECLINE;

  /* ShowInvisible is imported from the client library */
  if(flt_admin_is_admin(UserName) && *val == '1') {
    cf_hash_set(GlobalValues,"ShowInvisible",13,"1",1);
    cf_add_static_uri_flag("aaf","1",0);
  }

  return FLT_OK;
}
/* }}} */

/* {{{ flt_admin_posthandler */
int flt_admin_posthandler(cf_hash_t *cgi,configuration_t *dc,configuration_t *vc,message_t *msg,u_int64_t tid,int mode) {
  u_char *forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);  
  u_char *link;
  size_t l;
  u_char *UserName = cf_hash_get(GlobalValues,"UserName",8);
  int ShowInvisible = cf_hash_get(GlobalValues,"ShowInvisible",13) != NULL;
  cf_readmode_t *rm = cf_hash_get(GlobalValues,"RM",2);
  name_value_t *usejs = cfg_get_first_value(&fo_view_conf,forum_name,"AdminUseJS");

  if(!UserName) return FLT_DECLINE;

  if(flt_admin_is_admin(UserName) && ShowInvisible) {
    cf_tpl_hashvar_setvalue(&msg->hashvar,"admin",TPL_VARIABLE_INT,1);

    cf_tpl_hashvar_setvalue(&msg->hashvar,"aaf",TPL_VARIABLE_INT,1);
    if(usejs && cf_strcmp(usejs->values[0],"yes") == 0 && (mode & CF_MODE_THREADLIST)) cf_tpl_hashvar_setvalue(&msg->hashvar,"AdminJS",TPL_VARIABLE_INT,1);

    link = cf_advanced_get_link(rm->posting_uri[1],tid,msg->mid,NULL,1,&l,"faa","archive");
    cf_tpl_hashvar_setvalue(&msg->hashvar,"archive_link",TPL_VARIABLE_STRING,link,l);
    free(link);

    if(msg->invisible == 0) {
      link = cf_advanced_get_link(rm->posting_uri[1],tid,msg->mid,NULL,1,&l,"faa","del");
      cf_tpl_hashvar_setvalue(&msg->hashvar,"visible",TPL_VARIABLE_STRING,"1",1);
      cf_tpl_hashvar_setvalue(&msg->hashvar,"del_link",TPL_VARIABLE_STRING,link,l);
      free(link);
    }
    else {
      link = cf_advanced_get_link(rm->posting_uri[1],tid,msg->mid,NULL,1,&l,"faa","undel");
      cf_tpl_hashvar_setvalue(&msg->hashvar,"undel_link",TPL_VARIABLE_STRING,link,l);
      free(link);
    }

    return FLT_OK;
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_admin_finish */
void flt_admin_finish(void) {
  size_t i;
  if(flt_admin_Admins) {
    for(i=0;i<flt_admin_AdminNum;i++) free(flt_admin_Admins[i]);
    free(flt_admin_Admins);
  }
}
/* }}} */

/* {{{ flt_admin_validator */
#ifndef CF_SHARED_MEM
int flt_admin_validator(cf_hash_t *head,configuration_t *dc,configuration_t *vc,time_t last_modified,int sock)
#else
int flt_admin_validator(cf_hash_t *head,configuration_t *dc,configuration_t *vc,time_t last_modified,void *sock)
#endif
{
  u_char *UserName = cf_hash_get(GlobalValues,"UserName",8);
  int ShowInvisible = cf_hash_get(GlobalValues,"ShowInvisible",13) != NULL;

  if(flt_admin_is_admin(UserName) && ShowInvisible) return FLT_EXIT;
  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_admin_lm */
#ifndef CF_SHARED_MEM
time_t flt_admin_lm(cf_hash_t *head,configuration_t *dc,configuration_t *vc,int sock)
#else
time_t flt_admin_lm(cf_hash_t *head,configuration_t *dc,configuration_t *vc,void *sock)
#endif
{
  u_char *UserName = cf_hash_get(GlobalValues,"UserName",8);
  int ShowInvisible = cf_hash_get(GlobalValues,"ShowInvisible",13) != NULL;

  if(flt_admin_is_admin(UserName) && ShowInvisible) time(NULL);
  return 0;
}
/* }}} */

/* {{{ flt_admin_handle */
int flt_admin_handle(configfile_t *cfile,conf_opt_t *opt,const u_char *context,u_char **args,size_t argnum) {
  if(flt_admin_fn == NULL) flt_admin_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(flt_admin_fn,context) != 0) return 0;

  if(cf_strcmp(opt->name,"AdminSend204") == 0) flt_admin_204 = cf_strcmp(args[0],"yes") == 0;

  return 0;
}
/* }}} */

conf_opt_t flt_admin_config[] = {
  { "AdminSend204", flt_admin_handle, CFG_OPT_USER|CFG_OPT_LOCAL, NULL },
  { NULL, NULL, 0, NULL }
};

handler_config_t flt_admin_handlers[] = {
  { CONNECT_INIT_HANDLER, flt_admin_gogogo },
  { INIT_HANDLER,         flt_admin_init },
  { VIEW_INIT_HANDLER,    flt_admin_setvars },
  { VIEW_LIST_HANDLER,    flt_admin_posthandler },
  { POSTING_HANDLER,      flt_admin_posting_setvars },
  { PERPOST_VAR_HANDLER,  flt_admin_setvars_thread },
  { 0, NULL }
};

module_config_t flt_admin = {
  MODULE_MAGIC_COOKIE,
  flt_admin_config,
  flt_admin_handlers,
  NULL,
  flt_admin_validator,
  flt_admin_lm,
  NULL,
  flt_admin_finish
};

/* eof */
