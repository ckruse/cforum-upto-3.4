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

#include "readline.h"
#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "cfcgi.h"
#include "template.h"
#include "clientlib.h"
/* }}} */

/*
 * I think, a sequential search within a range of 512 is reasonable. I think, there are never more than
 * 20 or 30 values in it.
 *
 */
static u_int64_t saved_threads[512];  /* Maximum 512 opened/closed threads. This should be enough */
static int sclen = 0;
static t_string Cgi;
static int ThreadsOpenByDefault = -1;
static int UseJavaScript = 1;
static int OpenThreadIfNew = 0;

void parse_query_string(t_cf_hash *head,int cl) {
  u_char *val = cf_cgi_get(head,"o");
  u_char *pos = val;
  u_char buff[50];
  u_int64_t tid = 0;
  int i = 0;
  int len;

  if(val) {
    while((tid = strtoull(pos,(char **)&pos,10)) > 0) {
      pos += 1;
      saved_threads[sclen++] = tid;
    }
  }

  val = cf_cgi_get(head,"a");
  if(val) {
    if(cl) { /* threads are closed by default!! */
      if(cf_strcmp(val,"open") == 0) {
        val = cf_cgi_get(head,"t");
        if(val) {
          saved_threads[sclen++] = strtoull(val,NULL,10);
        }
      }
      else {
        val = cf_cgi_get(head,"t");
        if(val) {
          tid = strtoull(val,NULL,10);

          for(i=0;i<sclen;i++) {
            if(tid == saved_threads[i]) {
              saved_threads[i] = 0;
              break;
            }
          }
        }
      }
    }
    else {
      if(cf_strcmp(val,"open") == 0) {
        val = cf_cgi_get(head,"t");
        if(val) {
          tid = strtoull(val,NULL,10);

          for(i=0;i<sclen;i++) {
            if(tid == saved_threads[i]) {
              saved_threads[i] = 0;
              break;
            }
          }
        }
      }
      else {
        val = cf_cgi_get(head,"t");
        if(val) {
          saved_threads[sclen++] = strtoull(val,NULL,10);
        }
      }

    } /* end else */
  }

  for(i=0;i<sclen;i++) {
    if(saved_threads[i] != 0) {
      str_init(&Cgi);
      len = sprintf(buff,"%llu",saved_threads[i]);
      str_chars_append(&Cgi,buff,len);
      break;
    }
  }

  if(i+1 < sclen) {
    for(i++;i<sclen;i++) {
      if(saved_threads[i] != 0) {
        len = sprintf(buff,"%%2C%llu",saved_threads[i]);
        str_chars_append(&Cgi,buff,len);
      }
    }
  }

}

