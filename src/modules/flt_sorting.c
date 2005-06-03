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

static int flt_sorting_sort_threads  = -1;
static int flt_sorting_sort_messages = -1;

static u_char *flt_sorting_fn = NULL;

/* {{{ flt_sorting_threads_cmp */
int flt_sorting_threads_cmp(const void *a,const void *b) {
  t_cl_thread *ta = (t_cl_thread *)a;
  t_cl_thread *tb = (t_cl_thread *)b;

  if(flt_sorting_sort_threads == CF_SORT_ASCENDING) {
    if(ta->messages->date > tb->messages->date) return 1;
    else if(ta->messages->date < tb->messages->date) return -1;
  }
  else if(flt_sorting_sort_threads == CF_SORT_DESCENDING) {
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
  t_hierarchical_node *na = (t_hierarchical_node *)a;
  t_hierarchical_node *nb = (t_hierarchical_node *)b;

  if(flt_sorting_sort_messages == CF_SORT_ASCENDING) {
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
void flt_sorting_sort_msgs(t_hierarchical_node *h) {
  size_t i;

  array_sort(&h->childs,flt_sorting_msgs_cmp);
  for(i=0;i<h->childs.elements;++i) flt_sorting_sort_msgs(array_element_at(&h->childs,i));
}
/* }}} */

/* {{{ flt_sorting_sort */
#ifndef CF_SHARED_MEM
int flt_sorting_sort(t_cf_hash *head,t_configuration *dc,t_configuration *vc,int sock,rline_t *tsd,t_array *threads)
#else
int flt_sorting_sort(t_cf_hash *head,t_configuration *dc,t_configuration *vc,void *ptr,t_array *threads)
#endif
{
  size_t i;
  t_cl_thread *thr;

  /* sort threads first */
  if(flt_sorting_sort_threads != -1) array_sort(threads,flt_sorting_threads_cmp);

  if(flt_sorting_sort_messages != -1) {
    for(i=0;i<threads->elements;++i) {
      thr = array_element_at(threads,i);
      flt_sorting_sort_msgs(thr->ht);
      cf_msg_linearize(thr,thr->ht);
    }
  }

  return FLT_OK;
}
/* }}} */

/* {{{ flt_sorting_sort_thread */
#ifndef CF_SHARED_MEM
int flt_sorting_sort_thread(t_cf_hash *head,t_configuration *vc,t_configuration *dc,int sock,rline_t *tsd,t_cl_thread *thread)
#else
int flt_sorting_sort_thread(t_cf_hash *head,t_configuration *vc,t_configuration *dc,void *shm_ptr,t_cl_thread *thread)
#endif
{
  if(flt_sorting_sort_messages != -1) {
    flt_sorting_sort_msgs(thread->ht);
    cf_msg_linearize(thread,thread->ht);
  }

  return FLT_OK;
}
/* }}} */

/* {{{ flt_sorting_cfg */
int flt_sorting_cfg(t_configfile *cfile,t_conf_opt *opt,const u_char *context,u_char **args,size_t argnum) {
  if(flt_sorting_fn == NULL) flt_sorting_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(flt_sorting_fn,context) != 0) return 0;

  if(cf_strcmp(opt->name,"SortThreads") == 0) {
    if(cf_strcmp(args[0],"ascending") == 0) flt_sorting_sort_threads = CF_SORT_ASCENDING;
    else if(cf_strcmp(args[0],"newestfirst") == 0) flt_sorting_sort_threads = CF_SORT_NEWESTFIRST;
    else flt_sorting_sort_threads = CF_SORT_DESCENDING;
  }
  else if(cf_strcmp(opt->name,"SortMessages") == 0) flt_sorting_sort_messages = cf_strcmp(args[0],"ascending") == 0 ? CF_SORT_ASCENDING : CF_SORT_DESCENDING;

  return 0;
}
/* }}} */

t_conf_opt flt_sorting_config[] = {
  { "SortMessages", flt_sorting_cfg, CFG_OPT_CONFIG|CFG_OPT_USER|CFG_OPT_LOCAL, NULL },
  { "SortThreads",  flt_sorting_cfg, CFG_OPT_CONFIG|CFG_OPT_USER|CFG_OPT_LOCAL, NULL },
  { NULL, NULL, 0, NULL }
};

t_handler_config flt_sorting_handlers[] = {
  { SORTING_HANDLER,        flt_sorting_sort },
  { THREAD_SORTING_HANDLER, flt_sorting_sort_thread },
  { 0, NULL }
};

t_module_config flt_sorting = {
  MODULE_MAGIC_COOKIE,
  flt_sorting_config,
  flt_sorting_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};

/* eof */
