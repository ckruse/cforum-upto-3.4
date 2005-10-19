/**
 * \file handlerrunners.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 * \brief handler running functions
 *
 * This file contains handler running functions on client side
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <sys/types.h>

#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "template.h"
#include "readline.h"
#include "charconvert.h"
#include "clientlib.h"
#include "cfcgi.h"
#include "userconf.h"
/* }}} */

/* {{{ cf_run_view_list_handlers */
int cf_run_view_list_handlers(message_t *p,cf_hash_t *head,u_int64_t tid,int mode) {
  int ret = FLT_OK;
  cf_handler_config_t *handler;
  size_t i;
  filter_list_posting_t fkt;

  if(Modules[VIEW_LIST_HANDLER].elements) {
    for(i=0;i<Modules[VIEW_LIST_HANDLER].elements && (ret == FLT_OK || ret == FLT_DECLINE);i++) {
      handler = cf_array_element_at(&Modules[VIEW_LIST_HANDLER],i);
      fkt     = (filter_list_posting_t)handler->func;
      ret     = fkt(head,&fo_default_conf,&fo_view_conf,p,tid,mode);
    }
  }

  return ret;
}
/* }}} */

/* {{{ cf_run_view_handlers */
int cf_run_view_handlers(cl_thread_t *thr,cf_hash_t *head,int mode) {
  int    ret = FLT_OK;
  cf_handler_config_t *handler;
  size_t i;
  filter_list_t fkt;

  if(Modules[VIEW_HANDLER].elements) {
    for(i=0;i<Modules[VIEW_HANDLER].elements && (ret == FLT_OK || ret == FLT_DECLINE);i++) {
      handler = cf_array_element_at(&Modules[VIEW_HANDLER],i);
      fkt     = (filter_list_t)handler->func;
      ret     = fkt(head,&fo_default_conf,&fo_view_conf,thr,mode);
    }
  }

  return ret;
}
/* }}} */

/* {{{ cf_run_posting_handlers */
int cf_run_posting_handlers(cf_hash_t *head,cl_thread_t *thr,cf_template_t *tpl,cf_configuration_t *vc) {
  int ret = FLT_OK;
  cf_handler_config_t *handler;
  size_t i;
  filter_posting_t fkt;

  if(Modules[POSTING_HANDLER].elements) {
    for(i=0;i<Modules[POSTING_HANDLER].elements && (ret == FLT_OK || ret == FLT_DECLINE);i++) {
      handler = cf_array_element_at(&Modules[POSTING_HANDLER],i);
      fkt     = (filter_posting_t)handler->func;
      ret     = fkt(head,&fo_default_conf,vc,thr,tpl);
    }
  }

  return ret;
}
/* }}} */

/* {{{ cf_run_404_handlers */
int cf_run_404_handlers(cf_hash_t *head,u_int64_t tid,u_int64_t mid) {
  int ret = FLT_OK;
  cf_handler_config_t *handler;
  size_t i;
  filter_404_handler_t fkt;

  if(Modules[HANDLE_404_HANDLER].elements) {
    for(i=0;i<Modules[HANDLE_404_HANDLER].elements && (ret == FLT_OK || ret == FLT_DECLINE);i++) {
      handler = cf_array_element_at(&Modules[HANDLE_404_HANDLER],i);
      fkt     = (filter_404_handler_t)handler->func;
      ret     = fkt(head,&fo_default_conf,&fo_view_conf,tid,mid);
    }
  }

  return ret;
}
/* }}} */

/* {{{ cf_run_init_handlers */
int cf_run_init_handlers(cf_hash_t *head) {
  filter_begin_t exec;
  size_t i;
  cf_handler_config_t *handler;
  int ret = FLT_DECLINE;

  if(Modules[INIT_HANDLER].elements) {
    for(i=0;i<Modules[INIT_HANDLER].elements && (ret == FLT_OK || ret == FLT_DECLINE);i++) {
      handler = cf_array_element_at(&Modules[INIT_HANDLER],i);
      exec    = (filter_begin_t)handler->func;
      ret     = exec(head,&fo_default_conf,&fo_view_conf);
    }
  }

  return ret;
}
/* }}} */

/* {{{ cf_run_auth_handlers */
int cf_run_auth_handlers(cf_hash_t *head) {
  size_t i;
  int ret = FLT_DECLINE;
  filter_begin_t func;
  cf_handler_config_t *handler;


  if(Modules[AUTH_HANDLER].elements) {
    ret = FLT_DECLINE;

    for(i=0;i<Modules[AUTH_HANDLER].elements && ret == FLT_DECLINE;i++) {
      handler = cf_array_element_at(&Modules[AUTH_HANDLER],i);
      func    = (filter_begin_t)handler->func;
      ret     = func(head,&fo_default_conf,&fo_view_conf);
    }
  }

  return ret;
}
/* }}} */

