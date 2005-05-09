/**
 * \file flt_interesting.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 *
 * This plugin highlights interesting postings
 */

/* {{{ Initial comments */
/*
 * $LastChangedDate$
 * $LastChangedRevision$
 * $LastChangedBy$
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

static u_char *flt_interesting_file     = NULL;
static int     flt_interesting_resp_204 = 0;

static u_char *flt_interesting_fname    = NULL;
static DB     *flt_interesting_db       = NULL;

/* {{{ flt_interesting_init_handler */
int flt_interesting_init_handler(t_cf_hash *cgi,t_configuration *dc,t_configuration *vc) {
  int ret,fd;

  if(flt_interesting_file) {
    if((ret = db_create(&flt_interesting_db,NULL,0)) != 0) {
      fprintf(stderr,"DB error: %s\n",db_strerror(ret));
      return FLT_EXIT;
    }

    if((ret = flt_interesting_db->open(flt_interesting_db,NULL,flt_interesting_file,NULL,DB_BTREE,DB_CREATE,0644)) != 0) {
      fprintf(stderr,"DB error: %s\n",db_strerror(ret));
      return FLT_EXIT;
    }

    if((ret = flt_interesting_db->fd(flt_interesting_db,&fd)) != 0) {
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

/* {{{ flt_interesting_mark_thread */
#ifndef CF_SHARED_MEM
int flt_interesting_mark_thread(t_cf_hash *head,t_configuration *dc,t_configuration *vc,int sock)
#else
int flt_interesting_mark_thread(t_cf_hash *head,t_configuration *dc,t_configuration *vc,void *sock)
#endif
{
  u_char *c_tid,*a;
  u_int64_t tid;
  DBT key,data;
  char one[] = "1";
  int ret,fd;
  char buff[256];
  size_t len;
  t_cf_cgi_param *parm;
  u_char *tmp;

  if(head && flt_interesting_file) {
    if((a = cf_cgi_get(head,"a")) != NULL && cf_strcmp(a,"mi") == 0) {
      if((parm = cf_cgi_get_multiple(head,"mit")) != NULL) {
        /* {{{ put tids to database */
        for(;parm;parm=parm->next) {
          tid = strtoull(parm->value,NULL,10);

          if(tid) {
            memset(&key,0,sizeof(key));

            memset(&data,0,sizeof(data));
            data.data = one;
            data.size = sizeof(one);

            /* we transform the value again to a string because there could be trash in it... */
            len = snprintf(buff,256,"%llu",tid);

            key.data = buff;
            key.size = len;

            if((ret = flt_interesting_db->put(flt_interesting_db,NULL,&key,&data,0)) != 0) fprintf(stderr,"db->put(): %s\n",db_strerror(ret));
          }
        }
        /* }}} */

        /* {{{ set timestamp on file */
        snprintf(buff,256,"%s.tm",flt_interesting_file);
        remove(buff);
        if((fd = open(buff,O_CREAT|O_TRUNC|O_WRONLY)) != -1) close(fd);

        cf_hash_entry_delete(head,"mit",1);
        cf_hash_entry_delete(head,"a",1);
        /* }}} */

        /* {{{ 204 Response */
        if(flt_interesting_resp_204) {
          printf("Status: 204 No Content\015\012\015\012");
          return FLT_EXIT;
        }
        /* }}} */
      }

      return FLT_OK;
    }
  }

  return FLT_DECLINE;

}
/* }}} */

/* {{{ flt_interesting_mark_interesting */
int flt_interesting_mark_interesting(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_cl_thread *thread,int mode) {
  t_name_value *url;
  u_char *UserName = cf_hash_get(GlobalValues,"UserName",8);
  u_char *forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  DBT key,data;
  size_t len;
  u_char buff[256];
  t_message *msg;

  if(UserName) {
    url = cfg_get_first_value(dc,forum_name,"UBaseURL");
    msg = cf_msg_get_first_visible(thread->messages);

    /* run only in threadlist mode and only in pre mode */
    if((mode & CF_MODE_THREADLIST) && (mode & CF_MODE_PRE)) {
      if(flt_interesting_file) {
        memset(&key,0,sizeof(key));
        memset(&data,0,sizeof(data));

        len = snprintf(buff,256,"%llu",thread->tid);
        key.data = buff;
        key.size = len;

        if(flt_interesting_db->get(flt_interesting_db,NULL,&key,&data,0) == 0) cf_tpl_setvalue(&msg->tpl,"mi",TPL_VARIABLE_INT,1);
        else {
          len = snprintf(buff,150,"%s?a=mi&mit=%lld",url->values[0],thread->tid);
          cf_tpl_setvalue(&msg->tpl,"milink",TPL_VARIABLE_STRING,buff,len);
        }
      }
    }

    return FLT_OK;
  }


  return FLT_OK;
}
/* }}} */


/* {{{ flt_interesting_validate */
#ifndef CF_SHARED_MEM
int flt_interesting_validate(t_cf_hash *head,t_configuration *dc,t_configuration *vc,time_t last_modified,int sock)
#else
int flt_interesting_validate(t_cf_hash *head,t_configuration *dc,t_configuration *vc,time_t last_modified,void *sock)
#endif
{
  struct stat st;
  char buff[256];

  if(flt_interesting_file) {
    snprintf(buff,256,"%s.tm",flt_interesting_file);

    if(stat(buff,&st) == -1) return FLT_DECLINE;
    #ifdef DEBUG
    printf("X-Debug: stat(): %s",ctime(&st.st_mtime));
    printf("X-Debug: last_modified: %s",ctime(&last_modified));
    #endif
    if(st.st_mtime > last_modified) return FLT_EXIT;
  }

  return FLT_OK;
}
/* }}} */

/* {{{ flt_interesting_lm */
#ifndef CF_SHARED_MEM
time_t flt_interesting_lm(t_cf_hash *head,t_configuration *dc,t_configuration *vc,int sock)
#else
time_t flt_interesting_lm(t_cf_hash *head,t_configuration *dc,t_configuration *vc,void *sock)
#endif
{
  struct stat st;
  char buff[256];

  if(flt_interesting_file) {
    snprintf(buff,256,"%s.tm",flt_interesting_file);
    if(stat(buff,&st) == -1) return -1;
    return st.st_mtime;
  }

  return -1;
}
/* }}} */


/* {{{ flt_interesting_handle */
int flt_interesting_handle(t_configfile *cf,t_conf_opt *opt,const u_char *context,u_char **args,size_t argnum) {
  if(flt_interesting_fname == NULL) flt_interesting_fname = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(flt_interesting_fname,context) != 0) return 0;

  if(cf_strcmp(opt->name,"InterestingFile") == 0) {
    if(flt_interesting_file) free(flt_interesting_file);
    flt_interesting_file = strdup(args[0]);
  }
  else if(cf_strcmp(opt->name,"Interesting204") == 0) flt_interesting_resp_204 = cf_strcmp(args[0],"yes") == 0;

  return 0;
}
/* }}} */

/* {{{ flt_interesting_cleanup */
void flt_interesting_cleanup(void) {
  if(flt_interesting_file) free(flt_interesting_file);
  if(flt_interesting_db) flt_interesting_db->close(flt_interesting_db,0);
}
/* }}} */

t_conf_opt flt_interesting_config[] = {
  { "InterestingFile", flt_interesting_handle, CFG_OPT_USER|CFG_OPT_LOCAL, NULL },
  { "Interesting204",  flt_interesting_handle, CFG_OPT_USER|CFG_OPT_LOCAL, NULL },
  { NULL, NULL, 0, NULL }
};

t_handler_config flt_interesting_handlers[] = {
  { INIT_HANDLER,         flt_interesting_init_handler     },
  { CONNECT_INIT_HANDLER, flt_interesting_mark_thread      },
  { VIEW_HANDLER,         flt_interesting_mark_interesting },
  { 0, NULL }
};

t_module_config flt_interesting = {
  flt_interesting_config,
  flt_interesting_handlers,
  flt_interesting_validate,
  flt_interesting_lm,
  NULL,
  flt_interesting_cleanup
};


/* eof */
