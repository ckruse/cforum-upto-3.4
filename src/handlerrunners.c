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

#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "template.h"
#include "readline.h"
#include "charconvert.h"
#include "clientlib.h"
#include "cfcgi.h"
/* }}} */

/* {{{ cf_run_view_list_handlers */
int cf_run_view_list_handlers(t_message *p,t_cf_hash *head,u_int64_t tid,int mode) {
  int ret = FLT_OK;
  t_handler_config *handler;
  size_t i;
  t_filter_list_posting fkt;

  if(Modules[VIEW_LIST_HANDLER].elements) {
    for(i=0;i<Modules[VIEW_LIST_HANDLER].elements && (ret == FLT_OK || ret == FLT_DECLINE);i++) {
      handler = array_element_at(&Modules[VIEW_LIST_HANDLER],i);
      fkt     = (t_filter_list_posting)handler->func;
      ret     = fkt(head,&fo_default_conf,&fo_view_conf,p,tid,mode);
    }
  }

  return ret;
}
/* }}} */

/* {{{ cf_run_view_handlers */
int cf_run_view_handlers(t_cl_thread *thr,t_cf_hash *head,int mode) {
  int    ret = FLT_OK;
  t_handler_config *handler;
  size_t i;
  t_filter_list fkt;

  if(Modules[VIEW_HANDLER].elements) {
    for(i=0;i<Modules[VIEW_HANDLER].elements && (ret == FLT_OK || ret == FLT_DECLINE);i++) {
      handler = array_element_at(&Modules[VIEW_HANDLER],i);
      fkt     = (t_filter_list)handler->func;
      ret     = fkt(head,&fo_default_conf,&fo_view_conf,thr,mode);
    }
  }

  return ret;
}
/* }}} */

/* {{{ cf_run_posting_handlers */
int cf_run_posting_handlers(t_cf_hash *head,t_cl_thread *thr,t_cf_template *tpl,t_configuration *vc) {
  int ret = FLT_OK;
  t_handler_config *handler;
  size_t i;
  t_filter_posting fkt;

  if(Modules[POSTING_HANDLER].elements) {
    for(i=0;i<Modules[POSTING_HANDLER].elements && (ret == FLT_OK || ret == FLT_DECLINE);i++) {
      handler = array_element_at(&Modules[POSTING_HANDLER],i);
      fkt     = (t_filter_posting)handler->func;
      ret     = fkt(head,&fo_default_conf,vc,thr,tpl);
    }
  }

  return ret;
}
/* }}} */

/* {{{ cf_run_404_handlers */
int cf_run_404_handlers(t_cf_hash *head,u_int64_t tid,u_int64_t mid) {
  int ret = FLT_OK;
  t_handler_config *handler;
  size_t i;
  t_filter_404_handler fkt;

  if(Modules[HANDLE_404_HANDLER].elements) {
    for(i=0;i<Modules[HANDLE_404_HANDLER].elements && (ret == FLT_OK || ret == FLT_DECLINE);i++) {
      handler = array_element_at(&Modules[HANDLE_404_HANDLER],i);
      fkt     = (t_filter_404_handler)handler->func;
      ret     = fkt(head,&fo_default_conf,&fo_view_conf,tid,mid);
    }
  }

  return ret;
}
/* }}} */

/* {{{ cf_run_init_handlers */
int cf_run_init_handlers(t_cf_hash *head) {
  t_filter_begin exec;
  size_t i;
  t_handler_config *handler;
  int ret = FLT_DECLINE;

  if(Modules[INIT_HANDLER].elements) {
    for(i=0;i<Modules[INIT_HANDLER].elements && (ret == FLT_OK || ret == FLT_DECLINE);i++) {
      handler = array_element_at(&Modules[INIT_HANDLER],i);
      exec    = (t_filter_begin)handler->func;
      ret     = exec(head,&fo_default_conf,&fo_view_conf);
    }
  }

  return ret;
}
/* }}} */

/* {{{ cf_run_auth_handlers */
int cf_run_auth_handlers(t_cf_hash *head) {
  size_t i;
  int ret = FLT_DECLINE;
  t_filter_begin func;
  t_handler_config *handler;


  if(Modules[AUTH_HANDLER].elements) {
    ret = FLT_DECLINE;

    for(i=0;i<Modules[AUTH_HANDLER].elements && ret == FLT_DECLINE;i++) {
      handler = array_element_at(&Modules[AUTH_HANDLER],i);
      func    = (t_filter_begin)handler->func;
      ret     = func(head,&fo_default_conf,&fo_view_conf);
    }
  }

  return ret;
}
/* }}} */

