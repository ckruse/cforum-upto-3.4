/**
 * \file flt_nested.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 *
 * This plugin handles a posting read request in list mode
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
#include <time.h>

#include <string.h>
#include <ctype.h>
#include <sys/types.h>

#include "readline.h"
#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "cfcgi.h"
#include "template.h"
#include "clientlib.h"
#include "charconvert.h"
#include "htmllib.h"
/* }}} */

static int flt_nested_inprogress = 0;

static u_char *flt_nested_tpl    = NULL;
static u_char *flt_nested_pt_tpl = NULL;
static u_char *flt_nested_fn     = NULL;

/* {{{ flt_nested_make_hierarchical */
void flt_nested_make_hierarchical(t_configuration *vc,t_cf_template *tpl,t_cl_thread *thread,t_hierarchical_node *msg,t_cf_hash *head,int first,int ShowInvisible,int utf8,t_cf_tpl_variable *hash,t_name_value *cs,t_name_value *df,t_name_value *locale,t_name_value *qc,t_name_value *ms,t_name_value *ss) {
  size_t len,msgcntlen,i;
  u_char *msgcnt,*tmp,buff[512];
  t_cf_tpl_variable my_hash,subposts;
  t_hierarchical_node *msg1;

  t_string content;

  cf_tpl_var_init(hash,TPL_VARIABLE_HASH);

  if(first) cf_tpl_hashvar_setvalue(hash,"first",TPL_VARIABLE_INT,1);

  cf_set_variable_hash(hash,cs,"title",msg->msg->subject.content,msg->msg->subject.len,1);
  cf_set_variable_hash(hash,cs,"name",msg->msg->author.content,msg->msg->author.len,1);
  if(msg->msg->email.len) cf_set_variable_hash(hash,cs,"email",msg->msg->email.content,msg->msg->email.len,1);
  if(msg->msg->hp.len) cf_set_variable_hash(hash,cs,"link",msg->msg->hp.content,msg->msg->hp.len,1);
  if(msg->msg->img.len) cf_set_variable_hash(hash,cs,"image",msg->msg->img.content,msg->msg->img.len,1);
  if(msg->msg->category.len) cf_set_variable_hash(hash,cs,"category",msg->msg->category.content,msg->msg->category.len,1);

  tmp = cf_general_get_time(df->values[0],locale->values[0],&len,&msg->msg->date);
  cf_set_variable_hash(hash,cs,"time",tmp,len,1);
  free(tmp);

  /* {{{ generate html code for the message and the cite */
  /* ok -- lets convert the message to the target charset with html encoded */
  if(utf8 || (msgcnt = charset_convert_entities(msg->msg->content.content,msg->msg->content.len,"UTF-8",cs->values[0],&msgcntlen)) == NULL) {
    msgcnt    = strdup(msg->msg->content.content);
    msgcntlen = msg->msg->content.len;
  }

  str_init(&content);

  msg_to_html(
    thread,
    msgcnt,
    &content,
    NULL,
    qc->values[0],
    ms ? atoi(ms->values[0]) : -1,
    ss ? cf_strcmp(ss->values[0],"yes") == 0 : 0
  );

  cf_tpl_hashvar_setvalue(hash,"message",TPL_VARIABLE_STRING,content.content,content.len);
  /* }}} */

  str_cleanup(&content);
  free(msgcnt);

  len = snprintf(buff,256,"%llu",thread->tid);
  cf_set_variable_hash(hash,cs,"tid",buff,len,0);
  len = snprintf(buff,256,"%llu",msg->msg->mid);
  cf_set_variable_hash(hash,cs,"mid",buff,len,0);

  cf_run_perpost_var_handlers(head,thread,msg->msg,hash);
  cf_run_posting_handlers(head,thread,tpl,vc);

  if(msg->msg->next == NULL) cf_tpl_hashvar_setvalue(hash,"last",TPL_VARIABLE_INT,1);
  
  if(msg->childs.elements) { /* this message has at least one answer */
    cf_tpl_var_init(&subposts,TPL_VARIABLE_ARRAY);

    for(i=0;i<msg->childs.elements;++i) {
      msg1 = array_element_at(&msg->childs,i);

      if(ShowInvisible || (msg1->msg->may_show == 1 && msg1->msg->invisible == 0)) {
        cf_tpl_var_init(&my_hash,TPL_VARIABLE_HASH);
        flt_nested_make_hierarchical(vc,tpl,thread,msg1,head,0,ShowInvisible,utf8,&my_hash,cs,df,locale,qc,ms,ss);
        cf_tpl_var_add(&subposts,&my_hash);
      }
      else {
        if((msg1 = cf_msg_ht_get_first_visible(msg1)) != NULL) {
          cf_tpl_var_init(&my_hash,TPL_VARIABLE_HASH);
          flt_nested_make_hierarchical(vc,tpl,thread,msg1,head,0,ShowInvisible,utf8,&my_hash,cs,df,locale,qc,ms,ss);
          cf_tpl_var_add(&subposts,&my_hash);
        }
      }
    }

    cf_tpl_hashvar_set(hash,"subposts",&subposts);
  }
}
/* }}} */

