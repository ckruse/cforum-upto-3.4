/**
 * \file flt_sorting.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 *
 * This plugin sorts messages and threads
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
#include <time.h>

#include <string.h>
#include <ctype.h>
#include <sys/types.h>

#include "readline.h"
#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "cfcgi.h"
#include "template.h"
#include "clientlib.h"
#include "charconvert.h"
/* }}} */

static int flt_sorting_sorthread_ts  = -1;
static int flt_sorting_sormessage_ts = -1;

static u_char *flt_sorting_fn = NULL;

/* {{{ flt_sorting_threads_cmp */
int flt_sorting_threads_cmp(const void *a,const void *b) {
  cl_thread_t *ta = (cl_thread_t *)a;
  cl_thread_t *tb = (cl_thread_t *)b;

  if(flt_sorting_sorthread_ts == CF_SORT_ASCENDING) {
    if(ta->messages->date > tb->messages->date) return 1;
    else if(ta->messages->date < tb->messages->date) return -1;
  }
  else if(flt_sorting_sorthread_ts == CF_SORT_DESCENDING) {
    if(ta->messages->date > tb->messages->date) return -1;
    else if(ta->messages->date < tb->messages->date) return 1;
  }
  else {
    if(ta->newest->date > tb->newest->date) return -1;
    else if(ta->newest->date < tb->newest->date) return 1;
  }

  return 0;
}
/* }}} */

/* {{{ flt_sorting_msgs_cmp */
int flt_sorting_msgs_cmp(const void *a,const void *b) {
  hierarchical_node_t *na = (hierarchical_node_t *)a;
  hierarchical_node_t *nb = (hierarchical_node_t *)b;

  if(flt_sorting_sormessage_ts == CF_SORT_ASCENDING) {
    if(na->msg->date > nb->msg->date) return 1;
    else if(na->msg->date < nb->msg->date) return -1;
  }
  else {
    if(na->msg->date > nb->msg->date) return -1;
    else if(na->msg->date < nb->msg->date) return 1;
  }

  return 0;
}
/* }}} */

/* {{{ flt_sorting_sort_msgs */
void flt_sorting_sort_msgs(hierarchical_node_t *h) {
  size_t i;

  cf_array_sort(&h->childs,flt_sorting_msgs_cmp);
  for(i=0;i<h->childs.elements;++i) flt_sorting_sort_msgs(cf_array_element_at(&h->childs,i));
}
/* }}} */

/* {{{ flt_sorting_sort */
#ifndef CF_SHARED_MEM
int flt_sorting_sort(cf_hash_t *head,cf_configuration_t *dc,cf_configuration_t *vc,int sock,rline_t *tsd,cf_array_t *threads)
#else
int flt_sorting_sort(cf_hash_t *head,cf_configuration_t *dc,cf_configuration_t *vc,void *ptr,cf_array_t *threads)
#endif
{
  size_t i;
  cl_thread_t *thr;

  /* sort threads first */
  if(flt_sorting_sorthread_ts != -1) cf_array_sort(threads,flt_sorting_threads_cmp);

  if(flt_sorting_sormessage_ts != -1) {
    for(i=0;i<threads->elements;++i) {
      thr = cf_array_element_at(threads,i);
      flt_sorting_sort_msgs(thr->ht);
      cf_msg_linearize(thr,thr->ht);
    }
  }

  return FLT_OK;
}
/* }}} */

/* {{{ flt_sorting_sorthread_t */
#ifndef CF_SHARED_MEM
int flt_sorting_sorthread_t(cf_hash_t *head,cf_configuration_t *vc,cf_configuration_t *dc,int sock,rline_t *tsd,cl_thread_t *thread)
#else
int flt_sorting_sorthread_t(cf_hash_t *head,cf_configuration_t *vc,cf_configuration_t *dc,void *shm_ptr,cl_thread_t *thread)
#endif
{
  if(flt_sorting_sormessage_ts != -1) {
    flt_sorting_sort_msgs(thread->ht);
    cf_msg_linearize(thread,thread->ht);
  }

  return FLT_OK;
}
/* }}} */

/* {{{ flt_sorting_cfg */
int flt_sorting_cfg(cf_configfile_t *cfile,cf_conf_opt_t *opt,const u_char *context,u_char **args,size_t argnum) {
  if(flt_sorting_fn == NULL) flt_sorting_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(flt_sorting_fn,context) != 0) return 0;

  if(cf_strcmp(opt->name,"SortThreads") == 0) {
    if(cf_strcmp(args[0],"ascending") == 0) flt_sorting_sorthread_ts = CF_SORT_ASCENDING;
    else if(cf_strcmp(args[0],"newestfirst") == 0) flt_sorting_sorthread_ts = CF_SORT_NEWESTFIRST;
    else flt_sorting_sorthread_ts = CF_SORT_DESCENDING;
  }
  else if(cf_strcmp(opt->name,"SortMessages") == 0) flt_sorting_sormessage_ts = cf_strcmp(args[0],"ascending") == 0 ? CF_SORT_ASCENDING : CF_SORT_DESCENDING;

  return 0;
}
/* }}} */

cf_conf_opt_t flt_sorting_config[] = {
  { "SortMessages", flt_sorting_cfg, CF_CFG_OPT_CONFIG|CF_CFG_OPT_USER|CF_CFG_OPT_LOCAL, NULL },
  { "SortThreads",  flt_sorting_cfg, CF_CFG_OPT_CONFIG|CF_CFG_OPT_USER|CF_CFG_OPT_LOCAL, NULL },
  { NULL, NULL, 0, NULL }
};

cf_handler_config_t flsorting_handler_ts[] = {
  { SORTING_HANDLER,        flt_sorting_sort },
  { THREAD_SORTING_HANDLER, flt_sorting_sorthread_t },
  { 0, NULL }
};

cf_module_config_t flt_sorting = {
  MODULE_MAGIC_COOKIE,
  flt_sorting_config,
  flsorting_handler_ts,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};

/* eof */