/* {{{ cf_run_connect_init_handlers */
#ifdef CF_SHARED_MEM
int cf_run_connect_init_handlers(cf_hash_t *head,void *sock)
#else
int cf_run_connect_init_handlers(cf_hash_t *head,int sock)
#endif
{
  cf_handler_config_t *handler;
  connect_filter_t exec;
  size_t i;
  int ext = FLT_OK,ret = FLT_DECLINE;

  if(Modules[CONNECT_INIT_HANDLER].elements) {
    for(i=0;i<Modules[CONNECT_INIT_HANDLER].elements;i++) {
      handler = cf_array_element_at(&Modules[CONNECT_INIT_HANDLER],i);
      exec    = (connect_filter_t)handler->func;
      ret     = exec(head,&fo_default_conf,&fo_view_conf,sock);

      if(ret == FLT_EXIT) ext = FLT_EXIT;
    }
  }

  return ext;
}
/* }}} */

/* {{{ cf_run_sorting_handlers */
#ifdef CF_SHARED_MEM
int cf_run_sorting_handlers(cf_hash_t *head,void *ptr,cf_array_t *threads)
#else
int cf_run_sorting_handlers(cf_hash_t *head,int sock,rline_t *tsd,cf_array_t *threads)
#endif
{
  cf_handler_config_t *handler;
  sorting_handler_t exec;
  size_t i;
  int ret = FLT_DECLINE;

  if(Modules[SORTING_HANDLER].elements) {
    for(i=0;i<Modules[SORTING_HANDLER].elements && ret == FLT_DECLINE;i++) {
      handler = cf_array_element_at(&Modules[SORTING_HANDLER],i);
      exec    = (sorting_handler_t)handler->func;

      #ifdef CF_SHARED_MEM
      ret     = exec(head,&fo_default_conf,&fo_view_conf,ptr,threads);
      #else
      ret     = exec(head,&fo_default_conf,&fo_view_conf,sock,tsd,threads);
      #endif
    }
  }

  return ret;
}
/* }}} */

/* {{{ cf_run_thread_sorting_handlers */
#ifdef CF_SHARED_MEM
int cf_run_thread_sorting_handlers(cf_hash_t *head,void *ptr,cl_thread_t *thread)
#else
int cf_run_thread_sorting_handlers(cf_hash_t *head,int sock,rline_t *tsd,cl_thread_t *thread)
#endif
{
  cf_handler_config_t *handler;
  thread_sorting_handler_t exec;
  size_t i;
  int ret = FLT_DECLINE;

  if(Modules[THREAD_SORTING_HANDLER].elements) {
    for(i=0;i<Modules[THREAD_SORTING_HANDLER].elements && ret == FLT_DECLINE;i++) {
      handler = cf_array_element_at(&Modules[THREAD_SORTING_HANDLER],i);
      exec    = (thread_sorting_handler_t)handler->func;

      #ifdef CF_SHARED_MEM
      ret     = exec(head,&fo_default_conf,&fo_view_conf,ptr,thread);
      #else
      ret     = exec(head,&fo_default_conf,&fo_view_conf,sock,tsd,thread);
      #endif
    }
  }

  return ret;
}
/* }}} */

/* {{{ cf_run_view_init_handlers */
int cf_run_view_init_handlers(cf_hash_t *head,cf_template_t *tpl_begin,cf_template_t *tpl_end) {
  cf_handler_config_t *handler;
  filter_init_view_t fkt;
  size_t i;
  int ret = FLT_DECLINE;

  if(Modules[VIEW_INIT_HANDLER].elements) {
    for(i=0;i<Modules[VIEW_INIT_HANDLER].elements;i++) {
      handler = cf_array_element_at(&Modules[VIEW_INIT_HANDLER],i);
      fkt     = (filter_init_view_t)handler->func;
      ret     = fkt(head,&fo_default_conf,&fo_view_conf,tpl_begin,tpl_end);
    }
  }

  return ret;
}
/* }}} */

/* {{{ cf_run_after_post_filters */
#ifdef CF_SHARED_MEM
void cf_run_after_post_handlers(cf_hash_t *head,message_t *p,u_int64_t tid,void *shm,int sock)
#else
void cf_run_after_post_handlers(cf_hash_t *head,message_t *p,u_int64_t tid,int sock)
#endif
{
  int ret = FLT_OK;
  cf_handler_config_t *handler;
  size_t i;
  after_post_filter_t fkt;

  if(Modules[AFTER_POST_HANDLER].elements) {
    for(i=0;i<Modules[AFTER_POST_HANDLER].elements && (ret == FLT_OK || ret == FLT_DECLINE);i++) {
      handler = cf_array_element_at(&Modules[AFTER_POST_HANDLER],i);
      fkt     = (after_post_filter_t)handler->func;

      #ifdef CF_SHARED_MEM
      ret     = fkt(head,&fo_default_conf,&fo_post_conf,p,tid,sock,shm);
      #else
      ret     = fkt(head,&fo_default_conf,&fo_post_conf,p,tid,sock);
      #endif
    }
  }
}
/* }}} */

