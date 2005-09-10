/**
 * \file flt_noarchive.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 *
 * This plugin handles no-archive requests
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
#include <ctype.h>
#include <time.h>
#include <sys/types.h>

#include "cf_pthread.h"

#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "readline.h"

#include "cfcgi.h"
#include "template.h"
#include "clientlib.h"
/* }}} */

static int flt_noarchive_204 = 0;

static u_char *flt_noarchive_fn = NULL;
static int flt_noarchive_errno = 0;

/* {{{ flt_noarchive_gogogo */
#ifndef CF_SHARED_MEM
int flt_noarchive_gogogo(cf_hash_t *cgi,configuration_t *dc,configuration_t *vc,int sock)
#else
int flt_noarchive_gogogo(cf_hash_t *cgi,configuration_t *dc,configuration_t *vc,void *ptr)
#endif
{
  #ifdef CF_SHARED_MEM
  int sock;
  #endif

  size_t len;
  rline_t rl;
  u_char *action = NULL,*tid,*mid,buff[512],*fn,*answer,*mode;
  int si = cf_hash_get(GlobalValues,"ShowInvisible",13) != NULL,x = 0;

  if(si == 0 || cgi == NULL) return FLT_DECLINE;

  if((action = cf_cgi_get(cgi,"a")) != NULL && (cf_strcmp(action,"set-noarchive") == 0 || cf_strcmp(action,"remove-noarchive") == 0)) {
    fn    = cf_hash_get(GlobalValues,"FORUM_NAME",10);

    tid = cf_cgi_get(cgi,"t");
    mid = cf_cgi_get(cgi,"m");

    if(!tid || !mid) return FLT_DECLINE;
    if(str_to_u_int64(tid) == 0 || str_to_u_int64(mid) == 0) return FLT_DECLINE;

    memset(&rl,0,sizeof(rl));

    #ifdef CF_SHARED_MEM
    /* if in shared memory mode, the sock parameter is a pointer to the shared mem segment */
    sock = cf_socket_setup();
    #endif

    len = snprintf(buff,512,"SELECT %s\n",fn);
    writen(sock,buff,len);

    answer = readline(sock,&rl);
    if(!answer || (x = atoi(answer)) != 200) {
      if(!answer) flt_noarchive_errno = 500;
      else flt_noarchive_errno = x;
    }
    if(answer) free(answer);

    if(x == 0 || x == 200) {
      if(cf_strcmp(action,"set-noarchive") == 0) {
        len = snprintf(buff,512,"FLAG SET t%s m%s\nFlag: no-archive=yes\n\n",tid,mid);
        writen(sock,buff,len);
      }
      else if(cf_strcmp(action,"remove-noarchive") == 0) {
        len = snprintf(buff,512,"FLAG REMOVE t%s m%s\nFlags: no-archive\n",tid,mid);
        writen(sock,buff,len);
      }

      answer = readline(sock,&rl);
      if(!answer || ((x = atoi(answer)) != 200)) {
        if(!answer) flt_noarchive_errno = 500;
        else flt_noarchive_errno = x;
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

      if(flt_noarchive_204) {
        printf("Status: 204 No Content\015\012\015\012");
        return FLT_EXIT;
      }
    }

    return FLT_OK;
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_noarchive_thread */
int flt_noarchive_thread(cf_hash_t *head,configuration_t *dc,configuration_t *vc,cl_thread_t *thread,int mode) {
  int si = cf_hash_get(GlobalValues,"ShowInvisible",13) != NULL;
  cf_readmode_t *rm = cf_hash_get(GlobalValues,"RM",2);
  u_char *link;
  size_t l;
  cf_post_flag_t *flag;

  if((flag = cf_flag_by_name(&thread->messages->flags,"no-archive")) != NULL) cf_tpl_hashvar_setvalue(&thread->messages->hashvar,"noarchive",TPL_VARIABLE_INT,1);

  if(si) {
    if(flag) {
      link = cf_advanced_get_link(rm->posting_uri[1],thread->tid,thread->messages->mid,NULL,1,&l,"a","remove-noarchive");
      cf_tpl_hashvar_setvalue(&thread->messages->hashvar,"removenoarchive_link",TPL_VARIABLE_STRING,link,l);
      free(link);
    }
    else {
      link = cf_advanced_get_link(rm->posting_uri[1],thread->tid,thread->messages->mid,NULL,1,&l,"a","set-noarchive");
      cf_tpl_hashvar_setvalue(&thread->messages->hashvar,"setnoarchive_link",TPL_VARIABLE_STRING,link,l);
      free(link);
    }
  }

  return FLT_OK;
}
/* }}} */

/* {{{ flt_noarchive_handle */
int flt_noarchive_handle(configfile_t *cfile,conf_opt_t *opt,const u_char *context,u_char **args,size_t argnum) {
  if(flt_noarchive_fn == NULL) flt_noarchive_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(flt_noarchive_fn,context) != 0) return 0;

  flt_noarchive_204 = cf_strcmp(args[0],"yes") == 0;

  return 0;
}
/* }}} */

conf_opt_t flt_noarchive_config[] = {
  { "NoArchiveSend204", flt_noarchive_handle, CFG_OPT_USER|CFG_OPT_LOCAL, NULL },
  { NULL, NULL, 0, NULL }
};

handler_config_t flt_noarchive_handlers[] = {
  { CONNECT_INIT_HANDLER, flt_noarchive_gogogo },
  { VIEW_HANDLER,         flt_noarchive_thread },
  { 0, NULL }
};

module_config_t flt_noarchive = {
  MODULE_MAGIC_COOKIE,
  flt_noarchive_config,
  flt_noarchive_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};

/* eof */
