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
#include "cfconfig.h"
#include "defines.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <sys/types.h>

#include "hashlib.h"
#include "utils.h"
#include "cfgcomp.h"
#include "template.h"
#include "readline.h"
#include "charconvert.h"
#include "clientlib.h"
#include "cfcgi.h"
//#include "userconf.h"
/* }}} */

/* {{{ cf_run_view_list_handlers */
int cf_run_view_list_handlers(cf_cfg_config_t *cfg,cf_message_t *p,cf_hash_t *head,u_int64_t tid,int mode) {
  int ret = FLT_OK;
  cf_handler_config_t *handler;
  size_t i;
  cf_filter_list_posting_t fkt;

  if(cfg->modules[VIEW_LIST_HANDLER].elements) {
    for(i=0;i<cfg->modules[VIEW_LIST_HANDLER].elements && (ret == FLT_OK || ret == FLT_DECLINE);i++) {
      handler = cf_array_element_at(&cfg->modules[VIEW_LIST_HANDLER],i);
      fkt     = (cf_filter_list_posting_t)handler->func;
      ret     = fkt(head,cfg,p,tid,mode);
    }
  }

  return ret;
}
/* }}} */

/* {{{ cf_run_view_handlers */
int cf_run_view_handlers(cf_cfg_config_t *cfg,cf_cl_thread_t *thr,cf_hash_t *head,int mode) {
  int    ret = FLT_OK;
  cf_handler_config_t *handler;
  size_t i;
  cf_filter_list_t fkt;

  if(cfg->modules[VIEW_HANDLER].elements) {
    for(i=0;i<cfg->modules[VIEW_HANDLER].elements && (ret == FLT_OK || ret == FLT_DECLINE);i++) {
      handler = cf_array_element_at(&cfg->modules[VIEW_HANDLER],i);
      fkt     = (cf_filter_list_t)handler->func;
      ret     = fkt(head,cfg,thr,mode);
    }
  }

  return ret;
}
/* }}} */

/* {{{ cf_run_posting_handlers */
int cf_run_posting_handlers(cf_cfg_config_t *cfg,cf_hash_t *head,cf_cl_thread_t *thr,cf_template_t *tpl) {
  int ret = FLT_OK;
  cf_handler_config_t *handler;
  size_t i;
  cf_filter_posting_t fkt;

  if(cfg->modules[POSTING_HANDLER].elements) {
    for(i=0;i<cfg->modules[POSTING_HANDLER].elements && (ret == FLT_OK || ret == FLT_DECLINE);i++) {
      handler = cf_array_element_at(&cfg->modules[POSTING_HANDLER],i);
      fkt     = (cf_filter_posting_t)handler->func;
      ret     = fkt(head,cfg,thr,tpl);
    }
  }

  return ret;
}
/* }}} */

/* {{{ cf_run_404_handlers */
int cf_run_404_handlers(cf_cfg_config_t *cfg,cf_hash_t *head,u_int64_t tid,u_int64_t mid) {
  int ret = FLT_OK;
  cf_handler_config_t *handler;
  size_t i;
  cf_filter_404_handler_t fkt;

  if(cfg->modules[HANDLE_404_HANDLER].elements) {
    for(i=0;i<cfg->modules[HANDLE_404_HANDLER].elements && (ret == FLT_OK || ret == FLT_DECLINE);i++) {
      handler = cf_array_element_at(&cfg->modules[HANDLE_404_HANDLER],i);
      fkt     = (cf_filter_404_handler_t)handler->func;
      ret     = fkt(head,cfg,tid,mid);
    }
  }

  return ret;
}
/* }}} */

/* {{{ cf_run_init_handlers */
int cf_run_init_handlers(cf_cfg_config_t *cfg,cf_hash_t *head) {
  cf_filter_begin_t fkt;
  size_t i;
  cf_handler_config_t *handler;
  int ret = FLT_DECLINE;

  if(cfg->modules[INIT_HANDLER].elements) {
    for(i=0;i<cfg->modules[INIT_HANDLER].elements && (ret == FLT_OK || ret == FLT_DECLINE);i++) {
      handler = cf_array_element_at(&cfg->modules[INIT_HANDLER],i);
      fkt    = (cf_filter_begin_t)handler->func;
      ret     = fkt(head,cfg);
    }
  }

  return ret;
}
/* }}} */

