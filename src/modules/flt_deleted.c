/**
 * \file flt_deleted.c
 * \author Christian Kruse, <cjk@wwwtech.de>
 *
 * This plugin provides various methods to hide postings/threads
 */

/* {{{ Initial comments */
/*
 * $LastChangedDate$
 * $LastChangedRevision$
 * $LastChangedBy$
 */
/* }}} */

/* {{{ Includes */
#include "cfconfig.h"
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
#include <inttypes.h>

#include <sys/file.h>

#include <db.h>

#include "readline.h"
#include "hashlib.h"
#include "utils.h"
#include "cfgcomp.h"
#include "cfcgi.h"
#include "template.h"
#include "clientlib.h"
/* }}} */

static DB *flt_deleted_db;
static int flt_deleted_do_delete = 1;

/* {{{ flt_deleted_execute */
int flt_deleted_execute(cf_hash_t *head,cf_configuration_t *cfg,cf_cl_thread_t *thread,int mode) {
  cf_cfg_config_value_t *url,*deleted_file;
  u_char *UserName = cf_hash_get(GlobalValues,"UserName",8);
  DBT key,data;
  size_t len;
  u_char buff[256];
  u_char one[] = "1";
  cf_message_t *msg;

  if(UserName) {
    url = cf_cfg_get_value(cfg,"DF:BaseURL");
    msg = cf_msg_get_first_visible(thread->messages);

    /* run only in threadlist mode and only in pre mode */
    if((mode & CF_MODE_THREADLIST) && (mode & CF_MODE_PRE)) {
      if((deleted_file = cf_cfg_get_value(cfg,"Deleted:File")) != NULL) {
        memset(&key,0,sizeof(key));
        memset(&data,0,sizeof(data));

        len = snprintf(buff,256,"%"PRIu64,thread->tid);
        key.data = buff;
        key.size = len;

        data.data = one;
        data.size = sizeof(one);

        if(flt_deleted_db->get(flt_deleted_db,NULL,&key,&data,0) == 0) {
          if(flt_deleted_do_delete) {
            thread->messages->may_show = 0;
            cf_msg_delete_subtree(thread->messages);
          }
          else {
            len = snprintf(buff,256,"?a=u&dt=%"PRIu64,thread->tid);
            cf_tpl_hashvar_setvalue(&thread->messages->hashvar,"deleted_undel_link",TPL_VARIABLE_STRING,buff,len); //TODO: sinnvoller name

            for(msg=thread->messages;msg;msg=msg->next) cf_tpl_hashvar_setvalue(&msg->hashvar,"undel",TPL_VARIABLE_INT,1); //TODO: sinnvoller name
          }
        }
        else {
          if(cf_cfg_get_value_bool(cfg,"Deleted:UseCheckBoxes")) {
            cf_tpl_hashvar_setvalue(&msg->hashvar,"delcheckbox",TPL_VARIABLE_INT,1); //TODO: sinnvoller name
            cf_tpl_hashvar_setvalue(&msg->hashvar,"deltid",TPL_VARIABLE_STRING,buff,len); //TODO: sinnvoller name
          }

          if(cf_cfg_get_value_bool(cfg,"Deleted:UseXMLHttp")) cf_tpl_hashvar_setvalue(&msg->hashvar,"DeletedUseXMLHttp",TPL_VARIABLE_INT,1); //TODO: sinnvoller name

          len = snprintf(buff,150,"%s?a=d&dt=%"PRIu64,url->values[0],thread->tid);
          cf_tpl_hashvar_setvalue(&msg->hashvar,"dellink",TPL_VARIABLE_STRING,buff,len); //TODO: sinnvoller name
        }
      }
    }
    else {
      if(mode & CF_MODE_THREADVIEW) {
        len = snprintf(buff,150,"%s?a=d&dt=%"PRIu64,url->values[0],thread->tid);
        cf_tpl_hashvar_setvalue(&msg->hashvar,"dellink",TPL_VARIABLE_STRING,buff,len);

        if(cf_cfg_get_value_bool(cfg,"Deleted:UseXMLHttp")) cf_tpl_hashvar_setvalue(&msg->hashvar,"DeletedUseXMLHttp",TPL_VARIABLE_INT,1);
      }
    }

    return FLT_OK;
  }


  return FLT_OK;
}
/* }}} */

/* {{{ flt_deleted_pl_filter */
int flt_deleted_pl_filter(cf_hash_t *head,cf_configuration_t *cfg,cf_message_t *msg,u_int64_t tid,int mode) {
  long i;
  cf_cfg_config_value_t *blacklist = cf_cfg_get_value(cfg,"Deleted:Blacklist");

  if(blacklist && blacklist->type == CF_ASM_ARG_ARY && blacklist->alen) {
    if(cf_cfg_get_value_bool(cfg,"Deleted:Blacklist:ActivateInThreadview") || (mode & CF_MODE_THREADLIST)) {
      for(i=0;i<blacklist->alen;++i) {
        if(cf_strcasecmp(msg->author.content,blacklist->avals[i].sval) == 0) {
          msg->may_show = 0;

          if(Cfg.FollowUps == 0) {
            if(!cf_msg_delete_subtree(msg)) return FLT_OK;
          }
        }
      }
    }
  }

  return FLT_OK;
}
/* }}} */

