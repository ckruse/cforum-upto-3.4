/**
 * \file flt_admin.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
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
/* }}} */

u_char **flt_admin_Admins = NULL;
static size_t flt_admin_AdminNum = 0;
static int my_errno = 0;
static int is_admin = -1;

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

  is_admin = 0;
  return 0;
}

#ifndef CF_SHARED_MEM
int flt_admin_gogogo(t_cf_hash *cgi,t_configuration *dc,t_configuration *vc,int sock) {
#else
int flt_admin_gogogo(t_cf_hash *cgi,t_configuration *dc,t_configuration *vc,void *ptr) {
  int sock;
#endif
  u_char *action = NULL,*tid,*mid,buff[512],*answer;
  size_t len;
  rline_t rl;
  int x;
  u_char *UserName = cf_hash_get(GlobalValues,"UserName",8);

  if(!flt_admin_is_admin(UserName)) return FLT_DECLINE;

  if(cgi) action = cf_cgi_get(cgi,"faa");
  if(action) {
    tid = cf_cgi_get(cgi,"t");
    mid = cf_cgi_get(cgi,"m");

    if(!tid || !mid) return FLT_DECLINE;
    if(strtol(tid,NULL,10) == 0 || strtol(mid,NULL,10) == 0) return FLT_DECLINE;

    memset(&rl,0,sizeof(rline_t));

    #ifdef CF_SHARED_MEM
    /* if in shared memory mode, the sock parameter is a pointer to the shared mem segment */
    sock = set_us_up_the_socket();
    #endif

    if(cf_strcmp(action,"del") == 0) {
      len = snprintf(buff,512,"DELETE t=%s m=%s\nUser-Name: %s\n",tid,mid,UserName);
      writen(sock,buff,len);
    }
    else if(cf_strcmp(action,"undel") == 0) {
      len = snprintf(buff,512,"UNDELETE t=%s m=%s\nUser-Name: %s\n",tid,mid,UserName);
      writen(sock,buff,len);
    }
    else if(cf_strcmp(action,"archive") == 0) {
      len = snprintf(buff,512,"ARCHIVE THREAD t=%s\nUser-Name: %s\n",tid,UserName);
      writen(sock,buff,len);
    }

    answer = readline(sock,&rl);
    if(!answer || ((x = atoi(answer)) != 200)) {
      if(!answer) my_errno = 500;
      else my_errno = x;
    }
    if(answer) free(answer);

    #ifdef CF_SHARED_MEM
    writen(sock,"QUIT\n",5);
    close(sock);
    reget_shm_ptr();
    #endif

    cf_hash_entry_delete(cgi,"t",1);
    cf_hash_entry_delete(cgi,"m",1);

    return FLT_OK;
  }

  return FLT_DECLINE;
}

int flt_admin_setvars(t_cf_hash *cgi,t_configuration *dc,t_configuration *vc,t_cf_template *top,t_cf_template *down) {
  u_char *msg,buff[256];
  size_t len,len1;
  u_char *UserName = cf_hash_get(GlobalValues,"UserName",8);
  int ShowInvisible = cf_hash_get(GlobalValues,"ShowInvisible",13) != NULL;

  if(flt_admin_is_admin(UserName)) {
    tpl_cf_setvar(top,"admin","1",1,0);

    if(ShowInvisible) {
      tpl_cf_setvar(top,"aaf","1",1,0);
    }

    if(my_errno) {
      len = snprintf(buff,256,"E_FO_%d",my_errno);
      msg = get_error_message(buff,len,&len1);
      if(msg) {
        tpl_cf_setvar(top,"flt_admin_errmsg",msg+len+1,len1-len-1,1);
        free(msg);
      }
    }

    return FLT_OK;
  }

  return FLT_DECLINE;
}

int flt_admin_init(t_cf_hash *cgi,t_configuration *dc,t_configuration *vc) {
  t_name_value *v = cfg_get_first_value(dc,"Administrators");
  u_char *val = NULL;
  u_char *UserName = cf_hash_get(GlobalValues,"UserName",8);

  cf_register_mod_api_ent("flt_admin","is_admin",(t_mod_api)flt_admin_is_admin);

  if(!UserName) return FLT_DECLINE;
  if(!v)        return FLT_DECLINE;

  flt_admin_AdminNum = split(v->values[0],",",&flt_admin_Admins);

  if(!cgi)      return FLT_DECLINE;

  val = cf_cgi_get(cgi,"aaf");

  if(!val)      return FLT_DECLINE;

  if(!v) return FLT_DECLINE;

  /* ShowInvisible is imported from the client library */
  if(flt_admin_is_admin(UserName) && *val == '1') cf_hash_set(GlobalValues,"ShowInvisible",13,"1",1);

  return FLT_OK;
}

int flt_admin_posthandler(t_cf_hash *cgi,t_configuration *dc,t_configuration *vc,t_message *msg,u_int64_t tid,int mode) {
  u_char buff[256];
  size_t l;
  u_char *UserName = cf_hash_get(GlobalValues,"UserName",8);
  int ShowInvisible = cf_hash_get(GlobalValues,"ShowInvisible",13) != NULL;

  if(flt_admin_is_admin(UserName)) {
    tpl_cf_setvar(&msg->tpl,"admin","1",1,0);

    if(ShowInvisible) {
      tpl_cf_setvar(&msg->tpl,"aaf","1",1,0);

      l = snprintf(buff,256,"?t=%lld&m=%lld&aaf=1&faa=archive",tid,msg->mid);
      tpl_cf_setvar(&msg->tpl,"archive_link",buff,l,1);

      if(msg->invisible == 0) {
        l = snprintf(buff,256,"?t=%lld&m=%lld&aaf=1&faa=del",tid,msg->mid);
        tpl_cf_setvar(&msg->tpl,"visible","1",1,0);
        tpl_cf_setvar(&msg->tpl,"del_link",buff,l,1);
      }
      else {
        l = snprintf(buff,256,"?t=%lld&m=%lld&aaf=1&faa=undel",tid,msg->mid);
        tpl_cf_setvar(&msg->tpl,"undel_link",buff,l,1);
      }
    }

    return FLT_OK;
  }

  return FLT_DECLINE;
}

void flt_admin_finish(void) {
  size_t i;
  if(flt_admin_Admins) {
    for(i=0;i<flt_admin_AdminNum;i++) free(flt_admin_Admins[i]);
    free(flt_admin_Admins);
  }
}

#ifndef CF_SHARED_MEM
int flt_admin_validator(t_cf_hash *head,t_configuration *dc,t_configuration *vc,time_t last_modified,int sock) {
#else
int flt_admin_validator(t_cf_hash *head,t_configuration *dc,t_configuration *vc,time_t last_modified,void *sock) {
#endif
  u_char *UserName = cf_hash_get(GlobalValues,"UserName",8);
  int ShowInvisible = cf_hash_get(GlobalValues,"ShowInvisible",13) != NULL;

  if(flt_admin_is_admin(UserName) && ShowInvisible) {
    printf("X-I-am-it: flt_admin\015\012");
    return FLT_EXIT;
  }
  return FLT_DECLINE;
}

#ifndef CF_SHARED_MEM
time_t flt_admin_lm(t_cf_hash *head,t_configuration *dc,t_configuration *vc,int sock) {
#else
time_t flt_admin_lm(t_cf_hash *head,t_configuration *dc,t_configuration *vc,void *sock) {
#endif
  u_char *UserName = cf_hash_get(GlobalValues,"UserName",8);
  int ShowInvisible = cf_hash_get(GlobalValues,"ShowInvisible",13) != NULL;

  if(flt_admin_is_admin(UserName) && ShowInvisible) time(NULL);
  return 0;
}

t_conf_opt flt_admin_config[] = {
  { NULL, NULL, NULL }
};

t_handler_config flt_admin_handlers[] = {
  { CONNECT_INIT_HANDLER, flt_admin_gogogo },
  { INIT_HANDLER,         flt_admin_init },
  { VIEW_INIT_HANDLER,    flt_admin_setvars },
  { VIEW_LIST_HANDLER,    flt_admin_posthandler },
  { 0, NULL }
};

t_module_config flt_admin = {
  flt_admin_config,
  flt_admin_handlers,
  flt_admin_validator,
  flt_admin_lm,
  NULL,
  flt_admin_finish
};

/* eof */