/* {{{ cf_run_post_filters */
#ifdef CF_SHARED_MEM
int cf_run_post_filters(cf_hash_t *head,message_t *p,cl_thread_t *thr,void *ptr,int sock)
#else
int cf_run_post_filters(cf_hash_t *head,message_t *p,cl_thread_t *thr,int sock)
#endif
{
  int ret = FLT_OK;
  cf_handler_config_t *handler;
  size_t i;
  new_post_filter_t fkt;
  int fupto = 0;

  if(head) fupto = cf_cgi_get(head,"fupto") != NULL;

  if(Modules[NEW_POST_HANDLER].elements) {
    for(i=0;i<Modules[NEW_POST_HANDLER].elements && (ret == FLT_OK || ret == FLT_DECLINE);i++) {
      handler = cf_array_element_at(&Modules[NEW_POST_HANDLER],i);
      fkt     = (new_post_filter_t)handler->func;
      #ifdef CF_SHARED_MEM
      ret     = fkt(head,&fo_default_conf,&fo_post_conf,p,thr,ptr,sock,fupto);
      #else
      ret     = fkt(head,&fo_default_conf,&fo_post_conf,p,thr,sock,fupto);
      #endif
    }
  }

  return ret;
}
/* }}} */

/* {{{ cf_run_post_display_handlers */
int cf_run_post_display_handlers(cf_hash_t *head,cf_template_t *tpl,message_t *p) {
  int ret = FLT_OK;
  cf_handler_config_t *handler;
  size_t i;
  post_display_filter_t fkt;

  if(Modules[POST_DISPLAY_HANDLER].elements) {
    for(i=0;i<Modules[POST_DISPLAY_HANDLER].elements;++i) {
      handler = cf_array_element_at(&Modules[POST_DISPLAY_HANDLER],i);
      fkt     = (post_display_filter_t)handler->func;
      ret     = fkt(head,&fo_default_conf,&fo_post_conf,tpl,p);
    }
  }

  return ret;
}
/* }}} */

/* {{{ cf_run_perpost_var_handlers */
int cf_run_perpost_var_handlers(cf_hash_t *head,cl_thread_t *thread,message_t *msg,cf_tpl_variable_t *hash) {
  int ret = FLT_OK;
  cf_handler_config_t *handler;
  size_t i;
  filter_perpost_var_t fkt;

  if(Modules[PERPOST_VAR_HANDLER].elements) {
    for(i=0;i<Modules[PERPOST_VAR_HANDLER].elements && (ret == FLT_OK || ret == FLT_DECLINE);i++) {
      handler = cf_array_element_at(&Modules[PERPOST_VAR_HANDLER],i);
      fkt     = (filter_perpost_var_t)handler->func;
      ret     = fkt(head,&fo_default_conf,&fo_view_conf,thread,msg,hash);
    }
  }

  return ret;
}
/* }}} */

/* {{{ cf_run_readmode_collectors */
int cf_run_readmode_collectors(cf_hash_t *head,cf_configuration_t *vc,cf_readmode_t *rm_infos) {
  int ret = FLT_DECLINE,ext = FLT_EXIT;
  cf_handler_config_t *handler;
  size_t i;
  cf_readmode_collector_t fkt;

  if(Modules[RM_COLLECTORS_HANDLER].elements) {
    for(i=0;i<Modules[RM_COLLECTORS_HANDLER].elements && ret == FLT_DECLINE;i++) {
      handler   = cf_array_element_at(&Modules[RM_COLLECTORS_HANDLER],i);
      fkt       = (cf_readmode_collector_t)handler->func;
      ext = ret = fkt(head,&fo_default_conf,vc,rm_infos);
    }
  }

  return ext;
}
/* }}} */

/* {{{ cf_run_uconf_write_handlers */
int cf_run_uconf_write_handlers(cf_hash_t *cgi,cf_configuration_t *dc,cf_configuration_t *uc,cf_configuration_t *oldconf,uconf_userconfig_t *newconf) {
  int ret = FLT_OK;
  cf_handler_config_t *handler;
  size_t i;
  uconf_write_filter_t fkt;

  if(Modules[UCONF_WRITE_HANDLER].elements) {
    for(i=0;i<Modules[UCONF_WRITE_HANDLER].elements && ret != FLT_EXIT;++i) {
      handler = cf_array_element_at(&Modules[UCONF_WRITE_HANDLER],i);
      fkt     = (uconf_write_filter_t)handler->func;
      ret     = fkt(cgi,dc,uc,oldconf,newconf);
    }
  }

  return ret;
}
/* }}} */

/* {{{ cf_run_uconf_display_handlers */
void cf_run_uconf_display_handlers(cf_hash_t *cgi,cf_configuration_t *dc,cf_configuration_t *uc,cf_template_t *tpl,cf_configuration_t *user) {
  cf_handler_config_t *handler;
  size_t i;
  uconf_display_filter_t fkt;

  if(Modules[UCONF_DISPLAY_HANDLER].elements) {
    for(i=0;i<Modules[UCONF_DISPLAY_HANDLER].elements;++i) {
      handler = cf_array_element_at(&Modules[UCONF_DISPLAY_HANDLER],i);
      fkt     = (uconf_display_filter_t)handler->func;
      fkt(cgi,dc,uc,tpl,user);
    }
  }
}
/* }}} */

/* eof */
