/**
 * \file flt_handle404.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
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

void flt_handle404_gen_url(u_char *buff,u_char *aurl,u_char *url,u_int64_t tid,u_int64_t mid) {
  register u_char *ptr = buff,*ptr1;
  int slash = 1;
  u_char *server,*port;
  size_t len;

  server = getenv("SERVER_NAME");
  port   = getenv("SERVER_NAME");


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
  len = sprintf(ptr,"t%llu/",tid);
  ptr += len;

  if(mid) len = sprintf(ptr,"#m%llu",mid);
  ptr += len;

  *ptr = '\0';
}

int flt_handle404_execute(t_cf_hash *head,t_configuration *dc,t_configuration *vc,u_int64_t tid,u_int64_t mid) {
  t_name_value *v    = cfg_get_first_value(&fo_default_conf,NULL,"ThreadIndexFile");
  t_name_value *aurl = cfg_get_first_value(&fo_default_conf,NULL,"ArchiveURL");
  struct stat st;
  u_char *port = getenv("SERVER_PORT");
  DB *db;
  DBT key,data;
  u_char ctid[50];
  size_t len;
  int ret;
  u_char buff[256];

  if(stat(v->values[0],&st) == -1) return FLT_DECLINE;

  /* {{{ open database */
  if((ret = db_create(&db,NULL,0)) != 0) {
    fprintf(stderr,"db_create() error: %s\n",db_strerror(ret));
    return FLT_DECLINE;
  }

  if((ret = db->open(db,NULL,v->values[0],NULL,DB_BTREE,DB_RDONLY,0)) != 0) {
    fprintf(stderr,"db->open(%s) error: %s\n",v->values[0],db_strerror(ret));
    return FLT_DECLINE;
  }
  /* }}} */

  memset(&key,0,sizeof(key));
  memset(&data,0,sizeof(data));

  len = snprintf(ctid,50,"%llu",tid);
  key.data = ctid;
  key.size = len;

  if((ret = db->get(db,NULL,&key,&data,0)) != 0) {
    if(ret != DB_NOTFOUND) {
      fprintf(stderr,"db->get() error: %s\n",db_strerror(ret));
    }

    db->close(db,0);
    return FLT_DECLINE;
  }

  printf("Status: 301 Moved Permanently\015\012");
  flt_handle404_gen_url(buff,aurl->values[0],data.data,tid,mid);

  printf("Location: %s\015\012\015\012",buff);

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