/* {{{ cf_run_auth_handlers */
int cf_run_auth_handlers(cf_cfg_config_t *cfg,cf_hash_t *head) {
  size_t i;
  int ret = FLT_DECLINE;
  cf_filter_begin_t func;
  cf_handler_config_t *handler;


  if(cfg->modules[AUTH_HANDLER].elements) {
    ret = FLT_DECLINE;

    for(i=0;i<cfg->modules[AUTH_HANDLER].elements && ret == FLT_DECLINE;i++) {
      handler = cf_array_element_at(&cfg->modules[AUTH_HANDLER],i);
      func    = (cf_filter_begin_t)handler->func;
      ret     = func(head,cfg);
    }
  }

  return ret;
}
/* }}} */

/* {{{ cf_run_connect_init_handlers */
#ifdef CF_SHARED_MEM
int cf_run_connect_init_handlers(cf_cfg_config_t *cfg,cf_hash_t *head,void *sock)
#else
int cf_run_connect_init_handlers(cf_cfg_config_t *cfg,cf_hash_t *head,int sock)
#endif
{
  cf_handler_config_t *handler;
  cf_connect_filter_t fkt;
  size_t i;
  int ext = FLT_OK,ret = FLT_DECLINE;

  if(cfg->modules[CONNECT_INIT_HANDLER].elements) {
    for(i=0;i<cfg->modules[CONNECT_INIT_HANDLER].elements;i++) {
      handler = cf_array_element_at(&cfg->modules[CONNECT_INIT_HANDLER],i);
      fkt    = (cf_connect_filter_t)handler->func;
      ret     = fkt(head,cfg,sock);

      if(ret == FLT_EXIT) ext = FLT_EXIT;
    }
  }

  return ext;
}
/* }}} */

/* {{{ cf_run_sorting_handlers */
#ifdef CF_SHARED_MEM
int cf_run_sorting_handlers(cf_cfg_config_t *cfg,cf_hash_t *head,void *ptr,cf_array_t *threads)
#else
int cf_run_sorting_handlers(cf_cfg_config_t *cfg,cf_hash_t *head,int sock,rline_t *tsd,cf_array_t *threads)
#endif
{
  cf_handler_config_t *handler;
  cf_sorting_handler_t fkt;
  size_t i;
  int ret = FLT_DECLINE;

  if(cfg->modules[SORTING_HANDLER].elements) {
    for(i=0;i<cfg->modules[SORTING_HANDLER].elements && ret == FLT_DECLINE;i++) {
      handler = cf_array_element_at(&cfg->modules[SORTING_HANDLER],i);
      fkt    = (cf_sorting_handler_t)handler->func;

      #ifdef CF_SHARED_MEM
      ret     = fkt(head,cfg,ptr,threads);
      #else
      ret     = fkt(head,cfg,sock,tsd,threads);
      #endif
    }
  }

  return ret;
}
/* }}} */

/* {{{ cf_run_thread_sorting_handlers */
#ifdef CF_SHARED_MEM
int cf_run_thread_sorting_handlers(cf_cfg_config_t *cfg,cf_hash_t *head,void *ptr,cf_cl_thread_t *thread)
#else
int cf_run_thread_sorting_handlers(cf_cfg_config_t *cfg,cf_hash_t *head,int sock,rline_t *tsd,cf_cl_thread_t *thread)
#endif
{
  cf_handler_config_t *handler;
  cf_thread_sorting_handler_t fkt;
  size_t i;
  int ret = FLT_DECLINE;

  if(cfg->modules[THREAD_SORTING_HANDLER].elements) {
    for(i=0;i<cfg->modules[THREAD_SORTING_HANDLER].elements && ret == FLT_DECLINE;i++) {
      handler = cf_array_element_at(&cfg->modules[THREAD_SORTING_HANDLER],i);
      fkt    = (cf_thread_sorting_handler_t)handler->func;

      #ifdef CF_SHARED_MEM
      ret     = fkt(head,cfg,ptr,thread);
      #else
      ret     = fkt(head,cfg,sock,tsd,thread);
      #endif
    }
  }

  return ret;
}
/* }}} */

