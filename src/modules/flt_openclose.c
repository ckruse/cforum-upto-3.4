/**
 * \file flt_openclose.c
 * \author Christian Kruse, <cjk@wwwtech.de>
 *
 * This plugin implements opened or closed threads
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
#include <time.h>
#include <sys/types.h>
#include <inttypes.h>

#include <sys/file.h>
#include <db.h>

#include "readline.h"
#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "cfcgi.h"
#include "template.h"
#include "clientlib.h"
#include "htmllib.h"
/* }}} */

static int ThreadsOpenByDefault = 1;
static int OpenThreadIfNew      = 0;
static int UseJavaScript        = 1;
static u_char *flt_oc_dbfile    = NULL;

static DB *flt_oc_db            = NULL;
static u_char *flt_oc_fn        = NULL;


/* {{{ flt_oc_opendb */
int flt_oc_opendb(cf_hash_t *cgi,configuration_t *dc,configuration_t *vc) {
  int ret,fd;

  if(flt_oc_dbfile) {
    if((ret = db_create(&flt_oc_db,NULL,0)) != 0) {
      fprintf(stderr,"flt_openclose: db_create() error: %s\n",db_strerror(ret));
      return FLT_EXIT;
    }

    if((ret = flt_oc_db->open(flt_oc_db,NULL,flt_oc_dbfile,NULL,DB_BTREE,DB_CREATE,0644)) != 0) {
      fprintf(stderr,"flt_openclose: db->open(%s) error: %s\n",flt_oc_dbfile,db_strerror(ret));
      return FLT_EXIT;
    }

    if((ret = flt_oc_db->fd(flt_oc_db,&fd)) != 0) {
      fprintf(stderr,"flt_openclose: db->fd() error: %s\n",db_strerror(ret));
      return FLT_EXIT;
    }

    if((ret = flock(fd,LOCK_EX)) != 0) {
      fprintf(stderr,"flt_openclose: flock() error: %s\n",strerror(ret));
      return FLT_EXIT;
    }

    return FLT_OK;
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_oc_exec_xmlhttp */
#ifndef CF_SHARED_MEM
int flt_oc_exec_xmlhttp(cf_hash_t *cgi,configuration_t *dc,configuration_t *vc,int sock)
#else
int flt_oc_exec_xmlhttp(cf_hash_t *cgi,configuration_t *dc,configuration_t *vc,void *shm)
#endif
{
  u_char *val,buff[512];
  u_int64_t tid;
  int ret;
  char one[] = "1";
  size_t len;

  DBT key,data;

  if(cgi == NULL || flt_oc_dbfile == NULL) return FLT_DECLINE;

  if((val = cf_cgi_get(cgi,"a")) != NULL && (cf_strcmp(val,"open") == 0 || cf_strcmp(val,"close") == 0)) {
    if((val = cf_cgi_get(cgi,"oc_t")) != NULL && (tid = str_to_u_int64(val)) != 0) {
      /* {{{ put tid to database or remove it from database */
      len = snprintf(buff,512,"%"PRIu64,tid);

      memset(&key,0,sizeof(key));
      memset(&data,0,sizeof(data));

      key.data = buff;
      key.size = len;

      if((ret = flt_oc_db->get(flt_oc_db,NULL,&key,&data,0)) != 0) {
        if(ret == DB_NOTFOUND) {
          memset(&data,0,sizeof(data));
          data.data = one;
          data.size = sizeof(one);

          if((ret = flt_oc_db->put(flt_oc_db,NULL,&key,&data,0)) != 0) fprintf(stderr,"flt_openclose: db->put() error: %s\n",db_strerror(ret));
        }
      }
      else flt_oc_db->del(flt_oc_db,NULL,&key,0);
      /* }}} */
    }
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_oc_execute_filter */
int flt_oc_execute_filter(cf_hash_t *head,configuration_t *dc,configuration_t *vc,cl_thread_t *thread,int mode) {
  u_char *UserName = cf_hash_get(GlobalValues,"UserName",8);
  u_char buff[512];
  size_t i;
  name_value_t *vs;
  message_t *msg;
  mod_api_t is_visited;

  DBT key,data;

  if(mode & CF_MODE_PRE) return FLT_DECLINE;
  if(mode & CF_MODE_THREADVIEW) return FLT_DECLINE;
  if(flt_oc_dbfile == NULL) return FLT_DECLINE;
  if(flt_oc_fn == NULL) flt_oc_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);

  vs = cfg_get_first_value(dc,flt_oc_fn,UserName ? "UBaseURL" : "BaseURL");
  cf_tpl_hashvar_setvalue(&thread->messages->hashvar,"openclose",TPL_VARIABLE_INT,1);

  i = snprintf(buff,512,"t%"PRIu64,thread->tid);
  cf_tpl_hashvar_setvalue(&thread->messages->hashvar,"unanch",TPL_VARIABLE_STRING,buff,i);

  /* user wants to use java script */
  if(UseJavaScript) cf_tpl_hashvar_setvalue(&thread->messages->hashvar,"UseJavaScript",TPL_VARIABLE_INT,1);

  if(ThreadsOpenByDefault == 0) {
    i = snprintf(buff,512,"%"PRIu64,thread->tid);

    memset(&key,0,sizeof(key));
    memset(&data,0,sizeof(data));

    key.data = buff;
    key.size = i;

    if(flt_oc_db->get(flt_oc_db,NULL,&key,&data,0) == 0) {
      cf_tpl_hashvar_setvalue(&thread->messages->hashvar,"open",TPL_VARIABLE_INT,1);
      i = snprintf(buff,512,"%s?oc_t=%"PRIu64"&a=close",vs->values[0],thread->tid);
      cf_tpl_hashvar_setvalue(&thread->messages->hashvar,"link_oc",TPL_VARIABLE_STRING,buff,i);

      return FLT_DECLINE; /* thread is open */
    }

    /* shall we close threads? Other filters can tell us not to do
     * this.
     */
    if(cf_hash_get(GlobalValues,"openclose",9) != NULL) return FLT_DECLINE;

    /* Ok, thread should normaly be closed. But lets check for new posts */
    if(OpenThreadIfNew) {
      if((is_visited = cf_get_mod_api_ent("is_visited")) != NULL) {
        for(msg=thread->messages;msg;msg=msg->next) {
          /* Thread has at least one not yet visited messages -- leave it open */
          if(is_visited(&(msg->mid)) == NULL && msg->invisible == 0 && msg->may_show == 1) {
            cf_tpl_hashvar_setvalue(&thread->messages->hashvar,"open",TPL_VARIABLE_INT,1);
            i = snprintf(buff,500,"%s?oc_t=%"PRIu64"&a=close",vs->values[0],thread->tid);
            cf_tpl_hashvar_setvalue(&thread->messages->hashvar,"link_oc",TPL_VARIABLE_STRING,buff,i);

            return FLT_DECLINE;
          }
        }
      }
    }

    i = snprintf(buff,512,"%s?oc_t=%"PRIu64"&a=open",vs->values[0],thread->tid);
    cf_tpl_hashvar_setvalue(&thread->messages->hashvar,"link_oc",TPL_VARIABLE_STRING,buff,i,1);

    cf_msg_delete_subtree(thread->messages);
  }
  else {
    /* check, if the actual thread is in the closed threads list */
    i = snprintf(buff,512,"%"PRIu64,thread->tid);

    memset(&key,0,sizeof(key));
    memset(&data,0,sizeof(data));

    key.data = buff;
    key.size = i;

    if(flt_oc_db->get(flt_oc_db,NULL,&key,&data,0) == 0) {
      i = snprintf(buff,512,"%s?oc_t=%"PRIu64"&a=open",vs->values[0],thread->tid);
      cf_tpl_hashvar_setvalue(&thread->messages->hashvar,"link_oc",TPL_VARIABLE_STRING,buff,i,1);
      cf_msg_delete_subtree(thread->messages);
      return FLT_DECLINE; /* thread is closed */
    }

    /* this thread must be open */
    cf_tpl_hashvar_setvalue(&thread->messages->hashvar,"open",TPL_VARIABLE_INT,1);
    i = snprintf(buff,512,"%s?oc_t=%"PRIu64"&a=close",vs->values[0],thread->tid);
    cf_tpl_hashvar_setvalue(&thread->messages->hashvar,"link_oc",TPL_VARIABLE_STRING,buff,i);
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_oc_set_js */
int flt_oc_set_js(cf_hash_t *head,configuration_t *dc,configuration_t *vc,cf_template_t *begin,cf_template_t *end) {
  /* user wants to use java script */
  if(UseJavaScript) {
    cf_tpl_setvalue(begin,"UseJavaScript",TPL_VARIABLE_INT,1);
    if(ThreadsOpenByDefault) cf_tpl_setvalue(begin,"ThreadsOpenByDefault",TPL_VARIABLE_INT,1);
    return FLT_OK;
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_oc_validate */
#ifndef CF_SHARED_MEM
int flt_oc_validate(cf_hash_t *cgi,configuration_t *dc,configuration_t *vc,time_t last_modified,int sock)
#else
int flt_oc_validate(cf_hash_t *cgi,configuration_t *dc,configuration_t *vc,time_t last_modified,void *sock)
#endif
{
  u_char *val;

  if(cgi) {
    if((val = cf_cgi_get(cgi,"a")) != NULL) {
      if((val = cf_cgi_get(cgi,"oc_t")) != NULL) {
        if(str_to_u_int64(val) != 0) return FLT_EXIT;
      }
    }
  }

  return FLT_OK;
}
/* }}} */


/* {{{ flt_oc_get_conf */
int flt_oc_get_conf(configfile_t *cfile,conf_opt_t *opt,const u_char *context,u_char **args,size_t argnum) {
  if(flt_oc_fn == NULL) flt_oc_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(flt_oc_fn,context) != 0) return 0;

  if(cf_strcmp(opt->name,"ThreadsOpenByDefault") == 0) ThreadsOpenByDefault = cf_strcmp(args[0],"yes") == 0;
  else if(cf_strcmp(opt->name,"OpenThreadIfNew") == 0) OpenThreadIfNew = cf_strcmp(args[0],"yes") == 0;
  else if(cf_strcmp(opt->name,"UseJavaScript") == 0) UseJavaScript = cf_strcmp(args[0],"yes") == 0;
  else if(cf_strcmp(opt->name,"OcDbFile") == 0) flt_oc_dbfile = strdup(args[0]);

  return 0;
}
/* }}} */

/* {{{ flt_oc_cleanup */
void flt_oc_cleanup(void) {
  if(flt_oc_db) flt_oc_db->close(flt_oc_db,0);
  if(flt_oc_dbfile) free(flt_oc_dbfile);
}
/* }}} */

conf_opt_t flt_openclose_config[] = {
  { "ThreadsOpenByDefault", flt_oc_get_conf, CFG_OPT_CONFIG|CFG_OPT_USER|CFG_OPT_LOCAL, NULL },
  { "UseJavaScript",        flt_oc_get_conf, CFG_OPT_CONFIG|CFG_OPT_USER|CFG_OPT_LOCAL, NULL },
  { "OpenThreadIfNew",      flt_oc_get_conf, CFG_OPT_CONFIG|CFG_OPT_USER|CFG_OPT_LOCAL, NULL },
  { "OcDbFile",             flt_oc_get_conf, CFG_OPT_USER|CFG_OPT_LOCAL, NULL },
  { NULL, NULL, 0, NULL }
};

handler_config_t flt_openclose_handlers[] = {
  { INIT_HANDLER,         flt_oc_opendb },
  { CONNECT_INIT_HANDLER, flt_oc_exec_xmlhttp },
  { VIEW_HANDLER,         flt_oc_execute_filter },
  { VIEW_INIT_HANDLER,    flt_oc_set_js  },
  { 0, NULL }
};

module_config_t flt_openclose = {
  MODULE_MAGIC_COOKIE,
  flt_openclose_config,
  flt_openclose_handlers,
  NULL,
  flt_oc_validate,
  NULL,
  NULL,
  flt_oc_cleanup
};

/* eof */
