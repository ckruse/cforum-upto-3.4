/**
 * \file flt_visited.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 *
 * This filter marks threads visited
 *
 * We now use BDB. My tree structures are not very good
 * for saving to disks (AVL trees). Maybe in future I'll
 * write a B-Tree library.
 *
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
#include <sys/stat.h>
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
  int   HighlightVisitedPostings;
  u_char *VisitedPostingsColorF;
  u_char *VisitedPostingsColorB;
  u_char *VisitedFile;
  DB *db;
  int mark_visited;
  int resp_204;
  int mark_all_visited;
} Cfg = { 0, NULL, NULL, NULL, NULL, 0, 0, 0 };

static u_char *flt_visited_fn = NULL;

/* {{{ module api function, checks if message has been visited */
void *flt_visited_is_visited(void *vmid) {
  DBT key,data;
  u_int64_t *mid = (u_int64_t *)vmid;
  size_t len;
  u_char buff[256];

  memset(&key,0,sizeof(key));
  memset(&data,0,sizeof(data));

  if(Cfg.VisitedFile) {
    len = snprintf(buff,256,"%llu",*mid);
    key.data = buff;
    key.size = len;

    if(Cfg.db->get(Cfg.db,NULL,&key,&data,0) == 0) {
      return (void *)mid;
    }
  }

  return NULL;
}
/* }}} */

/* {{{ module api function, mark a posting visited */
void *flt_visited_mark_visited_api(void *vmid) {
  u_int64_t *mid = (u_int64_t *)vmid;
  DBT key,data;
  int ret,fd;
  u_char one[] = "1";
  u_char buff[256];
  size_t len;

  if(Cfg.VisitedFile) {
    memset(&key,0,sizeof(key));
    memset(&data,0,sizeof(data));

    len = snprintf(buff,256,"%llu",*mid);
    key.data = buff;
    key.size = len;

    data.data = one;
    data.size = sizeof(one);

    if((ret = Cfg.db->put(Cfg.db,NULL,&key,&data,DB_NODUPDATA)) != 0) {
      if(ret == DB_KEYEXIST) return vmid;
      return NULL;
    }

    snprintf(buff,256,"%s.tm",Cfg.VisitedFile);
    remove(buff);
    if((fd = open(buff,O_CREAT|O_TRUNC|O_WRONLY)) != -1) close(fd);

    return vmid;
  }

  return NULL;
}
/* }}} */