int execute_filter(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_cl_thread *thread,int mode) {
  int cl = !ThreadsOpenByDefault;
  t_name_value *vs = NULL;
  t_message *msg;
  int i = 0;
  u_char buff[500];
  u_char *UserName = cf_hash_get(GlobalValues,"UserName",8);
  t_mod_api is_visited;


  if(ThreadsOpenByDefault != -1 && mode == 0) {
    if(UserName) vs = cfg_get_first_value(dc,"UBaseURL");
    else         vs = cfg_get_first_value(dc,"BaseURL");

    if(!sclen && head) { // get me the list of opened threads
      parse_query_string(head,cl);
    }

    tpl_cf_setvar(&thread->messages->tpl,"openclose","1",1,0);

    /* user wants to use java script */
    if(UseJavaScript) {
      if(ThreadsOpenByDefault && OpenThreadIfNew == 0) {
        tpl_cf_setvar(&thread->messages->tpl,"UseJavaScript","1",1,0);
      }
    }

    i = sprintf(buff,"t%lld",thread->tid);
    tpl_cf_setvar(&thread->messages->tpl,"unanch",buff,i,0);

    if(cl) { /* threads are closed by default! */
      for(i=0;i<sclen;i++) {
        if(saved_threads[i] == thread->tid) {
          tpl_cf_setvar(&thread->messages->tpl,"open","1",1,0);
          i = sprintf(buff,"%s?t=%lld&a=close",vs->values[0],thread->tid);
          tpl_cf_setvar(&thread->messages->tpl,"link_oc",buff,i,1);

          if(Cgi.len) {
            tpl_cf_appendvar(&thread->messages->tpl,"link_oc","&o=",7);
            tpl_cf_appendvar(&thread->messages->tpl,"link_oc",Cgi.content,Cgi.len);
          }

          return FLT_DECLINE; /* thread is open */
        }
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
              tpl_cf_setvar(&thread->messages->tpl,"open","1",1,0);
              i = sprintf(buff,"%s?t=%lld&a=close",vs->values[0],thread->tid);
              tpl_cf_setvar(&thread->messages->tpl,"link_oc",buff,i,1);

              if(Cgi.len) {
                tpl_cf_appendvar(&thread->messages->tpl,"link_oc","&o=",7);
                tpl_cf_appendvar(&thread->messages->tpl,"link_oc",Cgi.content,Cgi.len);
              }

              return FLT_DECLINE;
            }
          }
        }
      }

      i = sprintf(buff,"%s?t=%lld&a=open",vs->values[0],thread->tid);
      tpl_cf_setvar(&thread->messages->tpl,"link_oc",buff,i,1);

      if(Cgi.len) {
        tpl_cf_appendvar(&thread->messages->tpl,"link_oc","&o=",7);
        tpl_cf_appendvar(&thread->messages->tpl,"link_oc",Cgi.content,Cgi.len);
      }

      msg = thread->messages->next;

      /* thread is closed */
      if(msg) {
        for(;msg;msg=msg->next) {
          msg->may_show = 0;
        }
      }
    }
    else { /* threads are open by default! */
      /* check, if the actual thread is in the closed threads list */
      for(i=0;i<sclen;i++) {
        if(thread->tid == saved_threads[i]) { /* this thread is closed */
          i = sprintf(buff,"%s?t=%lld&a=open",vs->values[0],thread->tid);
          tpl_cf_setvar(&thread->messages->tpl,"link_oc",buff,i,1);

          if(Cgi.len) {
            tpl_cf_appendvar(&thread->messages->tpl,"link_oc","&o=",7);
            tpl_cf_appendvar(&thread->messages->tpl,"link_oc",Cgi.content,Cgi.len);
          }

          msg = thread->messages->next;

          /* hide postings */
          if(msg) {
            for(;msg;msg=msg->next) {
              msg->may_show = 0;
            }
          }

          return FLT_DECLINE; /* thread is open */
        }
      }

      /* this thread must be open */
      tpl_cf_setvar(&thread->messages->tpl,"open","1",1,0);
      i = sprintf(buff,"%s?t=%lld&a=close",vs->values[0],thread->tid);
      tpl_cf_setvar(&thread->messages->tpl,"link_oc",buff,i,1);

      if(Cgi.len) {
        tpl_cf_appendvar(&thread->messages->tpl,"link_oc","&o=",7);
        tpl_cf_appendvar(&thread->messages->tpl,"link_oc",Cgi.content,Cgi.len);
      }
    }

    return FLT_OK;
  }

  return FLT_DECLINE;
}

int flt_oc_set_js(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_cf_template *begin,t_cf_template *end) {
  /* user wants to use java script */
  if(UseJavaScript) {
    if(ThreadsOpenByDefault && OpenThreadIfNew == 0) {
      tpl_cf_setvar(begin,"UseJavaScript","1",1,0);
      return FLT_OK;
    }
  }

  return FLT_DECLINE;
}

int get_conf(t_configfile *cfile,t_conf_opt *opt,u_char **args,int argnum) {
  if(*opt->name == 'T')   ThreadsOpenByDefault = cf_strcmp(args[0],"yes") == 0;
  else {
    if(*opt->name == 'O') OpenThreadIfNew = cf_strcmp(args[0],"yes") == 0;
    else                  UseJavaScript = cf_strcmp(args[0],"yes") == 0;
  }

  return 0;
}

t_conf_opt flt_openclose_config[] = {
  { "ThreadsOpenByDefault", get_conf, CFG_OPT_CONFIG|CFG_OPT_USER, NULL },
  { "UseJavaScript",        get_conf, CFG_OPT_CONFIG|CFG_OPT_USER, NULL },
  { "OpenThreadIfNew",      get_conf, CFG_OPT_CONFIG|CFG_OPT_USER, NULL },
  { NULL, NULL, 0, NULL }
};

t_handler_config flt_openclose_handlers[] = {
  { VIEW_HANDLER,      execute_filter },
  { VIEW_INIT_HANDLER, flt_oc_set_js  },
  { 0, NULL }
};

t_module_config flt_openclose = {
  flt_openclose_config,
  flt_openclose_handlers,
  NULL,
  NULL,
  NULL,
  NULL
};

/* eof */
