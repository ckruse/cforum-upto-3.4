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
} Cfg = { 0, NULL, NULL, NULL, NULL, 0 };

/* module api function, checks if message has been visited */
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

/* module api function, mark a posting visited */
void *flt_visited_mark_visited(void *vmid) {
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

/*
 * Returns: int   one of the FLT_* values
 * Parameters:
 *   - t_cf_hash *head          a pointer to the cgi paramter list
 *   - t_configuration *dc      a pointer to the default configuration
 *   - t_configuration *vc      a pointer to the fo_view configuration
 *   - t_configuration *uc      a pointer to the user config
 *
 * This function is the api function for the "mark visited messages" plugin. It is a
 * t_filter_begin function
 *
 */
#ifndef CF_SHARED_MEM
int execute_filter(t_cf_hash *head,t_configuration *dc,t_configuration *vc,int sock) {
#else
int execute_filter(t_cf_hash *head,t_configuration *dc,t_configuration *vc,void *sock) {
#endif
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
  char buff[256];
  size_t len;

  if(uname && Cfg.VisitedFile) {
    memset(&key,0,sizeof(key));
    memset(&data,0,sizeof(data));

    data.data = one;
    data.size = sizeof(one);

    /* register module api */
    cf_register_mod_api_ent("flt_visited","is_visited",flt_visited_is_visited);
    cf_register_mod_api_ent("flt_visited","mark_visited",flt_visited_mark_visited);

    if(head && Cfg.HighlightVisitedPostings) {
      cmid = cf_cgi_get(head,"m");
      ctid = cf_cgi_get(head,"mv");

      /* we shall mark a whole thread visited */
      if(ctid) {
        tid = strtoull(ctid,NULL,10);
        memset(&tsd,0,sizeof(tsd));
        memset(&thread,0,sizeof(thread));

        if(tid) {
          #ifdef CF_SHARED_MEM
          ret = cf_get_message_through_shm(sock,&thread,NULL,tid,0,CF_KILL_DELETED);
          #else
          ret = cf_get_message_through_sock(sock,&tsd,&thread,NULL,tid,0,CF_KILL_DELETED);
          #endif

          if(ret == -1) {
            return FLT_DECLINE;
          }

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

          cleanup_struct(&thread);
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

int set_col(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_cf_template *begin,t_cf_template *end) {
  if(Cfg.VisitedPostingsColorF || Cfg.VisitedPostingsColorB) {
    tpl_cf_setvar(begin,"visitedcol","1",1,0);

    if(Cfg.VisitedPostingsColorF && *Cfg.VisitedPostingsColorF) {
      tpl_cf_setvar(begin,"visitedcolfg",Cfg.VisitedPostingsColorF,strlen(Cfg.VisitedPostingsColorF),0);
    }
    if(Cfg.VisitedPostingsColorB && *Cfg.VisitedPostingsColorB) {
      tpl_cf_setvar(begin,"visitedcolbg",Cfg.VisitedPostingsColorB,strlen(Cfg.VisitedPostingsColorB),0);
    }
  }

  return FLT_OK;
}

int posting_handler(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_cl_thread *thread,t_cf_template *tpl) {
  return set_col(head,dc,vc,tpl,NULL);
}

int mark_visited(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_message *msg,u_int64_t tid,int mode) {
  u_char *uname = cf_hash_get(GlobalValues,"UserName",8);
  DBT key,data;
  u_char buff[256];
  size_t len;


  if(uname && Cfg.VisitedFile && Cfg.HighlightVisitedPostings) {
    memset(&key,0,sizeof(key));
    memset(&data,0,sizeof(data));

    len = snprintf(buff,256,"%llu",msg->mid);
    key.data = buff;
    key.size = len;

    if(Cfg.db->get(Cfg.db,NULL,&key,&data,0) == 0) {
      tpl_cf_setvar(&msg->tpl,"visited","1",1,0);
    }

    return FLT_OK;
  }

  return FLT_DECLINE;
}

int mark_own_visited(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_message *msg,u_int64_t tid) {
  return mark_visited(head,dc,vc,msg,tid,0);
}

int flt_visit_handle_command(t_configfile *cf,t_conf_opt *opt,u_char **args,int argnum) {
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

  return 0;
}

#ifndef CF_SHARED_MEM
int flt_visited_validate(t_cf_hash *head,t_configuration *dc,t_configuration *vc,time_t last_modified,int sock) {
#else
int flt_visited_validate(t_cf_hash *head,t_configuration *dc,t_configuration *vc,time_t last_modified,void *sock) {
#endif
  struct stat st;
  char buff[256];

  if(Cfg.VisitedFile) {
    snprintf(buff,256,"%s.tm",Cfg.VisitedFile);
    if(stat(buff,&st) == -1) return FLT_DECLINE;
    if(st.st_mtime > last_modified) return FLT_EXIT;
  }

  return FLT_OK;
}

#ifndef CF_SHARED_MEM
time_t flt_visited_lm(t_cf_hash *head,t_configuration *dc,t_configuration *vc,int sock) {
#else
time_t flt_visited_lm(t_cf_hash *head,t_configuration *dc,t_configuration *vc,void *sock) {
#endif
  struct stat st;
  char buff[256];

  if(Cfg.VisitedFile) {
    snprintf(buff,256,"%s.tm",Cfg.VisitedFile);
    if(stat(buff,&st) == -1) return -1;
    return st.st_mtime;
  }


  return -1;
}

int flt_visited_init_handler(t_cf_hash *cgi,t_configuration *dc,t_configuration *vc) {
  int ret;

  if(Cfg.VisitedFile) {
    if((ret = db_create(&Cfg.db,NULL,0)) != 0) {
      fprintf(stderr,"DB error: %s\n",db_strerror(ret));
      return FLT_EXIT;
    }

    if((ret = Cfg.db->open(Cfg.db,NULL,Cfg.VisitedFile,NULL,DB_BTREE,DB_CREATE,0644)) != 0) {
      fprintf(stderr,"DB error: %s\n",db_strerror(ret));
      return FLT_EXIT;
    }

    return FLT_OK;
  }

  return FLT_DECLINE;
}

#ifndef CF_SHARED_MEM
int set_link(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_cl_thread *thr,int sock) {
#else
int set_link(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_cl_thread *thr,void *sock) {
#endif
  u_char buff[512];
  int UserName = cf_hash_get(GlobalValues,"UserName",8) != NULL;
  t_name_value *x = cfg_get_first_value(dc,UserName?"UBaseURL":"BaseURL");
  size_t len;

  if(Cfg.VisitedFile && Cfg.HighlightVisitedPostings) {
    len = snprintf(buff,512,"%s?mv=%llu",x->values[0],thr->tid);
    tpl_cf_setvar(&thr->messages->tpl,"mvlink",buff,len,1);
  }

  return FLT_OK;
}

void flt_visited_cleanup(void) {
  if(Cfg.VisitedPostingsColorF) free(Cfg.VisitedPostingsColorF);
  if(Cfg.VisitedPostingsColorB) free(Cfg.VisitedPostingsColorB);
  if(Cfg.VisitedFile)      free(Cfg.VisitedFile);

  if(Cfg.db) Cfg.db->close(Cfg.db,0);
}

t_conf_opt flt_visited_config[] = {
  { "HighlightVisitedPostings", flt_visit_handle_command, NULL },
  { "VisitedPostingsColors",    flt_visit_handle_command, NULL },
  { "VisitedFile",              flt_visit_handle_command, NULL },
  { "MarkOwnPostsVisited",      flt_visit_handle_command, NULL },
  { NULL, NULL, NULL }
};

t_handler_config flt_visited_handlers[] = {
  { INIT_HANDLER,         flt_visited_init_handler },
  { CONNECT_INIT_HANDLER, execute_filter           },
  { VIEW_INIT_HANDLER,    set_col                  },
  { VIEW_HANDLER,         set_link                 },
  { POSTING_HANDLER,      posting_handler          },
  { VIEW_LIST_HANDLER,    mark_visited             },
  { AFTER_POST_HANDLER,   mark_own_visited         },
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
