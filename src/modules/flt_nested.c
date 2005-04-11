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

static u_char *flt_nested_tpl = NULL;
static u_char *flt_nested_pt_tpl = NULL;
static u_char *flt_nested_fn = NULL;

/* {{{ flt_nested_execute_filter */
int flt_nested_execute_filter(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_cl_thread *thread,t_cf_template *tpl) {
  t_cf_tpl_variable hash;

  u_char *qchars,*UserName,*tmp,*msgcnt,buff[256],tplname[256],*forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  t_name_value *cs,*st,*qc,*ms,*ss,*locale,*df,*dft,
    *open_thread,*open_post,*open_subtree,
    *close_subtree,*close_post,*close_thread,
    *rm = cfg_get_first_value(vc,forum_name,"ReadMode"),*lt,*fbase,*ps,*reg;
  size_t len,qclen,msgcntlen,
    open_thread_len,open_post_len,open_subtree_len,
    close_subtree_len,close_post_len,close_thread_len;
  t_string content,threadlist,allcnt;
  int utf8,ShowInvisible,slvl = -1,level = 0,first = 1,printed = 0;
  t_message *msg;

  t_cf_template pt_tpl;

  /* are we in the right read mode? */
  if(cf_strcmp(rm->values[0],"nested") != 0) return FLT_DECLINE;
  if(flt_nested_pt_tpl == NULL || flt_nested_tpl == NULL) return FLT_DECLINE;

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

  open_thread   = cfg_get_first_value(&fo_view_conf,forum_name,"OpenThread");
  open_post     = cfg_get_first_value(&fo_view_conf,forum_name,"OpenPosting");
  open_subtree  = cfg_get_first_value(&fo_view_conf,forum_name,"OpenSubtree");
  close_subtree = cfg_get_first_value(&fo_view_conf,forum_name,"CloseSubtree");
  close_post    = cfg_get_first_value(&fo_view_conf,forum_name,"ClosePosting");
  close_thread  = cfg_get_first_value(&fo_view_conf,forum_name,"CloseThread");

  open_thread_len   = strlen(open_thread->values[0]);
  open_post_len     = strlen(open_post->values[0]);
  open_subtree_len  = strlen(open_subtree->values[0]);
  close_subtree_len = strlen(close_subtree->values[0]);
  close_post_len    = strlen(close_post->values[0]);
  close_thread_len  = strlen(close_thread->values[0]);

  utf8 = cf_strcmp(cs->values[0],"UTF-8") == 0;
  /* }}} */

  if(utf8 || (qchars = htmlentities_charset_convert(qc->values[0],"UTF-8",cs->values[0],&qclen,0)) == NULL) {
    qchars = htmlentities(qc->values[0],0);
    qclen  = strlen(qchars);
  }

  cf_set_variable(tpl,cs,"title",thread->messages->subject.content,thread->messages->subject.len,1);
  cf_set_variable(tpl,cs,"name",thread->messages->author.content,thread->messages->author.len,1);
  if(thread->messages->category.len) cf_set_variable(tpl,cs,"category",thread->messages->category.content,thread->messages->category.len,1);

  cf_gen_tpl_name(tplname,256,flt_nested_pt_tpl);
  if(cf_tpl_init(&pt_tpl,tplname) != 0) return FLT_DECLINE;

  str_init(&allcnt);

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

  tmp = cf_get_link(fbase->values[0],0,0);
  cf_set_variable(&pt_tpl,cs,"forumbase",tmp,strlen(tmp),1);
  free(tmp);

  cf_set_variable(&pt_tpl,cs,"postscript",ps->values[0],strlen(ps->values[0]),1);
  cf_set_variable(&pt_tpl,cs,"regscript",reg->values[0],strlen(reg->values[0]),1);

  free(rm->values[0]);
  rm->values[0] = strdup("_NONE_");

  level = 0;
  for(msg=thread->messages;msg;msg=msg->next) {
    if(ShowInvisible == 0 && (msg->may_show == 0 || msg->invisible == 1)) continue;
    if(slvl == -1) slvl = msg->level;
    printed = 1;

    cf_tpl_var_init(&hash,TPL_VARIABLE_HASH);

    if(first) cf_tpl_hashvar_setvalue(&hash,"first",TPL_VARIABLE_INT,1);

    cf_set_variable_hash(&hash,cs,"title",msg->subject.content,msg->subject.len,1);
    cf_set_variable_hash(&hash,cs,"name",msg->author.content,msg->author.len,1);
    if(msg->email.len) cf_set_variable_hash(&hash,cs,"email",msg->email.content,msg->email.len,1);
    if(msg->hp.len) cf_set_variable_hash(&hash,cs,"link",msg->hp.content,msg->hp.len,1);
    if(msg->img.len) cf_set_variable_hash(&hash,cs,"image",msg->img.content,msg->img.len,1);
    if(msg->category.len) cf_set_variable_hash(&hash,cs,"category",msg->category.content,msg->category.len,1);

    tmp = cf_general_get_time(df->values[0],locale->values[0],&len,&msg->date);
    cf_set_variable_hash(&hash,cs,"time",tmp,len,1);
    free(tmp);

    /* {{{ generate html code for the message and the cite */
    /* ok -- lets convert the message to the target charset with html encoded */
    if(utf8 || (msgcnt = charset_convert_entities(msg->content.content,msg->content.len,"UTF-8",cs->values[0],&msgcntlen)) == NULL) {
      msgcnt    = strdup(msg->content.content);
      msgcntlen = msg->content.len;
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

    cf_tpl_hashvar_setvalue(&hash,"message",TPL_VARIABLE_STRING,content.content,content.len);
    /* }}} */


    str_cleanup(&content);
    free(msgcnt);

    len = snprintf(buff,256,"%llu",thread->tid);
    cf_set_variable_hash(&hash,cs,"tid",buff,len,0);
    len = snprintf(buff,256,"%llu",msg->mid);
    cf_set_variable_hash(&hash,cs,"mid",buff,len,0);

    cf_run_perpost_var_handlers(head,thread,msg,&hash);
    cf_run_posting_handlers(head,thread,&pt_tpl,vc);

    cf_tpl_setvar(&pt_tpl,"thread",&hash);
    if(thread->last == thread->messages) cf_tpl_setvalue(&pt_tpl,"last",TPL_VARIABLE_INT,1);

    if(msg->level < level) {
      for(;level>msg->level;level--) {
        str_chars_append(&allcnt,close_subtree->values[0],close_subtree_len);
        str_chars_append(&allcnt,close_post->values[0],close_post_len);
      }
    }

    level = msg->level;

    if(msg->next && cf_msg_has_answers(msg)) { /* this message has at least one answer */
      if(msg == thread->messages || first) str_chars_append(&allcnt,open_thread->values[0],open_thread_len);
      //else str_chars_append(&allcnt,open_post->values[0],open_post_len);
      str_chars_append(&allcnt,open_post->values[0],open_post_len);

      cf_tpl_parse_to_mem(&pt_tpl);
      str_str_append(&allcnt,&pt_tpl.parsed);

      str_chars_append(&allcnt,open_subtree->values[0],open_subtree_len);
      level++;
    }
    else {
      if(msg == thread->messages || first) str_chars_append(&allcnt,open_thread->values[0],open_thread_len);
      //else str_chars_append(&allcnt,open_post->values[0],open_post_len);
      str_chars_append(&allcnt,open_post->values[0],open_post_len);

      cf_tpl_parse_to_mem(&pt_tpl);
      str_str_append(&allcnt,&pt_tpl.parsed);

      str_chars_append(&allcnt,close_post->values[0],close_post_len);
      //if(msg == thread->messages || first) str_chars_append(&allcnt,ct->values[0],ct_l);
      //else str_chars_append(&allcnt,cp->values[0],cp_l);
    }

    pt_tpl.parsed.len = 0;
    first = 0;
  }

  if(printed) {
    for(;level>slvl;level--) {
      str_chars_append(&allcnt,close_subtree->values[0],close_subtree_len);
      str_chars_append(&allcnt,close_post->values[0],close_post_len);
    }

    //str_chars_append(&allcnt,close_post->values[0],close_post_len);
    str_chars_append(&allcnt,close_thread->values[0],close_thread_len);
  }

  //printf("allcnt: %s\n",allcnt.content);

  free(rm->values[0]);
  rm->values[0] = strdup("nested");

  cf_tpl_finish(&pt_tpl);

  cf_tpl_setvalue(tpl,"threads",TPL_VARIABLE_STRING,allcnt.content,allcnt.len);
  str_cleanup(&allcnt);

  if(cf_strcmp(st->values[0],"none") != 0) {
    cf_gen_threadlist(thread,head,&threadlist,st->values[0],lt->values[0],CF_MODE_THREADVIEW);
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
  { "PerThreadTemplate",   flt_nested_handle,  CFG_OPT_CONFIG|CFG_OPT_LOCAL, NULL },
  { NULL, NULL, 0, NULL }
};

t_handler_config flt_nested_handlers[] = {
  { RM_COLLECTORS_HANDLER, flt_nested_rm_collector },
  { POSTING_HANDLER,       flt_nested_execute_filter },
  { 0, NULL }
};

t_module_config flt_nested = {
  flt_nested_config,
  flt_nested_handlers,
  NULL,
  NULL,
  NULL,
  NULL
};

/* eof */
