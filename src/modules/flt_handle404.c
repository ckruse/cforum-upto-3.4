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

#include <db.h>

#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "cfcgi.h"
#include "template.h"
#include "readline.h"
#include "clientlib.h"
/* }}} */

int flt_handle404_execute(t_cf_hash *head,t_configuration *dc,t_configuration *vc,u_int64_t tid,u_int64_t mid) {
  t_name_value *v    = cfg_get_first_value(&fo_default_conf,"ThreadIndexFile");
  t_name_value *aurl = cfg_get_first_value(&fo_default_conf,"ArchiveURL");
  struct stat st;
  u_char *port = getenv("SERVER_PORT");
  DB *db;
  DBT key,data;
  u_char ctid[50];
  size_t len;
  int ret;

  if(stat(v->values[0],&st) == -1) return FLT_DECLINE;

  /* {{{ open database */
  if((ret = db_create(&db,NULL,0)) != 0) {
    fprintf(stderr,"DB error: %s\n",db_strerror(ret));
    return FLT_DECLINE;
  }

  if((ret = db->open(db,NULL,v->values[0],NULL,DB_BTREE,DB_RDONLY,0)) != 0) {
    fprintf(stderr,"DB error: %s\n",db_strerror(ret));
    return FLT_DECLINE;
  }
  /* }}} */

  len = snprintf(ctid,50,"%llu",tid);
  key.data = ctid;
  key.size = len;

  if((ret = db->get(db,NULL,&key,&data,0)) != 0) {
    if(ret != DB_NOTFOUND) {
      fprintf(stderr,"DB error: %s\n",db_strerror(ret));
    }

    db->close(db,0);
    return FLT_DECLINE;
  }

  printf("Status: 301 Moved Permanently\015\012");

  if(!port || cf_strcmp(port,"80") == 0) {
    printf("Location: http://%s%s/%s/t%llu/#m%llu\015\012\015\012",getenv("SERVER_NAME"),aurl->values[0],data.data,tid,mid);
  }
  else {
    printf("Location: http://%s:%s%s/%s/t%llu/#m%llu\015\012\015\012",getenv("SERVER_NAME"),getenv("SERVER_PORT"),aurl->values[0],data.data,tid,mid);
  }

  db->close(db,0);

  return FLT_EXIT;
}

t_conf_opt flt_handle404_config[] = {
  { NULL, NULL, 0, NULL }
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
