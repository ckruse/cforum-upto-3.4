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
#include "htmllib.h"
/* }}} */

static t_cf_hash *flt_oc_cookies = NULL;
static int ThreadsOpenByDefault = 1;
static int OpenThreadIfNew = 0;
static int UseJavaScript = 1;

static u_int64_t *flt_oc_opened = NULL;
static u_char *flt_oc_fn = NULL;
size_t flt_oc_opened_len = 0;

/* {{{ flt_oc_parse */
void flt_oc_parse(t_cf_hash *head) {
  u_char *pos,*val = cf_cgi_get(flt_oc_cookies,"o");

  u_int64_t tid;

  size_t i;

  if(val) {
    pos = val;
    while((tid = strtoull(pos,(char **)&pos,10)) > 0) {
      pos += 1;
      flt_oc_opened = fo_alloc(flt_oc_opened,sizeof(tid),++flt_oc_opened_len,FO_ALLOC_REALLOC);
      flt_oc_opened[flt_oc_opened_len-1] = tid;
    }
  }

  if(head) {
    if((val = cf_cgi_get(head,"a")) != NULL) {
      if(ThreadsOpenByDefault == 0) {
        if(cf_strcmp(val,"open") == 0) {
          if((val = cf_cgi_get(head,"oc_t")) != NULL) {
            if((tid = strtoull(val,NULL,10)) != 0) {
              flt_oc_opened = fo_alloc(flt_oc_opened,sizeof(tid),++flt_oc_opened_len,FO_ALLOC_REALLOC);
              flt_oc_opened[flt_oc_opened_len-1] = tid;
            }
          }
        }
        else {
          if((val = cf_cgi_get(head,"oc_t")) != NULL) {
            if((tid = strtoull(val,NULL,10)) != 0) {
              for(i=0;i<flt_oc_opened_len;++i) {
                if(tid == flt_oc_opened[i]) {
                  flt_oc_opened[i] = 0;
                  break;
                }
              }
            }
          }
        }
      }
      else {
        if(cf_strcmp(val,"close") == 0) {
          if((val = cf_cgi_get(head,"oc_t")) != NULL) {
            if((tid = strtoull(val,NULL,10)) != 0) {
              flt_oc_opened = fo_alloc(flt_oc_opened,sizeof(tid),++flt_oc_opened_len,FO_ALLOC_REALLOC);
              flt_oc_opened[flt_oc_opened_len-1] = tid;
            }
          }
        }
        else {
          if((val = cf_cgi_get(head,"oc_t")) != NULL) {
            if((tid = strtoull(val,NULL,10)) != 0) {
              for(i=0;i<flt_oc_opened_len;++i) {
                if(tid == flt_oc_opened[i]) {
                  flt_oc_opened[i] = 0;
                  break;
                }
              }
            }
          }
        }
      }
    }
  }
}
/* }}} */