/* {{{ flt_visited_execute_filter */
#ifndef CF_SHARED_MEM
int flt_visited_execute_filter(t_cf_hash *head,t_configuration *dc,t_configuration *vc,int sock)
#else
int flt_visited_execute_filter(t_cf_hash *head,t_configuration *dc,t_configuration *vc,void *sock)
#endif
{
  u_char *uname = cf_hash_get(GlobalValues,"UserName",8);
  u_char *cmid;
  u_char *ctid;
  u_int64_t mid,tid;
  int ret,fd;
  char one[] = "1";
  rline_t tsd;
  t_message *msg;
  t_cl_thread thread;
  DBT key,data;
  char buff[256],*mav;
  size_t len;

  if(uname && Cfg.VisitedFile) {
    memset(&key,0,sizeof(key));
    memset(&data,0,sizeof(data));

    data.data = one;
    data.size = sizeof(one);

    /* register module api */
    cf_register_mod_api_ent("flt_visited","is_visited",flt_visited_is_visited);
    cf_register_mod_api_ent("flt_visited","mark_visited",flt_visited_mark_visited_api);

    /* check if we should mark all visited */
    if(head && (mav = cf_cgi_get(head,"mav")) != NULL) {
      if(*mav == '1') Cfg.mark_all_visited = 1;
    }


    if(head && Cfg.HighlightVisitedPostings) {
      cmid = cf_cgi_get(head,"m");
      ctid = cf_cgi_get(head,"mv");

      /* we shall mark a whole thread visited */
      if(ctid) {
        tid = str_to_u_int64(ctid);
        memset(&tsd,0,sizeof(tsd));
        memset(&thread,0,sizeof(thread));

        if(tid) {
          #ifdef CF_SHARED_MEM
          ret = cf_get_message_through_shm(sock,&thread,NULL,tid,0,CF_KILL_DELETED);
          #else
          ret = cf_get_message_through_sock(sock,&tsd,&thread,NULL,tid,0,CF_KILL_DELETED);
          #endif

          if(ret == -1) return FLT_DECLINE;

          for(msg=thread.messages;msg;msg=msg->next) {
            len = snprintf(buff,256,"%llu",msg->mid);
            key.data = buff;
            key.size = len;

            if(Cfg.db->put(Cfg.db,NULL,&key,&data,DB_NODUPDATA|DB_NOOVERWRITE) == 0) {
              snprintf(buff,256,"%s.tm",Cfg.VisitedFile);
              remove(buff);
              if((fd = open(buff,O_CREAT|O_TRUNC|O_WRONLY)) != -1) close(fd);
            }

          }

          cf_cleanup_thread(&thread);

          if(Cfg.resp_204) {
            printf("Status: 204 No Content\015\012\015\012");
            return FLT_EXIT;
          }
        }
      }

      /* we shall only mark a message visited */
      else if(cmid) {
        mid = strtoull(cmid,NULL,10);

        if(mid) {
          len = snprintf(buff,256,"%llu",mid);
          key.data = buff;
          key.size = len;

          Cfg.db->put(Cfg.db,NULL,&key,&data,DB_NODUPDATA|DB_NOOVERWRITE);
          snprintf(buff,256,"%s.tm",Cfg.VisitedFile);
          remove(buff);
          if((fd = open(buff,O_CREAT|O_TRUNC|O_WRONLY)) != -1) close(fd);
        }
      }

      return FLT_OK;
    }
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_visited_set_col */
int flt_visited_set_col(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_cf_template *begin,t_cf_template *end) {
  if(Cfg.VisitedPostingsColorF || Cfg.VisitedPostingsColorB) {
    cf_tpl_setvalue(begin,"visitedcol",TPL_VARIABLE_STRING,"1",1);

    if(Cfg.VisitedPostingsColorF && *Cfg.VisitedPostingsColorF) {
      cf_tpl_setvalue(begin,"visitedcolfg",TPL_VARIABLE_STRING,Cfg.VisitedPostingsColorF,strlen(Cfg.VisitedPostingsColorF));
    }
    if(Cfg.VisitedPostingsColorB && *Cfg.VisitedPostingsColorB) {
      cf_tpl_setvalue(begin,"visitedcolbg",TPL_VARIABLE_STRING,Cfg.VisitedPostingsColorB,strlen(Cfg.VisitedPostingsColorB));
    }
  }

  return FLT_OK;
}
/* }}} */

/* {{{ flt_visited_posting_handler */
int flt_visited_posting_handler(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_cl_thread *thread,t_cf_template *tpl) {
  return flt_visited_set_col(head,dc,vc,tpl,NULL);
}
/* }}} */

/* {{{ flt_visited_mark_visited */
int flt_visited_mark_visited(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_message *msg,u_int64_t tid,int mode) {
  u_char *uname = cf_hash_get(GlobalValues,"UserName",8);
  DBT key,data;
  u_char buff[256];
  size_t len;
  int fd;

  if(uname && Cfg.VisitedFile && Cfg.HighlightVisitedPostings) {
    memset(&key,0,sizeof(key));
    memset(&data,0,sizeof(data));

    len = snprintf(buff,256,"%llu",msg->mid);
    key.data = buff;
    key.size = len;

    if(Cfg.mark_all_visited) {
      if(Cfg.db->put(Cfg.db,NULL,&key,&data,DB_NODUPDATA|DB_NOOVERWRITE) == 0) {
        snprintf(buff,256,"%s.tm",Cfg.VisitedFile);
        remove(buff);
        if((fd = open(buff,O_CREAT|O_TRUNC|O_WRONLY)) != -1) close(fd);
      }

      memset(&data,0,sizeof(data));
    }

    if(Cfg.db->get(Cfg.db,NULL,&key,&data,0) == 0) cf_tpl_setvalue(&msg->tpl,"visited",TPL_VARIABLE_STRING,"1",1);

    return FLT_OK;
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_visited_mark_own */
int flt_visited_mark_own(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_message *msg,u_int64_t tid) {
  return flt_visited_mark_visited_api(&msg->mid) != NULL ? FLT_OK : FLT_DECLINE;
}
/* }}} */

/* {{{ flt_visited_validate */
#ifndef CF_SHARED_MEM
int flt_visited_validate(t_cf_hash *head,t_configuration *dc,t_configuration *vc,time_t last_modified,int sock)
#else
int flt_visited_validate(t_cf_hash *head,t_configuration *dc,t_configuration *vc,time_t last_modified,void *sock)
#endif
{
  struct stat st;
  char buff[256];

  if(Cfg.VisitedFile) {
    snprintf(buff,256,"%s.tm",Cfg.VisitedFile);
    if(stat(buff,&st) == -1) return FLT_DECLINE;
    if(st.st_mtime > last_modified) return FLT_EXIT;
  }

  return FLT_OK;
}
/* }}} */

/* {{{ flt_visited_lm */
#ifndef CF_SHARED_MEM
time_t flt_visited_lm(t_cf_hash *head,t_configuration *dc,t_configuration *vc,int sock)
#else
time_t flt_visited_lm(t_cf_hash *head,t_configuration *dc,t_configuration *vc,void *sock)
#endif
{
  struct stat st;
  char buff[256];

  if(Cfg.VisitedFile) {
    snprintf(buff,256,"%s.tm",Cfg.VisitedFile);
    if(stat(buff,&st) == -1) return -1;
    return st.st_mtime;
  }


  return -1;
}
/* }}} */

/* {{{ flt_visited_init_handler */
int flt_visited_init_handler(t_cf_hash *cgi,t_configuration *dc,t_configuration *vc) {
  int ret,fd;

  if(Cfg.VisitedFile) {
    if((ret = db_create(&Cfg.db,NULL,0)) != 0) {
      fprintf(stderr,"DB error: %s\n",db_strerror(ret));
      return FLT_EXIT;
    }

    if((ret = Cfg.db->open(Cfg.db,NULL,Cfg.VisitedFile,NULL,DB_BTREE,DB_CREATE,0644)) != 0) {
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

/* {{{ flt_visited_set_link */
#ifndef CF_SHARED_MEM
int flt_visited_set_link(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_cl_thread *thr,int sock)
#else
int flt_visited_set_link(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_cl_thread *thr,void *sock)
#endif
{
  u_char buff[512];
  int UserName = cf_hash_get(GlobalValues,"UserName",8) != NULL;
  u_char *forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  t_name_value *x = cfg_get_first_value(dc,forum_name,UserName?"UBaseURL":"BaseURL");
  t_name_value *cs = cfg_get_first_value(dc,forum_name,"ExternCharset");
  size_t len;
  t_message *msg = cf_msg_get_first_visible(thr->messages);

  if(Cfg.VisitedFile && Cfg.HighlightVisitedPostings) {
    len = snprintf(buff,512,"%s?mv=%llu",x->values[0],thr->tid);
    cf_set_variable(&msg->tpl,cs,"mvlink",buff,len,1);
  }

  return FLT_OK;
}
/* }}} */

/* {{{ flt_visit_handle_command */
int flt_visit_handle_command(t_configfile *cf,t_conf_opt *opt,const u_char *context,u_char **args,size_t argnum) {
  if(flt_visited_fn == NULL) flt_visited_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(flt_visited_fn,context) != 0) return 0;

  if(cf_strcmp(opt->name,"HighlightVisitedPostings") == 0) {
    Cfg.HighlightVisitedPostings = !cf_strcmp(args[0],"yes");
  }
  else if(cf_strcmp(opt->name,"VisitedPostingsColors") == 0) {
    if(Cfg.VisitedPostingsColorF) free(Cfg.VisitedPostingsColorF);
    if(Cfg.VisitedPostingsColorB) free(Cfg.VisitedPostingsColorB);
    Cfg.VisitedPostingsColorF = strdup(args[0]);
    Cfg.VisitedPostingsColorB = strdup(args[1]);
  }
  else if(cf_strcmp(opt->name,"VisitedFile") == 0) {
    if(Cfg.VisitedFile) free(Cfg.VisitedFile);
    Cfg.VisitedFile = strdup(args[0]);
  }
  else if(cf_strcmp(opt->name,"MarkOwnPostsVisited") == 0) {
    Cfg.mark_visited = cf_strcmp(args[0],"yes") == 0;
  }
  else if(cf_strcmp(opt->name,"MarkThreadResponse204") == 0) {
    Cfg.resp_204 = cf_strcmp(args[0],"yes") == 0;
  }

  return 0;
}
/* }}} */

/* {{{ flt_visited_cleanup */
void flt_visited_cleanup(void) {
  int fd;

  if(Cfg.VisitedPostingsColorF) free(Cfg.VisitedPostingsColorF);
  if(Cfg.VisitedPostingsColorB) free(Cfg.VisitedPostingsColorB);
  if(Cfg.VisitedFile)      free(Cfg.VisitedFile);

  if(Cfg.db) {
    Cfg.db->fd(Cfg.db,&fd);
    flock(fd,LOCK_UN);

    Cfg.db->close(Cfg.db,0);
  }
}
/* }}} */

t_conf_opt flt_visited_config[] = {
  { "HighlightVisitedPostings", flt_visit_handle_command, CFG_OPT_USER|CFG_OPT_LOCAL,                NULL },
  { "VisitedPostingsColors",    flt_visit_handle_command, CFG_OPT_USER|CFG_OPT_LOCAL,                NULL },
  { "VisitedFile",              flt_visit_handle_command, CFG_OPT_USER|CFG_OPT_LOCAL|CFG_OPT_NEEDED, NULL },
  { "MarkOwnPostsVisited",      flt_visit_handle_command, CFG_OPT_USER|CFG_OPT_LOCAL,                NULL },
  { "MarkThreadResponse204",    flt_visit_handle_command, CFG_OPT_USER|CFG_OPT_CONFIG|CFG_OPT_LOCAL, NULL },
  { NULL, NULL, 0, NULL }
};

t_handler_config flt_visited_handlers[] = {
  { INIT_HANDLER,         flt_visited_init_handler   },
  { CONNECT_INIT_HANDLER, flt_visited_execute_filter },
  { VIEW_INIT_HANDLER,    flt_visited_set_col        },
  { VIEW_HANDLER,         flt_visited_set_link       },
  { POSTING_HANDLER,      flt_visited_posting_handler},
  { VIEW_LIST_HANDLER,    flt_visited_mark_visited   },
  { AFTER_POST_HANDLER,   flt_visited_mark_own       },
  { 0, NULL }
};

t_module_config flt_visited = {
  flt_visited_config,
  flt_visited_handlers,
  flt_visited_validate,
  flt_visited_lm,
  NULL,
  flt_visited_cleanup
};

/* eof */