void flt_nested_start_hierarchical(t_configuration *vc,t_cf_hash *head,t_cf_template *tpl,t_cl_thread *thread,int ShowInvisible,int utf8,t_cf_tpl_variable *hash,t_name_value *cs,t_name_value *df,t_name_value *locale,t_name_value *qc,t_name_value *ms,t_name_value *ss) {
  t_cf_tpl_variable ary,l_hash;
  t_hierarchical_node *ht = thread->ht,*ht1;
  size_t i;
  int did_push = 0,first = 1;

  if(ShowInvisible || (ht->msg->invisible == 0 && ht->msg->may_show)) {
    flt_nested_make_hierarchical(vc,tpl,thread,ht,head,1,ShowInvisible,utf8,hash,cs,df,locale,qc,ms,ss);
  }
  else {
    if(ht->childs.elements) {
      cf_tpl_var_init(&ary,TPL_VARIABLE_ARRAY);

      for(i=0;i<ht->childs.elements;++i) {
        ht1 = array_element_at(&ht->childs,i);

        if(ht1->msg->invisible == 0 && ht1->msg->may_show) {
          if(first) flt_nested_make_hierarchical(vc,tpl,thread,ht1,head,first,ShowInvisible,utf8,hash,cs,df,locale,qc,ms,ss);
          else {
            cf_tpl_var_init(&l_hash,TPL_VARIABLE_HASH);
            flt_nested_make_hierarchical(vc,tpl,thread,ht1,head,first,ShowInvisible,utf8,&l_hash,cs,df,locale,qc,ms,ss);
            cf_tpl_var_add(&ary,&l_hash);
          }

          did_push = 1;
          first    = 0;
        }
        else {
          if((ht1 = cf_msg_ht_get_first_visible(ht1)) != NULL) {
            if(first) flt_nested_make_hierarchical(vc,tpl,thread,ht1,head,first,ShowInvisible,utf8,hash,cs,df,locale,qc,ms,ss);
            else {
              cf_tpl_var_init(&l_hash,TPL_VARIABLE_HASH);
              flt_nested_make_hierarchical(vc,tpl,thread,ht1,head,first,ShowInvisible,utf8,&l_hash,cs,df,locale,qc,ms,ss);
              cf_tpl_var_add(&ary,&l_hash);
            }

            did_push = 1;
            first    = 0;
          }
        }
      }

      if(did_push) cf_tpl_hashvar_set(hash,"subposts",&ary);
    }
  }

}

/* {{{ flt_nested_execute_filter */
int flt_nested_execute_filter(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_cl_thread *thread,t_cf_template *tpl) {
  t_cf_tpl_variable hash;

  u_char *qchars,*UserName,*tmp,*msgcnt,buff[256],tplname[256],*forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  t_name_value *cs,*st,*qc,*ms,*ss,*locale,*df,*dft,
    *rm = cfg_get_first_value(vc,forum_name,"ReadMode"),*lt,*fbase,*ps,*reg;
  size_t len,qclen,msgcntlen;
  t_string content,threadlist;
  int utf8,ShowInvisible,slvl = -1,level = 0,first = 1,printed = 0;
  t_message *msg;
  cf_readmode_t *rm_infos = cf_hash_get(GlobalValues,"RM",2);

  /* are we in the right read mode? */
  if(cf_strcmp(rm->values[0],"nested") != 0) return FLT_DECLINE;
  if(flt_nested_tpl == NULL) return FLT_DECLINE;
  if(flt_nested_inprogress) return FLT_DECLINE;

  /* {{{ init some variables */
  UserName = cf_hash_get(GlobalValues,"UserName",8);
  ShowInvisible = cf_hash_get(GlobalValues,"ShowInvisible",13) == NULL ? 0 : 1;

  cs = cfg_get_first_value(dc,forum_name,"ExternCharset");
  st = cfg_get_first_value(vc,forum_name,"ShowThread");
  qc = cfg_get_first_value(vc,forum_name,"QuotingChars");
  ms = cfg_get_first_value(vc,forum_name,"MaxSigLines");
  ss = cfg_get_first_value(vc,forum_name,"ShowSig");
  locale = cfg_get_first_value(dc,forum_name,"DateLocale");
  df = cfg_get_first_value(vc,forum_name,"DateFormatThreadView");
  dft = cfg_get_first_value(vc,forum_name,"DateFormatThreadList");
  lt = cfg_get_first_value(dc,forum_name,UserName ? "UPostingURL_Nested" : "PostingURL_Nested");

  utf8 = cf_strcmp(cs->values[0],"UTF-8") == 0;
  /* }}} */

  if(utf8 || (qchars = htmlentities_charset_convert(qc->values[0],"UTF-8",cs->values[0],&qclen,0)) == NULL) {
    qchars = htmlentities(qc->values[0],0);
    qclen  = strlen(qchars);
  }

  cf_set_variable(tpl,cs,"title",thread->messages->subject.content,thread->messages->subject.len,1);
  cf_set_variable(tpl,cs,"name",thread->messages->author.content,thread->messages->author.len,1);
  if(thread->messages->category.len) cf_set_variable(tpl,cs,"category",thread->messages->category.content,thread->messages->category.len,1);

  if(UserName) {
    fbase = cfg_get_first_value(&fo_default_conf,forum_name,"UBaseURL");
    ps = cfg_get_first_value(&fo_default_conf,forum_name,"UPostScript");
    reg = cfg_get_first_value(&fo_default_conf,forum_name,"UserConfig");
  }
  else {
    fbase = cfg_get_first_value(&fo_default_conf,forum_name,"BaseURL");
    ps = cfg_get_first_value(&fo_default_conf,forum_name,"PostScript");
    reg = cfg_get_first_value(&fo_default_conf,forum_name,"UserRegister");
  }

  flt_nested_inprogress = 1;
  flt_nested_make_hierarchical(vc,tpl,thread,thread->ht,head,1,ShowInvisible,utf8,&hash,cs,df,locale,qc,ms,ss);

  cf_tpl_setvar(tpl,"thread",&hash);

  if(cf_strcmp(st->values[0],"none") != 0) {
    cf_gen_threadlist(thread,head,&threadlist,rm_infos->thread_posting_tpl,st->values[0],lt->values[0],CF_MODE_THREADVIEW);
    cf_tpl_setvalue(tpl,"threadlist",TPL_VARIABLE_STRING,threadlist.content,threadlist.len);
    str_cleanup(&threadlist);
  }

  return FLT_OK;
}
/* }}} */