int flt_deleted_perpost(cf_hash_t *head,cf_configuration_t *cfg,cf_cl_thread_t *thread,cf_message_t *msg,cf_tpl_variable_t *hash) {
  long i;
  cf_cfg_config_value_t *blacklist = cf_cfg_get_value(cfg,"Deleted:Blacklist");

  if(blacklist && blacklist->type == CF_ASM_ARG_ARY && blacklist->alen && cf_cfg_get_value_bool(cfg,"Deleted:Blacklist:ActivateInThreadview")) {
    for(i=0;i<blacklist->alen;++i) {
      if(cf_strcasecmp(msg->author.content,blacklist->avals[i].sval) == 0) {
        msg->may_show = 0;

        if(cf_cfg_get_value_book(cfg,"Deleted:Blacklist:ShowFollowups") == 0) cf_msg_delete_subtree(msg);

        return FLT_OK;
      }
    }
  }

  return FLT_DECLINE;
}

/* {{{ flt_deleted_del_thread */
#ifndef CF_SHARED_MEM
int flt_deleted_del_thread(cf_hash_t *head,cf_configuration_t *cfg,int sock)
#else
int flt_deleted_del_thread(cf_hash_t *head,cf_configuration_t *cfg,void *sock)
#endif
{
  u_int64_t tid;
  DBT key,data;
  char one[] = "1";
  int ret,fd;
  char buff[256];
  size_t len;
  cf_cfg_config_value_t *deleted_file;

  cf_cgi_param_t *parm;

  cf_string_t *a,*cgitmp;

  if(head && (deleted_file = cf_cfg_get_value(cfg,"Deleted:File")) != NULL) {
    if((a = cf_cgi_get(head,"a")) != NULL) {
      if(cf_strcmp(a->content,"d") == 0) {
        if((parm = cf_cgi_get_multiple(head,"dt")) != NULL) {
          /* {{{ put tids to database */
          for(;parm;parm=parm->next) {
            tid = cf_str_to_uint64(parm->value.content);

            if(tid) {
              memset(&key,0,sizeof(key));

              memset(&data,0,sizeof(data));
              data.data = one;
              data.size = sizeof(one);

              /* we transform the value again to a string because there could be trash in it... */
              len = snprintf(buff,256,"%"PRIu64,tid);

              key.data = buff;
              key.size = len;

              if((ret = flt_deleted_db->put(flt_deleted_db,NULL,&key,&data,0)) != 0) fprintf(stderr,"flt_deleted: db->put(): %s\n",db_strerror(ret));
            }
          }
          /* }}} */

          /* {{{ set timestamp on file */
          snprintf(buff,256,"%s.tm",Cfg.DeletedFile);
          remove(buff);
          if((fd = open(buff,O_CREAT|O_TRUNC|O_WRONLY)) != -1) close(fd);

          cf_hash_entry_delete(head,"dt",1);
          cf_hash_entry_delete(head,"a",1);
          /* }}} */

          /* {{{ XMLHttp mode */
          if(((cgitmp = cf_cgi_get(head,"mode")) == NULL || cf_strcmp(cgitmp->content,"xmlhttp") != 0) && cf_cfg_get_value_bool(cfg,"Deleted:UseXMLHttp")) {
            printf("Status: 204 No Content\015\012\015\012");
            return FLT_EXIT;
          }
          /* }}} */
        }
      }
      else if(cf_strcmp(a->content,"nd") == 0) flt_deleted_do_delete = 0;
      else if(cf_strcmp(a->content,"u") == 0) {
        if((cgitmp = cf_cgi_get(head,"dt")) != NULL) {
          /* {{{ remove tid from database */
          if((tid = cf_str_to_uint64(cgitmp->content)) > 0) {
            memset(&key,0,sizeof(key));

            /* we transform the value again to a string because there could be trash in it... */
            len = snprintf(buff,256,"%"PRIu64,tid);

            key.data = buff;
            key.size = len;

            flt_deleted_db->del(flt_deleted_db,NULL,&key,0);

            cf_hash_entry_delete(head,"dt",1);
            cf_hash_entry_delete(head,"a",1);
          }
          /* }}} */
        }
      }

      return FLT_OK;
    }
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_del_init_handler */
int flt_del_init_handler(cf_hash_t *cgi,cf_configuration_t *cfg) {
  int ret,fd;
  cf_cfg_config_value_t *delfile;

  if((delfile = cf_cfg_get_value(cfg,"Deleted:File")) != NULL) {
    if((ret = db_create(flt_deleted_db,NULL,0)) != 0) {
      fprintf(stderr,"flt_deleted: db_create() error: %s\n",db_strerror(ret));
      return FLT_EXIT;
    }

    if((ret = flt_deleted_db->open(flt_deleted_db,NULL,delfile->sval,NULL,DB_BTREE,DB_CREATE,0644)) != 0) {
      fprintf(stderr,"flt_deleted: db->open(%s) error: %s\n",delfile->sval,db_strerror(ret));
      return FLT_EXIT;
    }

    if((ret = flt_deleted_db->fd(flt_deleted_db,&fd)) != 0) {
      fprintf(stderr,"flt_deleted: db->fd() error: %s\n",db_strerror(ret));
      return FLT_EXIT;
    }

    if((ret = flock(fd,LOCK_EX)) != 0) {
      fprintf(stderr,"flt_deleted: flock() error: %s\n",strerror(ret));
      return FLT_EXIT;
    }

    return FLT_OK;
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_deleted_view_init_handler */
int flt_del_view_init_handler(cf_hash_t *head,cf_configuration_t *cfg,cf_template_t *begin,cf_template_t *end) {
  cf_string_t *val;
  cf_cfg_config_value_t *deleted_file;

  if((deleted_file = cf_cfg_get_value(cfg,"Deleted:File")) != NULL) {
    if(end && cf_cfg_get_value_bool(cfg,"Deleted:UseCheckboxes")) {
      cf_tpl_setvalue(begin,"delcheckbox",TPL_VARIABLE_INT,1); //TODO: sinnvoller name
      cf_tpl_setvalue(end,"delcheckbox",TPL_VARIABLE_INT,1); //TODO: sinnvoller name
    }

    if(head && (val = cf_cgi_get(head,"a")) != NULL && cf_strcmp(val->content,"nd") == 0) cf_tpl_setvalue(begin,"delnodelete",TPL_VARIABLE_INT,1); //TODO: sinnvoller name
    if(cf_cfg_get_value_bool(cfg,"Deleted:UseXMLHttp")) cf_tpl_setvalue(begin,"DeletedUseXMLHttp",TPL_VARIABLE_INT,1); //TODO: sinnvoller name
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_del_cleanup */
void flt_del_cleanup(void) {
  if(flt_deleted_db) flt_deleted_db->close(flt_deleted_db,0);
}
/* }}} */

/* {{{ flt_deleted_validate */
#ifndef CF_SHARED_MEM
int flt_deleted_validate(cf_hash_t *head,cf_configuration_t *cfg,time_t last_modified,int sock)
#else
int flt_deleted_validate(cf_hash_t *head,cf_configuration_t *cfg,time_t last_modified,void *sock)
#endif
{
  struct stat st;
  char buff[256];
  cf_cfg_config_value_t *deleted_file;

  if((deleted_file = cf_cfg_get_value(cfg,"Deleted:File")) != NULL) {
    snprintf(buff,256,"%s.tm",deleted_file->sval);

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

/* {{{ flt_deleted_lm */
#ifndef CF_SHARED_MEM
time_t flt_deleted_lm(cf_hash_t *head,cf_configuration_t *cfg,int sock)
#else
time_t flt_deleted_lm(cf_hash_t *head,cf_configuration_t *cfg,void *sock)
#endif
{
  struct stat st;
  char buff[256];
  cf_cfg_config_value_t *deleted_file;

  if((deleted_file = cf_cfg_get_value(cfg,"Deleted:File")) != NULL) {
    snprintf(buff,256,"%s.tm",deleted_file->sval);
    if(stat(buff,&st) == -1) return -1;
    return st.st_mtime;
  }

  return -1;
}
/* }}} */

/**
 * Config options:
 * Deleted:Blacklist = ("name","name1");
 * Deleted:Blacklist:ShowFollowups = Yes|No;
 * Deleted:Blacklist:ActivateInThreadview = Yes|No;
 * Deleted:File = "/path/to/file";
 * Deleted:UseCheckboxes = Yes|No;
 * Deleted:Response204 = Yes|No;
 * Deleted:UseXMLHttp = Yes|No;
 * Deleted:UseCheckboxes = Yes|No;
 */

cf_handler_config_t flt_deleted_handlers[] = {
  { VIEW_INIT_HANDLER,    flt_del_view_init_handler },
  { INIT_HANDLER,         flt_del_init_handler      },
  { CONNECT_INIT_HANDLER, flt_deleted_del_thread    },
  { VIEW_HANDLER,         flt_deleted_execute       },
  { VIEW_LIST_HANDLER,    flt_deleted_pl_filter     },
  { PERPOST_VAR_HANDLER,  flt_deleted_perpost       },
  { 0, NULL }
};

cf_module_config_t flt_deleted = {
  MODULE_MAGIC_COOKIE,
  flt_deleted_handlers,
  NULL,
  flt_deleted_validate,
  flt_deleted_lm,
  NULL,
  flt_del_cleanup
};

/* eof */
