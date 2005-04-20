/**
 * \file flt_noanswer.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
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

static int flt_na_errno = 0;

/* {{{ flt_noanswer_gogogo */
#ifndef CF_SHARED_MEM
int flt_noanswer_gogogo(t_cf_hash *cgi,t_configuration *dc,t_configuration *vc,int sock)
#else
int flt_noanswer_gogogo(t_cf_hash *cgi,t_configuration *dc,t_configuration *vc,void *ptr)
#endif
{
  #ifdef CF_SHARED_MEM
  int sock;
  #endif

  size_t len;
  rline_t rl;
  u_char *action = NULL,*tid,*mid,buff[512],*uname,*fn,*answer;
  int si = cf_hash_get(GlobalValues,"ShowInvisible",13) != NULL,x = 0;

  if(si == 0 || cgi == NULL) return FLT_DECLINE;

  if((action = cf_cgi_get(cgi,"a")) != NULL) {
    uname = cf_hash_get(GlobalValues,"UserName",8);
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
      if(!answer) flt_na_errno = 500;
      else flt_na_errno = x;
    }
    if(answer) free(answer);

    if(x == 0 || x == 200) {
      if(cf_strcmp(action,"set-na") == 0) {
        len = snprintf(buff,512,"FLAG SET t%s m%s\nFlag: no-answer=yes\n\n",tid,mid);
        writen(sock,buff,len);
      }
      else if(cf_strcmp(action,"remove-na") == 0) {
        len = snprintf(buff,512,"FLAG REMOVE t%s m%s\nFlags: no-answer\n",tid,mid);
        writen(sock,buff,len);
      }

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

    cf_hash_entry_delete(cgi,"t",1);
    cf_hash_entry_delete(cgi,"m",1);

    return FLT_OK;
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_noanswer_posthandler */
int flt_noanswer_posthandler(t_cf_hash *cgi,t_configuration *dc,t_configuration *vc,t_message *msg,u_int64_t tid,int mode) {
  int si = cf_hash_get(GlobalValues,"ShowInvisible",13) != NULL;
  cf_readmode_t *rm = cf_hash_get(GlobalValues,"RM",2);
  t_cf_post_flag *flag;
  u_char *link;
  size_t l;

  if((flag = cf_flag_by_name(&msg->flags,"no-answer")) != NULL) cf_tpl_setvalue(&msg->tpl,"na",TPL_VARIABLE_INT,1);

  if(si) {
    if(flag) {
      link = cf_advanced_get_link(rm->posting_uri[1],tid,msg->mid,NULL,1,&l,"a","remove-na");
      cf_tpl_setvalue(&msg->tpl,"removena_link",TPL_VARIABLE_STRING,link,l);
      free(link);
    }
    else {
      link = cf_advanced_get_link(rm->posting_uri[1],tid,msg->mid,NULL,1,&l,"a","set-na");
      cf_tpl_setvalue(&msg->tpl,"setna_link",TPL_VARIABLE_STRING,link,l);
      free(link);
    }
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_noanswer_setvars */
int flt_noanswer_setvars(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_cl_thread *thread,t_message *msg,t_cf_tpl_variable *hash) {
  int si = cf_hash_get(GlobalValues,"ShowInvisible",13) != NULL;
  t_cf_post_flag *flag;

  if((flag = cf_flag_by_name(&msg->flags,"no-answer")) != NULL) cf_tpl_hashvar_setvalue(hash,"na",TPL_VARIABLE_INT,1);

  return FLT_OK;
}
/* }}} */

/* {{{ flt_noanswer_post */
#ifdef CF_SHARED_MEM
int flt_noanswer_post(t_cf_hash *head,t_configuration *dc,t_configuration *pc,t_message *p,t_cl_thread *thr,void *ptr,int sock,int mode)
#else
int flt_noanswer_post(t_cf_hash *head,t_configuration *dc,t_configuration *pc,t_message *p,t_cl_thread *thr,int sock,int mode)
#endif
{
  t_name_value *cs;
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

t_conf_opt flt_noanswer_config[] = {
  { NULL, NULL, 0, NULL }
};

t_handler_config flt_noanswer_handlers[] = {
  { CONNECT_INIT_HANDLER, flt_noanswer_gogogo },
  { VIEW_LIST_HANDLER,    flt_noanswer_posthandler },
  { PERPOST_VAR_HANDLER,  flt_noanswer_setvars },
  { NEW_POST_HANDLER,     flt_noanswer_post },
  { 0, NULL }
};

t_module_config flt_noanswer = {
  flt_noanswer_config,
  flt_noanswer_handlers,
  NULL,
  NULL,
  NULL,
  NULL
};

/* eof */