/* {{{ flt_oc_exec_xmlhttp */
#ifndef CF_SHARED_MEM
int flt_oc_exec_xmlhttp(t_cf_hash *cgi,t_configuration *dc,t_configuration *vc,int sock)
#else
int flt_oc_exec_xmlhttp(t_cf_hash *cgi,t_configuration *dc,t_configuration *vc,void *shm)
#endif
{
  u_char *val,*fn = cf_hash_get(GlobalValues,"FORUM_NAME",10),buff[512],*line,fo_thread_tplname[256];
  u_int64_t tid;
  t_cl_thread thread;
  size_t len;
  t_string str;
  rline_t rl;

  t_name_value *fo_thread_tpl,*cs,*ot,*ct;

  if(!cgi) return FLT_DECLINE;

  if((val = cf_cgi_get(cgi,"a")) != NULL && cf_strcmp(val,"open") == 0) {
    if((val = cf_cgi_get(cgi,"mode")) != NULL && cf_strcmp(val,"xmlhttp") == 0) {
      if((val = cf_cgi_get(cgi,"oc_t")) != NULL && (tid = str_to_u_int64(val)) != 0) {
        /* ok, all parameters are fine, go and generate content */
        fo_thread_tpl  = cfg_get_first_value(vc,fn,"TemplateForumThread");
        cs             = cfg_get_first_value(dc,fn,"ExternCharset");
        ot             = cfg_get_first_value(vc,fn,"OpenThread");
        ct             = cfg_get_first_value(vc,fn,"CloseThread");

        cf_gen_tpl_name(fo_thread_tplname,256,fo_thread_tpl->values[0]);

        memset(&thread,0,sizeof(thread));
        memset(&rl,0,sizeof(rl));

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

  if(mode & CF_MODE_PRE) return FLT_DECLINE;
  if(mode & CF_MODE_THREADVIEW) return FLT_DECLINE;

  if(flt_oc_fn == NULL) flt_oc_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  
  if(UserName) vs = cfg_get_first_value(dc,flt_oc_fn,"UBaseURL");
  else         vs = cfg_get_first_value(dc,flt_oc_fn,"BaseURL");

  if(flt_oc_opened_len == 0) flt_oc_parse(head);

  cf_tpl_setvalue(&thread->messages->tpl,"openclose",TPL_VARIABLE_INT,1);

  i = snprintf(buff,512,"t%llu",thread->tid);
  cf_tpl_setvalue(&thread->messages->tpl,"unanch",TPL_VARIABLE_STRING,buff,i);

  /* user wants to use java script */
  if(UseJavaScript) cf_tpl_setvalue(&thread->messages->tpl,"UseJavaScript",TPL_VARIABLE_INT,1);

  if(ThreadsOpenByDefault == 0) {
    for(i=0;i<flt_oc_opened_len;++i) {
      if(flt_oc_opened[i] == thread->tid) {
        cf_tpl_setvalue(&thread->messages->tpl,"open",TPL_VARIABLE_INT,1);
        i = snprintf(buff,512,"%s?oc_t=%lld&a=close",vs->values[0],thread->tid);
        cf_tpl_setvalue(&thread->messages->tpl,"link_oc",TPL_VARIABLE_STRING,buff,i);

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
    for(i=0;i<flt_oc_opened_len;++i) {
      if(thread->tid == flt_oc_opened[i]) { /* this thread is closed */
        i = snprintf(buff,512,"%s?oc_t=%lld&a=open",vs->values[0],thread->tid);
        cf_tpl_setvalue(&thread->messages->tpl,"link_oc",TPL_VARIABLE_STRING,buff,i,1);

        cf_msg_delete_subtree(thread->messages);

        return FLT_DECLINE; /* thread is open */
      }
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

/* {{{ flt_oc_headers */
#ifndef CF_SHARED_MEM
int flt_oc_headers(t_cf_hash *cgi,t_cf_hash *header_table,t_configuration *dc,t_configuration *vc,int sock)
#else
int flt_oc_headers(t_cf_hash *cgi,t_cf_hash *header_table,t_configuration *dc,t_configuration *vc,void *sock)
#endif
{
  t_string str;
  u_char *val;
  u_int64_t tid;
  size_t i;

  flt_oc_cookies = cf_hash_new(cf_cgi_destroy_entry);
  cf_cgi_parse_cookies(flt_oc_cookies);

  if(cgi) {
    flt_oc_parse(cgi);
    if((val = cf_cgi_get(cgi,"a")) != NULL) {
      if((val = cf_cgi_get(cgi,"oc_t")) != NULL) {
        if((tid = str_to_u_int64(val)) != 0) {
          str_init(&str);
          str_char_set(&str,"o=",2);
          for(i=0;i<flt_oc_opened_len;++i) {
            if(flt_oc_opened[i] == 0) continue;
            u_int64_to_str(&str,flt_oc_opened[i]);
            if(i != flt_oc_opened_len-1) str_chars_append(&str,"%2C",3);
          }
          str_chars_append(&str,"; Domain=",9);
          val = getenv("HTTP_HOST");
          str_chars_append(&str,val,strlen(val));
          str_chars_append(&str,"; Path=/",9);

          cf_hash_set(header_table,"Set-Cookie",10,str.content,str.len);
          str_cleanup(&str);
          return FLT_OK;
        }
      }
    }
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_deleted_validate */
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

  if(*opt->name == 'T')   ThreadsOpenByDefault = cf_strcmp(args[0],"yes") == 0;
  else {
    if(*opt->name == 'O') OpenThreadIfNew = cf_strcmp(args[0],"yes") == 0;
    else                  UseJavaScript = cf_strcmp(args[0],"yes") == 0;
  }

  return 0;
}
/* }}} */

void flt_oc_cleanup(void) {
  if(flt_oc_cookies) cf_hash_destroy(flt_oc_cookies);
  if(flt_oc_opened) free(flt_oc_opened);
}

t_conf_opt flt_openclose_config[] = {
  { "ThreadsOpenByDefault", flt_oc_get_conf, CFG_OPT_CONFIG|CFG_OPT_USER|CFG_OPT_LOCAL, NULL },
  { "UseJavaScript",        flt_oc_get_conf, CFG_OPT_CONFIG|CFG_OPT_USER|CFG_OPT_LOCAL, NULL },
  { "OpenThreadIfNew",      flt_oc_get_conf, CFG_OPT_CONFIG|CFG_OPT_USER|CFG_OPT_LOCAL, NULL },
  { NULL, NULL, 0, NULL }
};

t_handler_config flt_openclose_handlers[] = {
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
  flt_oc_headers,
  flt_oc_cleanup
};

/* eof */