/* {{{ cf_run_view_init_handlers */
int cf_run_view_init_handlers(cf_cfg_config_t *cfg,cf_hash_t *head,cf_template_t *tpl_begin,cf_template_t *tpl_end) {
  cf_handler_config_t *handler;
  cf_filter_init_view_t fkt;
  size_t i;
  int ret = FLT_DECLINE;

  if(cfg->modules[VIEW_INIT_HANDLER].elements) {
    for(i=0;i<cfg->modules[VIEW_INIT_HANDLER].elements;i++) {
      handler = cf_array_element_at(&cfg->modules[VIEW_INIT_HANDLER],i);
      fkt     = (cf_filter_init_view_t)handler->func;
      ret     = fkt(head,cfg,tpl_begin,tpl_end);
    }
  }

  return ret;
}
/* }}} */

/* {{{ cf_run_after_post_filters */
#ifdef CF_SHARED_MEM
void cf_run_after_post_handlers(cf_cfg_config_t *cfg,cf_hash_t *head,cf_message_t *p,u_int64_t tid,void *shm,int sock)
#else
void cf_run_after_post_handlers(cf_cfg_config_t *cfg,cf_hash_t *head,cf_message_t *p,u_int64_t tid,int sock)
#endif
{
  int ret = FLT_OK;
  cf_handler_config_t *handler;
  size_t i;
  cf_after_post_filter_t fkt;

  if(cfg->modules[AFTER_POST_HANDLER].elements) {
    for(i=0;i<cfg->modules[AFTER_POST_HANDLER].elements && (ret == FLT_OK || ret == FLT_DECLINE);i++) {
      handler = cf_array_element_at(&cfg->modules[AFTER_POST_HANDLER],i);
      fkt     = (cf_after_post_filter_t)handler->func;

      #ifdef CF_SHARED_MEM
      ret     = fkt(head,cfg,p,tid,sock,shm);
      #else
      ret     = fkt(head,cfg,p,tid,sock);
      #endif
    }
  }
}
/* }}} */

/* {{{ cf_run_post_filters */
#ifdef CF_SHARED_MEM
int cf_run_post_filters(cf_cfg_config_t *cfg,cf_hash_t *head,cf_message_t *p,cf_cl_thread_t *thr,void *ptr,int sock)
#else
int cf_run_post_filters(cf_cfg_config_t *cfg,cf_hash_t *head,cf_message_t *p,cf_cl_thread_t *thr,int sock)
#endif
{
  int ret = FLT_OK;
  cf_handler_config_t *handler;
  size_t i;
  cf_new_post_filter_t fkt;
  int fupto = 0;

  if(head) fupto = cf_cgi_get(head,"fupto") != NULL;

  if(cfg->modules[NEW_POST_HANDLER].elements) {
    for(i=0;i<cfg->modules[NEW_POST_HANDLER].elements && (ret == FLT_OK || ret == FLT_DECLINE);i++) {
      handler = cf_array_element_at(&cfg->modules[NEW_POST_HANDLER],i);
      fkt     = (cf_new_post_filter_t)handler->func;
      #ifdef CF_SHARED_MEM
      ret     = fkt(head,cfg,p,thr,ptr,sock,fupto);
      #else
      ret     = fkt(head,cfg,p,thr,sock,fupto);
      #endif
    }
  }

  return ret;
}
/* }}} */

