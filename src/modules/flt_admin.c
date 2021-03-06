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
#include "htmllib.h"
/* }}} */

static int my_errno      = 0;
static int is_admin      = -1;
static cf_cfg_configuration_t *flt_admin_cfg = NULL;

/* {{{ flt_admin_is_admin */
int flt_admin_is_admin(const u_char *name) {
  size_t i;
  cf_cfg_config_value_t *admins;

  if(!name || is_admin != -1 || flt_admin_cfg == NULL || (admins = cf_cfg_get_value(flt_admin_cfg,"DF:Admins")) == NULL) return 0;

  for(i=0;i<admins->alen;++i) {
    if(cf_strcmp(admins->avals[i].sval,name) == 0) {
      is_admin = 1;
      return 1;
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
int flt_admin_gogogo(cf_hash_t *cgi,cf_configuration_t *cfg,int sock)
#else
int flt_admin_gogogo(cf_hash_t *cgi,cf_configuration_t *cfg,void *ptr)
#endif
{
  #ifdef CF_SHARED_MEM
  int sock,shmids[3];
  cf_cfg_config_value_t *sockpath,*shminf;
  #endif
  u_char buff[512],*answer;
  size_t len;
  rline_t rl;
  int x = 0;
  cf_string_t *action = NULL,*tid,*mid,*mode,str;
  cf_cfg_config_value_t *send204 = cf_cfg_get_value(cfg,"Admin:Send204");

  u_int64_t itid,imid;

  u_char *UserName = cf_hash_get(GlobalValues,"UserName",8);

  if(!flt_admin_is_admin(UserName)) return FLT_DECLINE;

  if(cgi) action = cf_cgi_get(cgi,"faa");
  if(action) {
    tid = cf_cgi_get(cgi,"t");
    mid = cf_cgi_get(cgi,"m");

    if(!tid || !mid) return FLT_DECLINE;
    if((itid = cf_str_to_uint64(tid->content)) == 0 || (imid = cf_str_to_uint64(mid->content)) == 0) return FLT_DECLINE;

    memset(&rl,0,sizeof(rl));

    #ifdef CF_SHARED_MEM
    /* if in shared memory mode, the sock parameter is a pointer to the shared mem segment */
    sockpath = cf_cfg_get_value(cfg,"DF:SocketName");
    if((sock = cf_socket_setup(sockpath->sval)) < 0) {
      fprintf(stderr,"Could not open socket: %s\n",strerror(errno));
      return FLT_DECLINE;
    }
    #endif

    len = snprintf(buff,512,"SELECT %s\n",forum_name);
    writen(sock,buff,len);

    answer = readline(sock,&rl);
    if(!answer || ((x = atoi(answer)) != 200)) {
      if(!answer) my_errno = 500;
      else my_errno = x;
    }
    if(answer) free(answer);

    cf_str_init_growth(&str,256);

    if(x == 0 || x == 200) {
      if(cf_strcmp(action->content,"del") == 0)          cf_str_char_set(&str,"DELETE ",7);
      else if(cf_strcmp(action->content,"undel") == 0)   cf_str_char_set(&str,"UNDELETE ",9);
      else if(cf_strcmp(action->content,"archive") == 0) cf_str_char_set(&str,"ARCHIVE THREAD ",15);

      cf_str_char_append(&str,'t');
      cf_uint64_to_str(&str,itid);
      cf_str_chars_append(&str," m",2);
      cf_uint64_to_str(&str,imid);
      cf_str_chars_append(&str,"\nUser-Name: ",11);
      cf_str_cstr_append(&str,UserName);
      cf_str_char_append(&str,'\n');

      writen(sock,str.content,str.len);

      cf_str_cleanup(&str);

      answer = readline(sock,&rl);
      if(!answer || ((x = atoi(answer)) != 200)) {
        if(!answer) my_errno = 500;
        else my_errno = x;
      }
      if(answer) free(answer);
    }

    #ifdef CF_SHARED_MEM
    shminf = cf_cfg_get_value(cfg,"DF:SharedMemIds");
    shmids[0] = shminf->avals[0].ival;
    shmids[1] = shminf->avals[1].ival;
    shmids[2] = shminf->avals[2].ival;
    ptr = cf_reget_shm_ptr(shmids);
    writen(sock,"QUIT\n",5);
    close(sock);
    #endif

    if((mode = cf_cgi_get(cgi,"mode")) == NULL || cf_strcmp(mode->content,"xmlhttp") != 0) {
      cf_hash_entry_delete(cgi,"t",1);
      cf_hash_entry_delete(cgi,"m",1);

      if(send204->ival) {
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
int flt_admin_setvars(cf_hash_t *cgi,cf_configuration_t *cfg,cf_template_t *top,cf_template_t *down) {
  u_char *msg,buff[256];
  size_t len,len1;
  u_char *UserName = cf_hash_get(GlobalValues,"UserName",8);
  int ShowInvisible = cf_hash_get(GlobalValues,"ShowInvisible",13) != NULL;
  cf_name_value_t *usejs = cf_cfg_get_first_value(cfg,"Admin:UseJS");

  if(flt_admin_is_admin(UserName)) {
    cf_tpl_setvalue(top,"admin",TPL_VARIABLE_INT,1);

    if(ShowInvisible) {
      cf_tpl_setvalue(top,"aaf",TPL_VARIABLE_INT,1);
      if(usejs && usejs->ival == 0) cf_tpl_setvalue(top,"AdminJS",TPL_VARIABLE_INT,1);
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
int flt_admin_posting_setvars(cf_hash_t *head,cf_configuration_t *cfg,cf_cl_thread_t *thread,cf_template_t *tpl) {
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
int flt_admin_setvars_thread(cf_hash_t *head,cf_configuration_t *cfg,cf_cl_thread_t *thread,cf_message_t *msg,cf_tpl_variable_t *hash) {
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
int flt_admin_init(cf_hash_t *cgi,cf_configuration_t *cfg) {
  cf_cfg_config_value_t *v = cf_cfg_get_value(cfg,"DF:Admins");
  cf_string_t *val = NULL;
  u_char *UserName = cf_hash_get(GlobalValues,"UserName",8);

  flt_admin_cfg = cfg;

  cf_register_mod_api_ent("flt_admin","is_admin",(mod_api_t)flt_admin_is_admin);

  if(!UserName || !cgi || (val = cf_cgi_get(cgi,"aaf")) == NULL) return FLT_DECLINE;

  /* ShowInvisible is imported from the client library */
  if(flt_admin_is_admin(UserName) && *val->content == '1') {
    cf_hash_set(GlobalValues,"ShowInvisible",13,"1",1);
    cf_add_static_uri_flag("aaf","1",0);
  }

  return FLT_OK;
}
/* }}} */

/* {{{ flt_admin_posthandler */
int flt_admin_posthandler(cf_hash_t *cgi,cf_configuration_t *cfg,cf_message_t *msg,u_int64_t tid,int mode) {
  u_char *link;
  size_t l;
  u_char *UserName = cf_hash_get(GlobalValues,"UserName",8);
  int ShowInvisible = cf_hash_get(GlobalValues,"ShowInvisible",13) != NULL;
  cf_readmode_t *rm = cf_hash_get(GlobalValues,"RM",2);
  cf_cfg_config_value_t *usejs = cf_cfg_get_value(cfg,"Admin:UseJS");

  if(!UserName) return FLT_DECLINE;

  if(flt_admin_is_admin(UserName) && ShowInvisible) {
    cf_tpl_hashvar_setvalue(&msg->hashvar,"admin",TPL_VARIABLE_INT,1);

    cf_tpl_hashvar_setvalue(&msg->hashvar,"aaf",TPL_VARIABLE_INT,1);
    if(usejs && usejs->ival && (mode & CF_MODE_THREADLIST)) cf_tpl_hashvar_setvalue(&msg->hashvar,"AdminJS",TPL_VARIABLE_INT,1);

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

/* {{{ flt_admin_validator */
#ifndef CF_SHARED_MEM
int flt_admin_validator(cf_hash_t *head,cf_configuration_t *dc,cf_configuration_t *vc,time_t last_modified,int sock)
#else
int flt_admin_validator(cf_hash_t *head,cf_configuration_t *dc,cf_configuration_t *vc,time_t last_modified,void *sock)
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
time_t flt_admin_lm(cf_hash_t *head,cf_configuration_t *dc,cf_configuration_t *vc,int sock)
#else
time_t flt_admin_lm(cf_hash_t *head,cf_configuration_t *dc,cf_configuration_t *vc,void *sock)
#endif
{
  u_char *UserName = cf_hash_get(GlobalValues,"UserName",8);
  int ShowInvisible = cf_hash_get(GlobalValues,"ShowInvisible",13) != NULL;

  if(flt_admin_is_admin(UserName) && ShowInvisible) time(NULL);
  return 0;
}
/* }}} */

/**
 * Config directives:
 * Admin:Send204 = (yes|no);
 */

cf_handler_config_t flt_admin_handlers[] = {
  { CONNECT_INIT_HANDLER, flt_admin_gogogo },
  { INIT_HANDLER,         flt_admin_init },
  { VIEW_INIT_HANDLER,    flt_admin_setvars },
  { VIEW_LIST_HANDLER,    flt_admin_posthandler },
  { POSTING_HANDLER,      flt_admin_posting_setvars },
  { PERPOST_VAR_HANDLER,  flt_admin_setvars_thread },
  { 0, NULL }
};

cf_module_config_t flt_admin = {
  MODULE_MAGIC_COOKIE,
  flt_admin_handlers,
  NULL,
  flt_admin_validator,
  flt_admin_lm,
  NULL,
  NULL
};

/* eof */
