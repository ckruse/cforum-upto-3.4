/**
 * \file flt_deleted.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 *
 * This plugin hides deleted postings
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

struct {
  u_char **BlackList;
  long BLlen;
  int FollowUps;
  time_t ShowFrom;
  time_t ShowUntil;
  u_char *DeletedFile;
  int CheckBoxes;
  DB *db;
  int DoDelete;
  int resp_204;
  int bl_in_thrv;
} Cfg = { NULL, 0, 0, 0, 0, NULL, 0, NULL, 1, 0, 0 };

static u_char *flt_deleted_fname = NULL;

/* {{{ flt_deleted_execute */
int flt_deleted_execute(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_cl_thread *thread,int mode) {
  t_name_value *url;
  u_char *UserName = cf_hash_get(GlobalValues,"UserName",8);
  u_char *forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  DBT key,data;
  size_t len;
  u_char buff[256];
  u_char one[] = "1";
  url = cfg_get_first_value(dc,forum_name,"UBaseURL");
  t_message *msg;

  if(UserName) {
    msg = cf_msg_get_first_visible(thread->messages);

    /* run only in threadlist mode and only in pre mode */
    if(mode & (CF_MODE_THREADLIST|CF_MODE_PRE)) {
      if(Cfg.DeletedFile) {
        memset(&key,0,sizeof(key));
        memset(&data,0,sizeof(data));

        len = snprintf(buff,256,"%llu",thread->tid);
        key.data = buff;
        key.size = len;

        data.data = one;
        data.size = sizeof(one);

        if(Cfg.db->get(Cfg.db,NULL,&key,&data,0) == 0) {
          if(Cfg.DoDelete) {
            thread->messages->may_show = 0;
            cf_msg_delete_subtree(thread->messages);
          }
          else {
            len = snprintf(buff,256,"?a=u&t=%llu",thread->tid);
            cf_tpl_setvalue(&thread->messages->tpl,"undel",TPL_VARIABLE_STRING,buff,len);
          }
        }
        else {
          if(Cfg.CheckBoxes) {
            cf_tpl_setvalue(&msg->tpl,"delcheckbox",TPL_VARIABLE_STRING,"1",1);
            cf_tpl_setvalue(&msg->tpl,"deltid",TPL_VARIABLE_STRING,buff,len);
          }

          len = snprintf(buff,150,"%s?a=d&t=%lld",url->values[0],thread->tid);
          cf_tpl_setvalue(&msg->tpl,"dellink",TPL_VARIABLE_STRING,buff,len);
        }
      }
    }
    else {
      len = snprintf(buff,150,"%s?a=d&t=%lld",url->values[0],thread->tid);
      cf_tpl_setvalue(&msg->tpl,"dellink",TPL_VARIABLE_STRING,buff,len);
    }

    return FLT_OK;
  }


  return FLT_OK;
}
/* }}} */

/* {{{ flt_deleted_pl_filter */
int flt_deleted_pl_filter(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_message *msg,u_int64_t tid,int mode) {
  long i;

  if(Cfg.BLlen) {
    if(Cfg.bl_in_thrv || (mode & CF_MODE_THREADLIST)) {
      for(i=0;i<Cfg.BLlen;i++) {
        if(cf_strcasecmp(msg->author.content,Cfg.BlackList[i]) == 0) {
          msg->may_show = 0;

          if(Cfg.FollowUps == 0) {
            if(!cf_msg_delete_subtree(msg)) return FLT_OK;
          }
        }
      }
    }
  }

  if(mode & CF_MODE_THREADLIST) {
    if(Cfg.ShowFrom) {
      if(msg->may_show) {
        if(msg->date < Cfg.ShowFrom) msg->may_show = 0;
      }
    }

    if(Cfg.ShowUntil) {
      if(msg->may_show) {
        if(msg->date > Cfg.ShowUntil) msg->may_show = 0;
      }
    }
  }

  return FLT_OK;
}
/* }}} */