/* {{{ cf_run_post_display_handlers */
int cf_run_post_display_handlers(cf_cfg_config_t *cfg,cf_hash_t *head,cf_template_t *tpl,cf_message_t *p) {
  int ret = FLT_OK;
  cf_handler_config_t *handler;
  size_t i;
  cf_post_display_filter_t fkt;

  if(cfg->modules[POST_DISPLAY_HANDLER].elements) {
    for(i=0;i<cfg->modules[POST_DISPLAY_HANDLER].elements;++i) {
      handler = cf_array_element_at(&cfg->modules[POST_DISPLAY_HANDLER],i);
      fkt     = (cf_post_display_filter_t)handler->func;
      ret     = fkt(head,cfg,tpl,p);
    }
  }

  return ret;
}
/* }}} */

/* {{{ cf_run_perpost_var_handlers */
int cf_run_perpost_var_handlers(cf_cfg_config_t *cfg,cf_hash_t *head,cf_cl_thread_t *thread,cf_message_t *msg,cf_tpl_variable_t *hash) {
  int ret = FLT_OK;
  cf_handler_config_t *handler;
  size_t i;
  cf_filter_perpost_var_t fkt;

  if(cfg->modules[PERPOST_VAR_HANDLER].elements) {
    for(i=0;i<cfg->modules[PERPOST_VAR_HANDLER].elements && (ret == FLT_OK || ret == FLT_DECLINE);i++) {
      handler = cf_array_element_at(&cfg->modules[PERPOST_VAR_HANDLER],i);
      fkt     = (cf_filter_perpost_var_t)handler->func;
      ret     = fkt(head,cfg,thread,msg,hash);
    }
  }

  return ret;
}
/* }}} */

/* {{{ cf_run_readmode_collectors */
int cf_run_readmode_collectors(cf_cfg_config_t *cfg,cf_hash_t *head,cf_readmode_t *rm_infos) {
  int ret = FLT_DECLINE,ext = FLT_EXIT;
  cf_handler_config_t *handler;
  size_t i;
  cf_readmode_collector_t fkt;

  if(cfg->modules[RM_COLLECTORS_HANDLER].elements) {
    for(i=0;i<cfg->modules[RM_COLLECTORS_HANDLER].elements && ret == FLT_DECLINE;i++) {
      handler   = cf_array_element_at(&cfg->modules[RM_COLLECTORS_HANDLER],i);
      fkt       = (cf_readmode_collector_t)handler->func;
      ext = ret = fkt(head,cfg,rm_infos);
    }
  }

  return ext;
}
/* }}} */

#if 0
/* {{{ cf_run_uconf_write_handlers */
int cf_run_uconf_write_handlers(cf_cfg_config_t *cfg,cf_hash_t *cgi,cf_cfg_config_t *oldconf,cf_uconf_userconfig_t *newconf) {
  int ret = FLT_OK;
  cf_handler_config_t *handler;
  size_t i;
  cf_uconf_write_filter_t fkt;

  if(cfg->modules[UCONF_WRITE_HANDLER].elements) {
    for(i=0;i<cfg->modules[UCONF_WRITE_HANDLER].elements && ret != FLT_EXIT;++i) {
      handler = cf_array_element_at(&cfg->modules[UCONF_WRITE_HANDLER],i);
      fkt     = (cf_uconf_write_filter_t)handler->func;
      ret     = fkt(cgi,cfg,oldconf,newconf);
    }
  }

  return ret;
}
/* }}} */

/* {{{ cf_run_uconf_display_handlers */
void cf_run_uconf_display_handlers(cf_cfg_config_t *cfg,cf_hash_t *cgi,cf_template_t *tpl,cf_cfg_config_t *user) {
  cf_handler_config_t *handler;
  size_t i;
  cf_uconf_display_filter_t fkt;

  if(cfg->modules[UCONF_DISPLAY_HANDLER].elements) {
    for(i=0;i<cfg->modules[UCONF_DISPLAY_HANDLER].elements;++i) {
      handler = cf_array_element_at(&cfg->modules[UCONF_DISPLAY_HANDLER],i);
      fkt     = (cf_uconf_display_filter_t)handler->func;
      fkt(cgi,cfg,tpl,user);
    }
  }
}
/* }}} */
#endif

/* eof */
