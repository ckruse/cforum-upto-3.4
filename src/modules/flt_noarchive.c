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

static int flt_noarchive_errno = 0;

/* {{{ flt_noarchive_gogogo */
#ifndef CF_SHARED_MEM
int flt_noarchive_gogogo(t_cf_hash *cgi,t_configuration *dc,t_configuration *vc,int sock)
#else
int flt_noarchive_gogogo(t_cf_hash *cgi,t_configuration *dc,t_configuration *vc,void *ptr)
#endif
{
  #ifdef CF_SHARED_MEM
  int sock;
  #endif

  size_t len;
  rline_t rl;
  u_char *action = NULL,*tid,*mid,buff[512],*fn,*answer;
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

    cf_hash_entry_delete(cgi,"t",1);
    cf_hash_entry_delete(cgi,"m",1);

    return FLT_OK;
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_noarchive_thread */
int flt_noarchive_thread(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_cl_thread *thread,int mode) {
  int si = cf_hash_get(GlobalValues,"ShowInvisible",13) != NULL,ret;
  cf_readmode_t *rm = cf_hash_get(GlobalValues,"RM",2);
  u_char *link;
  size_t l;
  t_cf_post_flag *flag;

  if((flag = cf_flag_by_name(&thread->messages->flags,"no-archive")) != NULL) cf_tpl_setvalue(&thread->messages->tpl,"noarchive",TPL_VARIABLE_INT,1);

  if(si) {
    if(flag) {
      link = cf_advanced_get_link(rm->posting_uri[1],thread->tid,thread->messages->mid,NULL,1,&l,"a","remove-noarchive");
      cf_tpl_setvalue(&thread->messages->tpl,"removenoarchive_link",TPL_VARIABLE_STRING,link,l);
      free(link);
    }
    else {
      link = cf_advanced_get_link(rm->posting_uri[1],thread->tid,thread->messages->mid,NULL,1,&l,"a","set-noarchive");
      cf_tpl_setvalue(&thread->messages->tpl,"setnoarchive_link",TPL_VARIABLE_STRING,link,l);
      free(link);
    }
  }

  return FLT_OK;
}
/* }}} */

t_conf_opt flt_noarchive_config[] = {
  { NULL, NULL, 0, NULL }
};

t_handler_config flt_noarchive_handlers[] = {
  { CONNECT_INIT_HANDLER, flt_noarchive_gogogo },
  { VIEW_HANDLER,         flt_noarchive_thread },
  { 0, NULL }
};

t_module_config flt_noarchive = {
  flt_noarchive_config,
  flt_noarchive_handlers,
  NULL,
  NULL,
  NULL,
  NULL
};

/* eof */
