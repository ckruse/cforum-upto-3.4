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
#include "cfconfig.h"
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
#include "cfgcomp.h"
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
int flt_noarchive_gogogo(cf_hash_t *cgi,cf_configuration_t *dc,cf_configuration_t *vc,int sock)
#else
int flt_noarchive_gogogo(cf_hash_t *cgi,cf_configuration_t *dc,cf_configuration_t *vc,void *ptr)
#endif
{
  #ifdef CF_SHARED_MEM
  int sock;
  #endif

  size_t len;
  rline_t rl;
  u_char buff[512],*fn,*answer;
  int si = cf_hash_get(GlobalValues,"ShowInvisible",13) != NULL,x = 0;
  cf_string_t *action = NULL,*tid,*mid,*mode,str;

  u_int64_t itid,imid;

  if(si == 0 || cgi == NULL) return FLT_DECLINE;

  if((action = cf_cgi_get(cgi,"a")) != NULL && (cf_strcmp(action->content,"set-noarchive") == 0 || cf_strcmp(action->content,"remove-noarchive") == 0)) {
    fn  = cf_hash_get(GlobalValues,"FORUM_NAME",10);

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
      if(!answer) flt_noarchive_errno = 500;
      else flt_noarchive_errno = x;
    }
    if(answer) free(answer);

    if(x == 0 || x == 200) {
      cf_str_init_growth(&str,256);

      if(cf_strcmp(action->content,"set-noarchive") == 0)         cf_str_char_set(&str,"FLAG SET ",9);
      else if(cf_strcmp(action->content,"remove-noarchive") == 0) cf_str_char_set(&str,"FLAG REMOVE ",12);

      cf_str_char_append(&str,'t');
      cf_uint32_to_str(&str,itid);
      cf_str_chars_append(&str," m",2);
      cf_uint32_to_str(&str,imid);
      cf_str_chars_append(&str,"\nFlag: no-archive",17);
      if(cf_strcmp(action->content,"set-noarchive") == 0) cf_str_chars_append(&str,"=yes",4);
      cf_str_chars_append(&str,"\n\n",2);

      writen(sock,str.content,str.len);

      cf_str_cleanup(&str);

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

    if((mode = cf_cgi_get(cgi,"mode")) == NULL || cf_strcmp(mode->content,"xmlhttp") != 0) {
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
int flt_noarchive_thread(cf_hash_t *head,cf_configuration_t *dc,cf_configuration_t *vc,cl_thread_t *thread,int mode) {
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
int flt_noarchive_handle(cf_configfile_t *cfile,cf_conf_opt_t *opt,const u_char *context,u_char **args,size_t argnum) {
  if(flt_noarchive_fn == NULL) flt_noarchive_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(flt_noarchive_fn,context) != 0) return 0;

  flt_noarchive_204 = cf_strcmp(args[0],"yes") == 0;

  return 0;
}
/* }}} */

cf_conf_opt_t flt_noarchive_config[] = {
  { "NoArchiveSend204", flt_noarchive_handle, CF_CFG_OPT_USER|CF_CFG_OPT_LOCAL, NULL },
  { NULL, NULL, 0, NULL }
};

cf_handler_config_t flt_noarchive_handlers[] = {
  { CONNECT_INIT_HANDLER, flt_noarchive_gogogo },
  { VIEW_HANDLER,         flt_noarchive_thread },
  { 0, NULL }
};

cf_module_config_t flt_noarchive = {
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
