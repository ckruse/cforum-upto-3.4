/**
 * \file flt_sorting.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 *
 * This plugin sorts messages and threads
 */

/* {{{ Initial comments */
/*
 * $LastChangedDate: 2005-01-11 07:32:07 +0100 (Di, 11 Jan 2005) $
 * $LastChangedRevision$
 * $LastChangedBy: ckruse $
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

#define CF_SORT_ASCENDING   0
#define CF_SORT_DESCENDING  1
#define CF_SORT_NEWESTFIRST 2

static int sort_threads  = CF_SORT_DESCENDING;
static int sort_messages = CF_SORT_DESCENDING;

int flt_sorting_threads_cmp(const void *a,const void *b) {
  t_cl_thread *ta = (t_cl_thread *)a;
  t_cl_thread *tb = (t_cl_thread *)b;

  if(sort_threads == CF_SORT_ASCENDING) {
    if(ta->messages->date > tb->messages->date) return 1;
    else if(ta->messages->date < tb->messages->date) return -1;
  }
  else if(sort_threads == CF_SORT_DESCENDING) {
    if(ta->messages->date > tb->messages->date) return -1;
    else if(ta->messages->date < tb->messages->date) return 1;
  }
  else {
    if(ta->newest->date > tb->newest->date) return -1;
    else if(ta->newest->date < tb->newest->date) return 1;
  }

  return 0;
}

void flt_sorting_sort_threads_newest(t_array *threads) {

}

void flt_sorting_sort_messages(t_array *threads) {
}

#ifndef CF_SHARED_MEM
int flt_sorting_sort(t_cf_hash *head,t_configuration *dc,t_configuration *vc,int sock,rline_t *tsd,t_array *threads)
#else
int flt_sorting_sort(t_cf_hash *head,t_configuration *dc,t_configuration *vc,void *ptr,t_array *threads)
#endif
{

  /* sort threads first */
  array_sort(threads,flt_sorting_threads_cmp);

  flt_sorting_sort_messages(threads);

  return FLT_OK;
}

/* {{{ flt_sorting_cfg */
int flt_sorting_cfg(t_configfile *cfile,t_conf_opt *opt,const u_char *context,u_char **args,size_t argnum) {
  if(cf_strcmp(opt->name,"SortThreads") == 0) {
    if(cf_strcmp(args[0],"ascending") == 0) sort_threads = CF_SORT_ASCENDING;
    else if(cf_strcmp(args[0],"newestfirst") == 0) sort_threads = CF_SORT_NEWESTFIRST;
    else sort_threads = CF_SORT_DESCENDING;
  }
  else if(cf_strcmp(opt->name,"SortMessages") == 0) sort_messages = cf_strcmp(args[0],"ascending") == 0 ? CF_SORT_ASCENDING : CF_SORT_DESCENDING;

  return 0;
}
/* }}} */

t_conf_opt flt_sorting_config[] = {
  { "SortMessages", flt_sorting_cfg, CFG_OPT_CONFIG|CFG_OPT_USER|CFG_OPT_LOCAL, NULL },
  { "SortThreads",  flt_sorting_cfg, CFG_OPT_CONFIG|CFG_OPT_USER|CFG_OPT_LOCAL, NULL },
  { NULL, NULL, 0, NULL }
};

t_handler_config flt_sorting_handlers[] = {
  { SORTING_HANDLER, flt_sorting_sort },
  { 0, NULL }
};

t_module_config flt_sorting = {
  flt_sorting_config,
  flt_sorting_handlers,
  NULL,
  NULL,
  NULL,
  NULL
};

/* eof */
