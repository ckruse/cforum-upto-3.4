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
#include <sys/file.h>
#include <errno.h>

#include <db.h>

struct sockaddr_un;

#include "cf_pthread.h"

#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "readline.h"

#include "serverutils.h"
#include "serverlib.h"
#include "fo_server.h"
#include "fo_tid_index.h"
/* }}} */

/* {{{ flt_tidx_module */
int flt_tidx_module(forum_t *forum,thread_t *thr) {
  cf_name_value_t *v = cf_cfg_get_first_value(&fo_default_conf,forum->name,"ThreadIndexFile");
  struct stat st;
  struct tm t;
  DB *db;
  DBT key,data;
  int ret;
  cf_string_t str;
  u_char buff[256];
  u_char tid[50];
  size_t len,tlen;
  int fd;

  if(!v) return FLT_DECLINE;
  if(stat(v->values[0],&st) == -1) return FLT_DECLINE;
  if(localtime_r(&thr->postings->date,&t) == NULL) return FLT_DECLINE;

  /* {{{ open and lock database */
  if((ret = db_create(&db,NULL,0)) != 0) {
    cf_log(CF_ERR,__FILE__,__LINE__,"DB error: %s\n",db_strerror(ret));
    return FLT_DECLINE;
  }

  if((ret = db->open(db,NULL,v->values[0],NULL,DB_BTREE,DB_CREATE,0644)) != 0) {
    cf_log(CF_ERR,__FILE__,__LINE__,"DB error: %s\n",db_strerror(ret));
    return FLT_DECLINE;
  }

  if((ret = db->fd(db,&fd)) != 0) {
    cf_log(CF_ERR,__FILE__,__LINE__,"db->fd(): %s\n",db_strerror(ret));
    return FLT_DECLINE;
  }

  if((ret = flock(fd,LOCK_EX)) == -1) {
    cf_log(CF_ERR,__FILE__,__LINE__,"flock(): %s\n",strerror(errno));
    return FLT_DECLINE;
  }
  /* }}} */

  len  = snprintf(buff,256,"%d/%d",t.tm_year+1900,t.tm_mon+1);
  tlen = snprintf(tid,50,"%llu",thr->tid);

  memset(&key,0,sizeof(key));
  memset(&data,0,sizeof(data));

  key.data = tid;
  key.size = tlen;

  if((ret = db->get(db,NULL,&key,&data,0)) != 0) {
    if(ret != DB_NOTFOUND) {
      cf_log(CF_ERR,__FILE__,__LINE__,"DB error: %s\n",db_strerror(ret));
      db->close(db,0);
      return FLT_DECLINE;
    }

    memset(&data,0,sizeof(data));

    data.data = buff;
    data.size = len;

    if((ret = db->put(db,NULL,&key,&data,0)) != 0) {
      cf_log(CF_ERR,__FILE__,__LINE__,"DB error: %s\n",db_strerror(ret));
      db->close(db,0);
      return FLT_DECLINE;
    }
  }
  else {
    cf_str_init(&str);
    cf_str_chars_append(&str,data.data,data.size);
    cf_str_char_append(&str,'\0');
    cf_str_chars_append(&str,buff,len);

    memset(&data,0,sizeof(data));

    data.data = str.content;
    data.size = str.len;

    db->del(db,NULL,&key,0);
    if((ret = db->put(db,NULL,&key,&data,0)) != 0) {
      cf_log(CF_ERR,__FILE__,__LINE__,"DB error: %s\n",db_strerror(ret));
      db->close(db,0);
      return FLT_DECLINE;
    }

    cf_str_cleanup(&str);
  }

  db->close(db,0);

  return FLT_DECLINE;
}
/* }}} */

cf_conf_opt_t flt_tid_index_config[] = {
  { NULL, NULL, 0, NULL }
};

cf_handler_config_t flt_tid_index_handlers[] = {
  { ARCHIVE_HANDLER, flt_tidx_module },
  { 0, NULL }
};

cf_module_config_t flt_tid_index = {
  MODULE_MAGIC_COOKIE,
  flt_tid_index_config,
  flt_tid_index_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
};

/* eof */

