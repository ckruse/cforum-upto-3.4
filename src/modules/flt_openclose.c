/**
 * \file flt_openclose.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
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
int flt_oc_opendb(t_cf_hash *cgi,t_configuration *dc,t_configuration *vc) {
  int ret,fd;

  if(flt_oc_dbfile) {
    if((ret = db_create(&flt_oc_db,NULL,0)) != 0) {
      fprintf(stderr,"DB error: %s\n",db_strerror(ret));
      return FLT_EXIT;
    }

    if((ret = flt_oc_db->open(flt_oc_db,NULL,flt_oc_dbfile,NULL,DB_BTREE,DB_CREATE,0644)) != 0) {
      fprintf(stderr,"DB error: %s\n",db_strerror(ret));
      return FLT_EXIT;
    }

    if((ret = flt_oc_db->fd(flt_oc_db,&fd)) != 0) {
      fprintf(stderr,"DB error: %s\n",db_strerror(ret));
      return FLT_EXIT;
    }

    if((ret = flock(fd,LOCK_EX)) != 0) {
      fprintf(stderr,"DB error: %s\n",db_strerror(ret));
      return FLT_EXIT;
    }

    return FLT_OK;
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_oc_exec_xmlhttp */
#ifndef CF_SHARED_MEM
int flt_oc_exec_xmlhttp(t_cf_hash *cgi,t_configuration *dc,t_configuration *vc,int sock)
#else
int flt_oc_exec_xmlhttp(t_cf_hash *cgi,t_configuration *dc,t_configuration *vc,void *shm)
#endif
{
  u_char *val,*fn = cf_hash_get(GlobalValues,"FORUM_NAME",10),buff[512],fo_thread_tplname[256];
  u_int64_t tid;
  t_cl_thread thread;
  size_t len;
  t_string str;
  rline_t rl;
  int ret;
  char one[] = "1";

  DBT key,data;

  t_name_value *fo_thread_tpl,*cs,*ot,*ct;

  if(cgi == NULL || flt_oc_dbfile == NULL) return FLT_DECLINE;

  if((val = cf_cgi_get(cgi,"a")) != NULL && (cf_strcmp(val,"open") == 0 || cf_strcmp(val,"close") == 0)) {
    if((val = cf_cgi_get(cgi,"oc_t")) != NULL && (tid = str_to_u_int64(val)) != 0) {
      /* {{{ put tid to database or remove it from database */
      len = snprintf(buff,512,"%llu",tid);

      memset(&key,0,sizeof(key));
      memset(&data,0,sizeof(data));

      key.data = buff;
      key.size = len;

      if((ret = flt_oc_db->get(flt_oc_db,NULL,&key,&data,0)) != 0) {
        if(ret == DB_NOTFOUND) {
          memset(&data,0,sizeof(data));
          data.data = one;
          data.size = sizeof(one);

          if((ret = flt_oc_db->put(flt_oc_db,NULL,&key,&data,0)) != 0) fprintf(stderr,"db->put(): %s\n",db_strerror(ret));
        }
      }
      else flt_oc_db->del(flt_oc_db,NULL,&key,0);
      /* }}} */

      /* {{{ xmlhttp mode */
      if((val = cf_cgi_get(cgi,"mode")) != NULL && cf_strcmp(val,"xmlhttp") == 0) {
        /* {{{ init variables to get content */
        fo_thread_tpl  = cfg_get_first_value(vc,fn,"TemplateForumThread");
        cs             = cfg_get_first_value(dc,fn,"ExternCharset");
        ot             = cfg_get_first_value(vc,fn,"OpenThread");
        ct             = cfg_get_first_value(vc,fn,"CloseThread");

        cf_gen_tpl_name(fo_thread_tplname,256,fo_thread_tpl->values[0]);

        memset(&thread,0,sizeof(thread));
        memset(&rl,0,sizeof(rl));
        /* }}} */

        #ifndef CF_SHARED_MEM
        if(cf_get_message_through_sock(sock,&tsd,&thread,fo_thread_tplname,tid,0,CF_KILL_DELETED) == -1)
        #else
        if(cf_get_message_through_shm(shm,&thread,fo_thread_tplname,tid,0,CF_KILL_DELETED) == -1)
        #endif
          fprintf(stderr,"500 Internal Server Error\015\012\015\012");

        thread.threadmsg = thread.messages;
        #ifndef CF_NO_SORTING
        #ifdef CF_SHARED_MEM
        cf_run_thread_sorting_handlers(cgi,shm,&thread);
        #else
        cf_run_thread_sorting_handlers(cgi,sock,&rl,&thread);
        #endif
        #endif

        str_init(&str);
        cf_gen_threadlist(&thread,cgi,&str,"full",NULL,CF_MODE_THREADLIST);
        cf_cleanup_thread(&thread);

        printf("Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
        fwrite(str.content + strlen(ot->values[0]),1,str.len - strlen(ot->values[0]) - strlen(ct->values[0]),stdout);
        str_cleanup(&str);

        return FLT_EXIT;
      }
      /* }}} */
    }
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_oc_execute_filter */
int flt_oc_execute_filter(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_cl_thread *thread,int mode) {
  u_char *UserName = cf_hash_get(GlobalValues,"UserName",8);
  u_char buff[512];
  size_t i;
  t_name_value *vs;
  t_message *msg;
  t_mod_api is_visited;

  DBT key,data;

  if(mode & CF_MODE_PRE) return FLT_DECLINE;
  if(mode & CF_MODE_THREADVIEW) return FLT_DECLINE;
  if(flt_oc_dbfile == NULL) return FLT_DECLINE;
  if(flt_oc_fn == NULL) flt_oc_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);

  vs = cfg_get_first_value(dc,flt_oc_fn,UserName ? "UBaseURL" : "BaseURL");
  cf_tpl_setvalue(&thread->messages->tpl,"openclose",TPL_VARIABLE_INT,1);

  i = snprintf(buff,512,"t%llu",thread->tid);
  cf_tpl_setvalue(&thread->messages->tpl,"unanch",TPL_VARIABLE_STRING,buff,i);

  /* user wants to use java script */
  if(UseJavaScript) cf_tpl_setvalue(&thread->messages->tpl,"UseJavaScript",TPL_VARIABLE_INT,1);

  if(ThreadsOpenByDefault == 0) {
    i = snprintf(buff,512,"%llu",thread->tid);

    memset(&key,0,sizeof(key));
    memset(&data,0,sizeof(data));

    key.data = buff;
    key.size = i;

    if(flt_oc_db->get(flt_oc_db,NULL,&key,&data,0) == 0) {
      cf_tpl_setvalue(&thread->messages->tpl,"open",TPL_VARIABLE_INT,1);
      i = snprintf(buff,512,"%s?oc_t=%lld&a=close",vs->values[0],thread->tid);
      cf_tpl_setvalue(&thread->messages->tpl,"link_oc",TPL_VARIABLE_STRING,buff,i);

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
            cf_tpl_setvalue(&thread->messages->tpl,"open",TPL_VARIABLE_INT,1);
            i = snprintf(buff,500,"%s?oc_t=%lld&a=close",vs->values[0],thread->tid);
            cf_tpl_setvalue(&thread->messages->tpl,"link_oc",TPL_VARIABLE_STRING,buff,i);

            return FLT_DECLINE;
          }
        }
      }
    }

    i = snprintf(buff,512,"%s?oc_t=%lld&a=open",vs->values[0],thread->tid);
    cf_tpl_setvalue(&thread->messages->tpl,"link_oc",TPL_VARIABLE_STRING,buff,i,1);

    cf_msg_delete_subtree(thread->messages);
  }
  else {
    /* check, if the actual thread is in the closed threads list */
    i = snprintf(buff,512,"%llu",thread->tid);

    memset(&key,0,sizeof(key));
    memset(&data,0,sizeof(data));

    key.data = buff;
    key.size = i;

    if(flt_oc_db->get(flt_oc_db,NULL,&key,&data,0) == 0) {
      i = snprintf(buff,512,"%s?oc_t=%lld&a=open",vs->values[0],thread->tid);
      cf_tpl_setvalue(&thread->messages->tpl,"link_oc",TPL_VARIABLE_STRING,buff,i,1);
      cf_msg_delete_subtree(thread->messages);
      return FLT_DECLINE; /* thread is closed */
    }

    /* this thread must be open */
    cf_tpl_setvalue(&thread->messages->tpl,"open",TPL_VARIABLE_INT,1);
    i = snprintf(buff,512,"%s?oc_t=%lld&a=close",vs->values[0],thread->tid);
    cf_tpl_setvalue(&thread->messages->tpl,"link_oc",TPL_VARIABLE_STRING,buff,i);
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_oc_set_js */
int flt_oc_set_js(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_cf_template *begin,t_cf_template *end) {
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
int flt_oc_validate(t_cf_hash *cgi,t_configuration *dc,t_configuration *vc,time_t last_modified,int sock)
#else
int flt_oc_validate(t_cf_hash *cgi,t_configuration *dc,t_configuration *vc,time_t last_modified,void *sock)
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
int flt_oc_get_conf(t_configfile *cfile,t_conf_opt *opt,const u_char *context,u_char **args,size_t argnum) {
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
  int fd;
  long i;

  if(flt_oc_db) {
    flt_oc_db->fd(flt_oc_db,&fd);
    flock(fd,LOCK_UN);
    flt_oc_db->close(flt_oc_db,0);
  }

  if(flt_oc_dbfile) free(flt_oc_dbfile);
}
/* }}} */

t_conf_opt flt_openclose_config[] = {
  { "ThreadsOpenByDefault", flt_oc_get_conf, CFG_OPT_CONFIG|CFG_OPT_USER|CFG_OPT_LOCAL, NULL },
  { "UseJavaScript",        flt_oc_get_conf, CFG_OPT_CONFIG|CFG_OPT_USER|CFG_OPT_LOCAL, NULL },
  { "OpenThreadIfNew",      flt_oc_get_conf, CFG_OPT_CONFIG|CFG_OPT_USER|CFG_OPT_LOCAL, NULL },
  { "OcDbFile",             flt_oc_get_conf, CFG_OPT_USER|CFG_OPT_LOCAL, NULL },
  { NULL, NULL, 0, NULL }
};

t_handler_config flt_openclose_handlers[] = {
  { INIT_HANDLER,         flt_oc_opendb },
  { CONNECT_INIT_HANDLER, flt_oc_exec_xmlhttp },
  { VIEW_HANDLER,         flt_oc_execute_filter },
  { VIEW_INIT_HANDLER,    flt_oc_set_js  },
  { 0, NULL }
};

t_module_config flt_openclose = {
  flt_openclose_config,
  flt_openclose_handlers,
  flt_oc_validate,
  NULL,
  NULL,
  flt_oc_cleanup
};

/* eof */