/* {{{ cf_run_connect_init_handlers */
#ifdef CF_SHARED_MEM
int cf_run_connect_init_handlers(t_cf_hash *head,void *sock)
#else
int cf_run_connect_init_handlers(t_cf_hash *head,int sock)
#endif
{
  t_handler_config *handler;
  t_filter_connect exec;
  size_t i;
  int ext = FLT_OK,ret = FLT_DECLINE;

  if(Modules[CONNECT_INIT_HANDLER].elements) {
    for(i=0;i<Modules[CONNECT_INIT_HANDLER].elements;i++) {
      handler = array_element_at(&Modules[CONNECT_INIT_HANDLER],i);
      exec    = (t_filter_connect)handler->func;
      ret     = exec(head,&fo_default_conf,&fo_view_conf,sock);

      if(ret == FLT_EXIT) ext = 1;
    }
  }

  return ext;
}
/* }}} */

/* {{{ cf_run_sorting_handlers */
#ifdef CF_SHARED_MEM
int cf_run_sorting_handlers(t_cf_hash *head,void *ptr,t_array *threads)
#else
int cf_run_sorting_handlers(t_cf_hash *head,int sock,rline_t *tsd,t_array *threads)
#endif
{
  t_handler_config *handler;
  t_sorting_handler exec;
  size_t i;
  int ret = FLT_DECLINE;

  if(Modules[SORTING_HANDLER].elements) {
    for(i=0;i<Modules[SORTING_HANDLER].elements && ret == FLT_DECLINE;i++) {
      handler = array_element_at(&Modules[SORTING_HANDLER],i);
      exec    = (t_sorting_handler)handler->func;

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

/* {{{ cf_run_view_init_handlers */
int cf_run_view_init_handlers(t_cf_hash *head,t_cf_template *tpl_begin,t_cf_template *tpl_end) {
  t_handler_config *handler;
  t_filter_init_view fkt;
  size_t i;
  int ret = FLT_DECLINE;

  if(Modules[VIEW_INIT_HANDLER].elements) {
    for(i=0;i<Modules[VIEW_INIT_HANDLER].elements;i++) {
      handler = array_element_at(&Modules[VIEW_INIT_HANDLER],i);
      fkt     = (t_filter_init_view)handler->func;
      ret     = fkt(head,&fo_default_conf,&fo_view_conf,tpl_begin,tpl_end);
    }
  }

  return ret;
}
/* }}} */

/* {{{ cf_run_after_post_filters */
void cf_run_after_post_handlers(t_cf_hash *head,t_message *p,u_int64_t tid) {
  int ret = FLT_OK;
  t_handler_config *handler;
  size_t i;
  t_after_post_filter fkt;

  if(Modules[AFTER_POST_HANDLER].elements) {
    for(i=0;i<Modules[AFTER_POST_HANDLER].elements && (ret == FLT_OK || ret == FLT_DECLINE);i++) {
      handler = array_element_at(&Modules[AFTER_POST_HANDLER],i);
      fkt     = (t_after_post_filter)handler->func;
      ret     = fkt(head,&fo_default_conf,&fo_post_conf,p,tid);
    }
  }
}
/* }}} */

/* {{{ cf_run_post_filters */
#ifdef CF_SHARED_MEM
int cf_run_post_filters(t_cf_hash *head,t_message *p,void *ptr,int sock)
#else
int cf_run_post_filters(t_cf_hash *head,t_message *p,int sock)
#endif
{
  int ret = FLT_OK;
  t_handler_config *handler;
  size_t i;
  t_new_post_filter fkt;
  int fupto = 0;

  if(head) fupto = cf_cgi_get(head,"fupto") != NULL;

  if(Modules[NEW_POST_HANDLER].elements) {
    for(i=0;i<Modules[NEW_POST_HANDLER].elements && (ret == FLT_OK || ret == FLT_DECLINE);i++) {
      handler = array_element_at(&Modules[NEW_POST_HANDLER],i);
      fkt     = (t_new_post_filter)handler->func;
      #ifdef CF_SHARED_MEM
      ret     = fkt(head,&fo_default_conf,&fo_post_conf,p,ptr,sock,fupto);
      #else
      ret     = fkt(head,&fo_default_conf,&fo_post_conf,p,sock,fupto);
      #endif
    }
  }

  return ret;
}
/* }}} */

/* {{{ cf_run_post_display_handlers */
int cf_run_post_display_handlers(t_cf_hash *head,t_cf_template *tpl) {
  int ret = FLT_OK;
  t_handler_config *handler;
  size_t i;
  t_post_display_filter fkt;

  if(Modules[POST_DISPLAY_HANDLER].elements) {
    for(i=0;i<Modules[POST_DISPLAY_HANDLER].elements;++i) {
      handler = array_element_at(&Modules[POST_DISPLAY_HANDLER],i);
      fkt     = (t_post_display_filter)handler->func;
      ret     = fkt(head,&fo_default_conf,&fo_post_conf,tpl);
    }
  }

  return ret;
}
/* }}} */

/* eof */
