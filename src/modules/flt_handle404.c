/**
 * \file flt_handle404.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 *
 * This plug-in provides administrator functions
 */

/* {{{ Initial comments */
/*
 * $LastChangedDate: 2004-06-09 15:55:53 +0200 (Wed, 09 Jun 2004) $
 * $LastChangedRevision: 106 $
 * $LastChangedBy: cseiler $
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

#include <sys/stat.h>
#include <unistd.h>

#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "cfcgi.h"
#include "template.h"
#include "readline.h"
#include "clientlib.h"
#include "fo_tid_index.h"
/* }}} */

/* {{{ flt_handle404_cmp */
int flt_handle404_cmp(const void *elem1,const void *elem2) {
  u_int64_t *tid = (u_int64_t *)elem1;
  t_tid_index *id = (t_tid_index *)elem2;

  if(*tid >= id->start && *tid <= id->end) return 0;
  if(*tid < id->start) return -1;

  return 1;
}
/* }}} */

int flt_handle404_execute(t_cf_hash *head,t_configuration *dc,t_configuration *vc,u_int64_t tid,u_int64_t mid) {
  t_name_value *v    = cfg_get_first_value(&fo_default_conf,"ThreadIndexFile");
  t_name_value *aurl = cfg_get_first_value(&fo_default_conf,"ArchiveURL");
  t_array ary;
  struct stat st;
  FILE *fd;
  u_char *port = getenv("SERVER_PORT");
  t_tid_index *idx;

  if(stat(v->values[0],&st) == -1) return FLT_DECLINE;
  if((fd = fopen(v->values[0],"r")) == NULL) return FLT_DECLINE;

  array_init(&ary,sizeof(t_tid_index),NULL);
  ary.array    = fo_alloc(NULL,1,st.st_size,FO_ALLOC_MALLOC);
  ary.reserved = st.st_size;
  ary.elements = st.st_size / sizeof(t_tid_index);

  fread(ary.array,sizeof(t_tid_index),st.st_size/sizeof(t_tid_index),fd);
  fclose(fd);

  if((idx = array_bsearch(&ary,(void *)&tid,flt_handle404_cmp)) == NULL) return FLT_DECLINE;

  printf("Status: 301 Moved Permanently\015\012");

  if(!port || cf_strcmp(port,"80") == 0) {
    printf("Location: http://%s%s%d/%d/t%llu/#m%llu\015\012\015\012",getenv("SERVER_NAME"),aurl->values[0],idx->year,idx->month,tid,mid);
  }
  else {
    printf("Location: http://%s:%s%s%d/%d/t%llu/#m%llu\015\012\015\012",getenv("SERVER_NAME"),getenv("SERVER_PORT"),aurl->values[0],idx->year,idx->month,tid,mid);
  }

  return FLT_EXIT;
}

t_conf_opt flt_handle404_config[] = {
  { NULL, NULL, NULL }
};

t_handler_config flt_handle404_handlers[] = {
  { HANDLE_404_HANDLER, flt_handle404_execute },
  { 0, NULL }
};

t_module_config flt_handle404 = {
  flt_handle404_config,
  flt_handle404_handlers,
  NULL,
  NULL,
  NULL,
  NULL
};

/* eof */