/* {{{ flt_nested_rm_collector */
int flt_nested_rm_collector(t_cf_hash *head,t_configuration *dc,t_configuration *vc,cf_readmode_t *rm_infos) {
  u_char *fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);

  t_name_value *rm = cfg_get_first_value(vc,fn,"ReadMode");
  t_name_value *v;

  u_char buff[256];

  if(cf_strcmp(rm->values[0],"nested") == 0) {
    v = cfg_get_first_value(dc,fn,"PostingURL_Nested");
    rm_infos->posting_uri[0] = v->values[0];

    v = cfg_get_first_value(dc,fn,"UPostingURL_Nested");
    rm_infos->posting_uri[1] = v->values[0];

    if((v = cfg_get_first_value(vc,fn,"TemplateForumBegin")) != NULL) {
      cf_gen_tpl_name(buff,256,v->values[0]);
      rm_infos->pre_threadlist_tpl = strdup(buff);
    }

    if((v = cfg_get_first_value(vc,fn,"TemplateForumThread")) != NULL) {
      cf_gen_tpl_name(buff,256,v->values[0]);
      rm_infos->thread_posting_tpl = rm_infos->threadlist_thread_tpl = strdup(buff);
    }

    if((v = cfg_get_first_value(vc,fn,"TemplateForumEnd")) != NULL) {
      cf_gen_tpl_name(buff,256,v->values[0]);
      rm_infos->post_threadlist_tpl = strdup(buff);
    }

    if(flt_nested_tpl) {
      cf_gen_tpl_name(buff,256,flt_nested_tpl);
      rm_infos->thread_tpl = strdup(buff);
    }

    return FLT_OK;
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_nested_handle */
int flt_nested_handle(t_configfile *cfile,t_conf_opt *opt,const u_char *context,u_char **args,size_t argnum) {
  if(flt_nested_fn == NULL) flt_nested_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(flt_nested_fn,context) != 0) return 0;

  if(cf_strcmp(opt->name,"TemplateForumNested") == 0) {
    if(flt_nested_tpl) free(flt_nested_tpl);
    flt_nested_tpl = strdup(args[0]);
  }
  else if(cf_strcmp(opt->name,"PerThreadTemplate") == 0) {
    if(flt_nested_pt_tpl) free(flt_nested_pt_tpl);
    flt_nested_pt_tpl = strdup(args[0]);
  }

  return 0;
}
/* }}} */

t_conf_opt flt_nested_config[] = {
  { "TemplateForumNested", flt_nested_handle,  CFG_OPT_CONFIG|CFG_OPT_LOCAL, NULL },
  { NULL, NULL, 0, NULL }
};

t_handler_config flt_nested_handlers[] = {
  { RM_COLLECTORS_HANDLER, flt_nested_rm_collector },
  { POSTING_HANDLER,       flt_nested_execute_filter },
  { 0, NULL }
};

t_module_config flt_nested = {
  MODULE_MAGIC_COOKIE,
  flt_nested_config,
  flt_nested_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};

/* eof */
