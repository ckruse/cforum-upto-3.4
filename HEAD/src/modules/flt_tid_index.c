/**
 * \file flt_tid_index.c
 * \author Christian Kruse
 *
 * This plugin indexes a thread id of a newly archived thread
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
#include <string.h>
#include <ctype.h>
#include <time.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <pthread.h>

#include "cf_pthread.h"

#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "readline.h"
#include "fo_server.h"
#include "serverlib.h"
#include "fo_tid_index.h"
/* }}} */

int cmp(const void *t,const void *elem) {
  const struct tm *tm = (const struct tm *)t;
  const t_tid_index *idx = (const t_tid_index *)elem;

  if(tm->tm_year + 1900 > idx->year) return 1;
  if(tm->tm_year + 1900 < idx->year) return -1;
  if(tm->tm_mon + 1 > idx->month) return 1;
  if(tm->tm_mon + 1 < idx->month) return -1;

  return 0;
}

int flt_tidx_module(t_thread *thr) {
  t_name_value *v = cfg_get_first_value(&fo_default_conf,"ThreadIndexFile");
  t_array index;
  FILE *fd;
  struct stat st;
  t_tid_index *idx;
  struct tm t;

  array_init(&index,sizeof(t_tid_index),NULL);

  if(!v) return FLT_DECLINE;
  if(stat(v->values[0],&st) == -1) return FLT_DECLINE;

  index.array    = fo_alloc(NULL,1,st.st_size,FO_ALLOC_MALLOC);
  index.reserved = st.st_size;
  index.elements = st.st_size / sizeof(t_tid_index);

  if((fd = fopen(v->values[0],"r")) == NULL) {
    free(index.array);
    return FLT_DECLINE;
  }
  fread(index.array,sizeof(t_tid_index),st.st_size/sizeof(t_tid_index),fd);
  fclose(fd);

  if(localtime_r(&thr->postings->date,&t) == NULL) {
    free(index.array);
    return FLT_DECLINE;
  }

  if((idx = array_bsearch(&index,&t,cmp)) == NULL) {
    t_tid_index id;

    id.year  = t.tm_year + 1900;
    id.month = t.tm_mon  + 1;

    id.start = thr->tid;
    id.end   = thr->tid;

    array_push(&index,&id);
  }
  else {
    if(thr->tid < idx->start)    idx->start = thr->tid;
    else if(thr->tid > idx->end) idx->end   = thr->tid;
  }

  if((fd = fopen(v->values[0],"w")) != 0) {
    free(index.array);
    return FLT_DECLINE;
  }
  fwrite(index.array,sizeof(t_tid_index),index.elements,fd);
  fclose(fd);

  return FLT_OK;
}


t_conf_opt flt_tid_index_config[] = {
  { NULL, NULL, NULL }
};

t_handler_config flt_tid_index_handlers[] = {
  { ARCHIVE_HANDLER,            flt_tidx_module   },
  { 0, NULL }
};

t_module_config flt_tid_index = {
  flt_tid_index_config,
  flt_tid_index_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
};

/* eof */

