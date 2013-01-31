/**
 * \file flt_moderated.c
 * \author Christian Kruse, <cjk@wwwtech.de>
 *
 * This plugin gives the user the ability to moderate the forum. Therefore,
 * every posting has to be approved.
 */

/* {{{ Initial comments */
/*
 * $LastChangedDate: 2009-01-16 14:32:24 +0100 (Fri, 16 Jan 2009) $
 * $LastChangedRevision: 1639 $
 * $LastChangedBy: ckruse $
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
#include <fcntl.h>
#include <unistd.h>

#include <sys/file.h>

#include <db.h>

#include "readline.h"
#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "cfcgi.h"
#include "template.h"
#include "clientlib.h"
/* }}} */

#define FLT_MOD_THREAD 0x1
#define FLT_MOD_POSTS  0x2

static int flt_moderated_cfg        = 0;
static u_char *flt_moderated_dbname = NULL;

static u_char *flt_modated_fn = NULL;
static DB *flt_moderated_db   = NULL;


/* {{{ flt_mod_getdb */
int flt_mod_getdb(int read) {
  int ret,fd;

  if(flt_moderated_dbname) {
    if((ret = db_create(&flt_moderated_db,NULL,0)) != 0) {
      fprintf(stderr,"flt_moderated: db_create() error: %s\n",db_strerror(ret));
      return FLT_EXIT;
    }

    if((ret = flt_moderated_db->open(flt_moderated_db,NULL,flt_moderated_dbname,NULL,DB_BTREE,read?DB_RDONLY:DB_CREATE,0644)) != 0) {
      flt_moderated_db->close(flt_moderated_db,0);
      flt_moderated_db = NULL;
      fprintf(stderr,"flt_moderated: db->open(%s) error: %s\n",flt_moderated_dbname,db_strerror(ret));
      return FLT_EXIT;
    }

    if((ret = flt_moderated_db->fd(flt_moderated_db,&fd)) != 0) {
      flt_moderated_db->close(flt_moderated_db,0);
      flt_moderated_db = NULL;
      fprintf(stderr,"flt_moderated: db->fd() error: %s\n",db_strerror(ret));
      return FLT_EXIT;
    }

    if((ret = flock(fd,read?LOCK_SH:LOCK_EX)) != 0) {
      flt_moderated_db->close(flt_moderated_db,0);
      flt_moderated_db = NULL;
      fprintf(stderr,"flt_moderated: flock() error: %s\n",strerror(ret));
      return FLT_EXIT;
    }

    return FLT_OK;
  }

  return FLT_EXIT;
}
/* }}} */

/* {{{ flt_mod_closedb */
int flt_mod_closedb(void) {
  if(flt_moderated_db) {
    flt_moderated_db->close(flt_moderated_db,0);
    flt_moderated_db = NULL;
  }
  return FLT_OK;
}
/* }}} */

/* {{{ flt_mod_setlinks */
void flt_mod_setlinks(cf_tpl_variable_t *tpl,int ret,u_int64_t tid,u_int64_t mid) {
  cf_readmode_t *rm = cf_hash_get(GlobalValues,"RM",2);
  u_char *link;
  size_t l;

  if(ret == 0) {
    link = cf_advanced_get_link(rm->posting_uri[1],tid,mid,NULL,1,&l,"a","unapprove");
    cf_tpl_hashvar_setvalue(tpl,"unapprove_link",TPL_VARIABLE_STRING,link,l);
    free(link);
  }
  else {
    link = cf_advanced_get_link(rm->posting_uri[1],tid,mid,NULL,1,&l,"a","approve");
    cf_tpl_hashvar_setvalue(tpl,"approve_link",TPL_VARIABLE_STRING,link,l);
    free(link);
  }
}
/* }}} */

/* {{{ flt_moderated_thread */
int flt_moderated_thread(cf_hash_t *head,configuration_t *dc,configuration_t *vc,cl_thread_t *thread,int mode) {
  int si = cf_hash_get(GlobalValues,"ShowInvisible",13) != NULL,ret;

  DBT key,data;
  string_t str;

  if(flt_moderated_cfg != FLT_MOD_THREAD || (mode & CF_MODE_POST) == 0) return FLT_DECLINE;
  if(flt_moderated_db == NULL) {
    if(flt_mod_getdb(1) != FLT_OK) {
      if(si) flt_mod_setlinks(&thread->messages->hashvar,1,thread->tid,thread->messages->mid);
      thread->messages->may_show = 0;
      cf_msg_delete_subtree(thread->messages);
      return FLT_DECLINE;
    }
  }

  str_init_growth(&str,50);
  str_char_append(&str,'t');
  u_int64_to_str(&str,thread->tid);
  str_char_append(&str,'m');
  u_int64_to_str(&str,thread->messages->mid);

  memset(&key,0,sizeof(key));
  memset(&data,0,sizeof(data));

  key.data = str.content;
  key.size = str.len;

  ret = flt_moderated_db->get(flt_moderated_db,NULL,&key,&data,0);
  if(si) flt_mod_setlinks(&thread->messages->hashvar,ret,thread->tid,thread->messages->mid);

  if(ret != 0) {
    thread->messages->may_show = 0;
    cf_msg_delete_subtree(thread->messages);
  }

  return FLT_OK;
}
/* }}} */