/* {{{ flt_deleted_del_thread */
#ifndef CF_SHARED_MEM
int flt_deleted_del_thread(t_cf_hash *head,t_configuration *dc,t_configuration *vc,int sock)
#else
int flt_deleted_del_thread(t_cf_hash *head,t_configuration *dc,t_configuration *vc,void *sock)
#endif
{
  u_char *c_tid,*qs,*a;
  u_int64_t tid;
  DBT key,data;
  char one[] = "1";
  int ret,fd;
  char buff[256];
  size_t len;
  t_cf_cgi_param *parm;
  u_char *tmp;

  if(head && Cfg.DeletedFile) {
    qs = getenv("QUERY_STRING");

    if(qs) {
      a = cf_cgi_get(head,"a");

      if(a) {
        if(cf_strcmp(a,"d") == 0) {
          if((parm = cf_cgi_get_multiple(head,"t")) != NULL) {
            memset(&data,0,sizeof(data));
            data.data = one;
            data.size = sizeof(one);

            for(;parm;parm=parm->next) {
              tid = strtoull(parm->value,NULL,10);

              if(tid) {
                memset(&key,0,sizeof(key));

                /* we transform the value again to a string because there could be trash in it... */
                len = snprintf(buff,256,"%llu",tid);

                key.data = buff;
                key.size = len;

                Cfg.db->put(Cfg.db,NULL,&key,&data,DB_NODUPDATA|DB_NOOVERWRITE);
              }
            }

            /* set timestamp on file */
            snprintf(buff,256,"%s.tm",Cfg.DeletedFile);
            remove(buff);
            if((fd = open(buff,O_CREAT|O_TRUNC|O_WRONLY)) != -1) close(fd);

            cf_hash_entry_delete(head,"t",1);
            cf_hash_entry_delete(head,"a",1);

            if(Cfg.resp_204) {
              printf("Status: 204 No Content\015\012\015\012");
              return FLT_EXIT;
            }
          }
        }
        else if(cf_strcmp(a,"nd") == 0) {
          Cfg.DoDelete = 0;
        }
        else if(cf_strcmp(a,"u") == 0) {
          if((tmp = cf_cgi_get(head,"t")) != NULL) {
            if((tid = strtoull(tmp,NULL,10)) > 0) {
              memset(&key,0,sizeof(key));

              /* we transform the value again to a string because there could be trash in it... */
              len = snprintf(buff,256,"%llu",tid);

              key.data = buff;
              key.size = len;

              Cfg.db->del(Cfg.db,NULL,&key,0);

              cf_hash_entry_delete(head,"t",1);
              cf_hash_entry_delete(head,"a",1);
            }
          }
        }

        return FLT_OK;
      }
    }
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_del_init_handler */
int flt_del_init_handler(t_cf_hash *cgi,t_configuration *dc,t_configuration *vc) {
  int ret,fd;

  if(Cfg.DeletedFile) {
    if((ret = db_create(&Cfg.db,NULL,0)) != 0) {
      fprintf(stderr,"DB error: %s\n",db_strerror(ret));
      return FLT_EXIT;
    }

    if((ret = Cfg.db->open(Cfg.db,NULL,Cfg.DeletedFile,NULL,DB_BTREE,DB_CREATE,0644)) != 0) {
      fprintf(stderr,"DB error: %s\n",db_strerror(ret));
      return FLT_EXIT;
    }

    if((ret = Cfg.db->fd(Cfg.db,&fd)) != 0) {
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

/* {{{ flt_deleted_view_init_handler */
int flt_del_view_init_handler(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_cf_template *begin,t_cf_template *end) {
  if(end && Cfg.CheckBoxes && Cfg.DeletedFile) {
    if(head && cf_cgi_get(head,"nd") != NULL) cf_tpl_setvalue(begin,"delnodelete",TPL_VARIABLE_STRING,"1",1);

    cf_tpl_setvalue(begin,"delcheckbox",TPL_VARIABLE_STRING,"1",1);
    cf_tpl_setvalue(end,"delcheckbox",TPL_VARIABLE_STRING,"1",1);
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_del_handle_command */
int flt_del_handle_command(t_configfile *cf,t_conf_opt *opt,const u_char *context,u_char **args,size_t argnum) {
  long i;
  if(flt_deleted_fname == NULL) flt_deleted_fname = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(flt_deleted_fname,context) != 0) return 0;

  if(cf_strcmp(opt->name,"BlackList") == 0) {
    if(Cfg.BLlen) {
      for(i=0;i<Cfg.BLlen;i++) {
        free(Cfg.BlackList[i]);
      }
      free(Cfg.BlackList);
    }

    Cfg.BLlen = split(args[0],",",&Cfg.BlackList);
  }
  else if(cf_strcmp(opt->name,"ShowBlacklistFollowups") == 0) {
    Cfg.FollowUps = cf_strcasecmp(args[0],"yes") == 0;
  }
  else if(cf_strcmp(opt->name,"ShowFrom") == 0) {
    Cfg.ShowFrom = strtol(args[0],NULL,10);
  }
  else if(cf_strcmp(opt->name,"ShowUntil") == 0) {
    Cfg.ShowUntil = strtol(args[0],NULL,10);
  }
  else if(cf_strcmp(opt->name,"DeletedFile") == 0) {
    if(Cfg.DeletedFile) free(Cfg.DeletedFile);
    Cfg.DeletedFile = strdup(args[0]);
  }
  else if(cf_strcmp(opt->name,"DeletedUseCheckboxes") == 0) {
    Cfg.CheckBoxes = cf_strcmp(args[0],"yes") == 0;
  }
  else if(cf_strcmp(opt->name,"DelThreadResponse204") == 0) {
    Cfg.resp_204 = cf_strcmp(args[0],"yes") == 0;
  }
  else if(cf_strcmp(opt->name,"BlacklistInThreadView") == 0) {
    Cfg.bl_in_thrv = cf_strcmp(args[0],"yes") == 0;
  }

  return 0;
}
/* }}} */

/* {{{ flt_del_cleanup */
void flt_del_cleanup(void) {
  int fd;
  long i;

  if(Cfg.db) {
    Cfg.db->fd(Cfg.db,&fd);
    flock(fd,LOCK_UN);
    Cfg.db->close(Cfg.db,0);
  }

  if(Cfg.BLlen) {
    for(i=0;i<Cfg.BLlen;i++) {
      free(Cfg.BlackList[i]);
    }
    free(Cfg.BlackList);
  }

  if(Cfg.DeletedFile) free(Cfg.DeletedFile);
}
/* }}} */

/* {{{ flt_deleted_validate */
#ifndef CF_SHARED_MEM
int flt_deleted_validate(t_cf_hash *head,t_configuration *dc,t_configuration *vc,time_t last_modified,int sock)
#else
int flt_deleted_validate(t_cf_hash *head,t_configuration *dc,t_configuration *vc,time_t last_modified,void *sock)
#endif
{
  struct stat st;
  char buff[256];

  if(Cfg.DeletedFile) {
    snprintf(buff,256,"%s.tm",Cfg.DeletedFile);

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
time_t flt_deleted_lm(t_cf_hash *head,t_configuration *dc,t_configuration *vc,int sock)
#else
time_t flt_deleted_lm(t_cf_hash *head,t_configuration *dc,t_configuration *vc,void *sock)
#endif
{
  struct stat st;
  char buff[256];

  if(Cfg.DeletedFile) {
    snprintf(buff,256,"%s.tm",Cfg.DeletedFile);
    if(stat(buff,&st) == -1) return -1;
    return st.st_mtime;
  }


  return -1;
}
/* }}} */

t_conf_opt flt_deleted_config[] = {
  { "BlackList",               flt_del_handle_command, CFG_OPT_CONFIG|CFG_OPT_USER|CFG_OPT_LOCAL, NULL },
  { "ShowBlacklistFollowups",  flt_del_handle_command, CFG_OPT_CONFIG|CFG_OPT_USER|CFG_OPT_LOCAL, NULL },
  { "BlacklistInThreadview",   flt_del_handle_command, CFG_OPT_CONFIG|CFG_OPT_USER|CFG_OPT_LOCAL, NULL },
  { "ShowFrom",                flt_del_handle_command, CFG_OPT_CONFIG|CFG_OPT_USER|CFG_OPT_LOCAL, NULL },
  { "ShowUntil",               flt_del_handle_command, CFG_OPT_CONFIG|CFG_OPT_USER|CFG_OPT_LOCAL, NULL },
  { "DeletedFile",             flt_del_handle_command, CFG_OPT_USER|CFG_OPT_NEEDED|CFG_OPT_LOCAL, NULL },
  { "DeletedUseCheckboxes",    flt_del_handle_command, CFG_OPT_CONFIG|CFG_OPT_USER|CFG_OPT_LOCAL, NULL },
  { "DelThreadResponse204",    flt_del_handle_command, CFG_OPT_CONFIG|CFG_OPT_USER|CFG_OPT_LOCAL, NULL },
  { NULL, NULL, 0, NULL }
};

t_handler_config flt_deleted_handlers[] = {
  { VIEW_INIT_HANDLER,    flt_del_view_init_handler },
  { INIT_HANDLER,         flt_del_init_handler      },
  { CONNECT_INIT_HANDLER, flt_deleted_del_thread    },
  { VIEW_HANDLER,         flt_deleted_execute       },
  { VIEW_LIST_HANDLER,    flt_deleted_pl_filter     },
  { 0, NULL }
};

t_module_config flt_deleted = {
  flt_deleted_config,
  flt_deleted_handlers,
  flt_deleted_validate,
  flt_deleted_lm,
  NULL,
  flt_del_cleanup
};

/* eof */
