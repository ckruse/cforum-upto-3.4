/**
 * \file flt_handle404.c
 * \author Christian Kruse, <cjk@wwwtech.de>
 *
 * This plug-in provides administrator functions
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <inttypes.h>

#include <sys/stat.h>
#include <unistd.h>

#include <db.h>

#include "hashlib.h"
#include "utils.h"
#include "cfgcomp.h"
#include "cfcgi.h"
#include "template.h"
#include "readline.h"
#include "clientlib.h"
/* }}} */

/* {{{ flt_handle404_gen_url */
void flt_handle404_gen_url(u_char *buff,u_char *aurl,u_char *url,u_int64_t tid,u_int64_t mid) {
  register u_char *ptr = buff,*ptr1;
  int slash = 1;
  u_char *server,*port;
  size_t len;

  server = getenv("SERVER_NAME");
  port   = getenv("SERVER_PORT");


  if(!port || cf_strcmp(port,"80") == 0) len = snprintf(ptr,256,"http://%s/",server);
  else len = snprintf(ptr,256,"http://%s:%s/",server,port);
  ptr += len;

  for(ptr1=aurl;*ptr1;ptr1++) {
    if(*ptr1 == '/') {
      if(slash == 0) {
        *ptr++ = '/';
      }
      slash++;
    }
    else {
      *ptr++ = *ptr1;
      slash = 0;
    }
  }

  if(slash == 0) {
    *ptr++ = '/';
    slash = 1;
  }

  /* ok, append normal URI */
  for(ptr1=url;*ptr1;ptr1++) {
    if(*ptr1 == '/') {
      if(slash == 0) {
        *ptr++ = '/';
      }
      slash++;
    }
    else {
      *ptr++ = *ptr1;
      slash = 0;
    }
  }

  if(slash == 0) *ptr++ = '/';

  /* now, append tid */
  len = sprintf(ptr,"t%"PRIu64"/",tid);
  ptr += len;

  if(mid) len = sprintf(ptr,"#m%"PRIu64,mid);
  ptr += len;

  *ptr = '\0';
}
/* }}} */

/* {{{ flt_handle404_execute */
int flt_handle404_execute(cf_hash_t *head,cf_configuration_t *dc,cf_configuration_t *vc,u_int64_t tid,u_int64_t mid) {
  u_char *forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  cf_name_value_t *v    = cf_cfg_get_first_value(&fo_default_conf,forum_name,"DF:ThreadIndexFile");
  cf_name_value_t *aurl = cf_cfg_get_first_value(&fo_default_conf,forum_name,"DF:ArchiveURL");
  struct stat st;
  DB *db;
  DBT key,data;
  u_char ctid[50];
  size_t len;
  int ret;
  u_char buff[256];

  if(stat(v->values[0],&st) == -1) return FLT_DECLINE;

  /* {{{ open database */
  if((ret = db_create(&db,NULL,0)) != 0) {
    fprintf(stderr,"flt_handle404: db_create() error: %s\n",db_strerror(ret));
    return FLT_DECLINE;
  }

  if((ret = db->open(db,NULL,v->values[0],NULL,DB_BTREE,DB_RDONLY,0)) != 0) {
    fprintf(stderr,"flt_handle404: db->open(%s) error: %s\n",v->values[0],db_strerror(ret));
    return FLT_DECLINE;
  }
  /* }}} */

  memset(&key,0,sizeof(key));
  memset(&data,0,sizeof(data));

  len = snprintf(ctid,50,"%"PRIu64,tid);
  key.data = ctid;
  key.size = len;

  if((ret = db->get(db,NULL,&key,&data,0)) != 0) {
    if(ret != DB_NOTFOUND) fprintf(stderr,"flt_handle404: db->get() error: %s\n",db_strerror(ret));

    db->close(db,0);
    return FLT_DECLINE;
  }

  flt_handle404_gen_url(buff,aurl->values[0],data.data,tid,mid);
  cf_http_redirect_with_nice_uri(buff,1);

  db->close(db,0);

  return FLT_EXIT;
}
/* }}} */

cf_conf_opt_t flt_handle404_config[] = {
  { NULL, NULL, 0, NULL }
};

cf_handler_config_t flt_handle404_handlers[] = {
  { HANDLE_404_HANDLER, flt_handle404_execute },
  { 0, NULL }
};

cf_module_config_t flt_handle404 = {
  MODULE_MAGIC_COOKIE,
  flt_handle404_config,
  flt_handle404_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};

/* eof */