/* {{{ flt_moderated_posthandler */
int flt_moderated_posthandler(cf_hash_t *cgi,configuration_t *dc,configuration_t *vc,message_t *msg,u_int64_t tid,int mode) {
  int si = cf_hash_get(GlobalValues,"ShowInvisible",13) != NULL,ret;

  DBT key,data;
  string_t str;

  if(flt_moderated_cfg != FLT_MOD_POSTS) return FLT_DECLINE;

  if(flt_moderated_db == NULL) {
    if(flt_mod_getdb(1) != FLT_OK) {
      if(si) flt_mod_setlinks(&msg->hashvar,1,tid,msg->mid);
      msg->may_show = 0;
      return FLT_DECLINE;
    }
  }

  str_init_growth(&str,50);
  str_char_append(&str,'t');
  u_int64_to_str(&str,tid);
  str_char_append(&str,'m');
  u_int64_to_str(&str,msg->mid);

  memset(&key,0,sizeof(key));
  memset(&data,0,sizeof(data));

  key.data = str.content;
  key.size = str.len;

  ret = flt_moderated_db->get(flt_moderated_db,NULL,&key,&data,0);

  if(si) flt_mod_setlinks(&msg->hashvar,ret,tid,msg->mid);

  if(ret != 0) msg->may_show = 0;

  str_cleanup(&str);

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_moderated_gogogo */
#ifndef CF_SHARED_MEM
int flt_moderated_gogogo(cf_hash_t *cgi,configuration_t *dc,configuration_t *vc,int sock)
#else
int flt_moderated_gogogo(cf_hash_t *cgi,configuration_t *dc,configuration_t *vc,void *ptr)
#endif
{
  u_char *action = NULL,*tid,*mid;
  int si = cf_hash_get(GlobalValues,"ShowInvisible",13) != NULL,ret;
  string_t str;
  DBT key,data;
  u_int64_t itid,imid;

  u_char one[] = "1";

  if(si == 0 || cgi == NULL) return FLT_DECLINE;

  if((action = cf_cgi_get(cgi,"a")) != NULL && (cf_strcmp(action,"approve") == 0 || cf_strcmp(action,"unapprove") == 0)) {
    tid = cf_cgi_get(cgi,"t");
    mid = cf_cgi_get(cgi,"m");

    if(!tid || !mid) return FLT_DECLINE;
    if((itid = str_to_u_int64(tid)) == 0 || (imid = str_to_u_int64(mid)) == 0) return FLT_DECLINE;

    str_init_growth(&str,50);
    str_char_append(&str,'t');
    u_int64_to_str(&str,itid);
    str_char_append(&str,'m');
    u_int64_to_str(&str,imid);

    memset(&key,0,sizeof(key));
    memset(&data,0,sizeof(data));

    /* {{{ approve */
    if(cf_strcmp(action,"approve") == 0) {
      if(flt_mod_getdb(0) == FLT_EXIT) return FLT_DECLINE;

      key.data = str.content;
      key.size = str.len;

      data.data = one;
      data.size = sizeof(one);

      if((ret = flt_moderated_db->put(flt_moderated_db,NULL,&key,&data,0)) != 0) fprintf(stderr,"flt_moderated: db->put() error: %s\n",db_strerror(ret));

      flt_mod_closedb();
    }
    /* }}} */
    /* {{{ unapprove */
    else if(cf_strcmp(action,"unapprove") == 0) {
      if(flt_mod_getdb(0) == FLT_EXIT) return FLT_DECLINE;

      key.data = str.content;
      key.size = str.len;

      flt_moderated_db->del(flt_moderated_db,NULL,&key,0);

      flt_mod_closedb();
    }
    /* }}} */

    str_cleanup(&str);

    cf_hash_entry_delete(cgi,"t",1);
    cf_hash_entry_delete(cgi,"m",1);

    return FLT_OK;
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_moderated_handle */
int flt_modated_handle(configfile_t *cfile,conf_opt_t *opt,const u_char *context,u_char **args,size_t argnum) {
  if(flt_modated_fn == NULL) flt_modated_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(flt_modated_fn,context) != 0) return 0;

  if(cf_strcmp(opt->name,"Moderation") == 0) {
    if(cf_strcmp(args[0],"threads") == 0) flt_moderated_cfg =  FLT_MOD_THREAD;
    else if(cf_strcmp(args[0],"posts") == 0) flt_moderated_cfg = FLT_MOD_POSTS;
  }
  else flt_moderated_dbname = strdup(args[0]);

  return 0;
}
/* }}} */

void flt_moderated_cleanup(void) {
  if(flt_moderated_db) flt_moderated_db->close(flt_moderated_db,0);
  if(flt_moderated_dbname) free(flt_moderated_dbname);
}

conf_opt_t flt_moderated_config[] = {
  { "Moderation",   flt_modated_handle, CFG_OPT_CONFIG|CFG_OPT_LOCAL, NULL },
  { "ModerationDB", flt_modated_handle, CFG_OPT_CONFIG|CFG_OPT_LOCAL, NULL },
  { NULL, NULL, 0, NULL }
};

handler_config_t flt_moderated_handlers[] = {
  { CONNECT_INIT_HANDLER, flt_moderated_gogogo },
  { VIEW_LIST_HANDLER,    flt_moderated_posthandler },
  { VIEW_HANDLER,         flt_moderated_thread },
  { 0, NULL }
};

module_config_t flt_moderated = {
  MODULE_MAGIC_COOKIE,
  flt_moderated_config,
  flt_moderated_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  flt_moderated_cleanup
};

/* eof */
