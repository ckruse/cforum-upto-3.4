/**
 * \file flt_list.c
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

/* {{{ flt_list_execute_filter */
int flt_list_execute_filter(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_cl_thread *thread,t_cf_template *tpl) {
  t_cf_tpl_variable array,hash;

  u_char *qchars,*UserName,*tmp,*msgcnt,buff[256],*forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  t_name_value *cs,*st,*qc,*ms,*ss,*locale,*df,*dft,*rm = cfg_get_first_value(vc,forum_name,"ReadMode"),*lt;
  size_t len,qclen,msgcntlen;
  t_string content,threadlist;
  int utf8,ShowInvisible;
  t_message *msg,*msg1;

  /* are we in the right read mode? */
  if(cf_strcmp(rm->values[0],"list") != 0) return FLT_DECLINE;

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
  lt = cfg_get_first_value(dc,forum_name,UserName ? "UPostingURL_List" : "PostingURL_List");

  utf8 = cf_strcmp(cs->values[0],"UTF-8") == 0;
  /* }}} */

  if(utf8 || (qchars = htmlentities_charset_convert(qc->values[0],"UTF-8",cs->values[0],&qclen,0)) == NULL) {
    qchars = htmlentities(qc->values[0],0);
    qclen  = strlen(qchars);
  }

  cf_set_variable(tpl,cs,"title",thread->messages->subject.content,thread->messages->subject.len,1);
  cf_set_variable(tpl,cs,"name",thread->messages->author.content,thread->messages->author.len,1);
  if(thread->messages->category.len) cf_set_variable(tpl,cs,"category",thread->messages->category.content,thread->messages->category.len,1);

  cf_tpl_var_init(&array,TPL_VARIABLE_ARRAY);

  for(msg=thread->messages;msg;msg=msg->next) {
    if((msg->may_show && msg->invisible == 0) || ShowInvisible == 1) {
      cf_tpl_var_init(&hash,TPL_VARIABLE_HASH);

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
      cf_tpl_var_add(&array,&hash);
    }
  }

  cf_tpl_setvar(tpl,"threads",&array);

  if(cf_strcmp(st->values[0],"none") != 0) {
    cf_gen_threadlist(thread,head,&threadlist,st->values[0],lt->values[0],CF_MODE_THREADVIEW);
    cf_tpl_setvalue(tpl,"threadlist",TPL_VARIABLE_STRING,threadlist.content,threadlist.len);
    str_cleanup(&threadlist);
  }

  return FLT_OK;
}
/* }}} */

/* {{{ flt_list_rm_collector */
int flt_list_rm_collector(t_cf_hash *head,t_configuration *dc,t_configuration *vc,cf_readmode_t *rm_infos) {
  u_char *fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);

  t_name_value *rm = cfg_get_first_value(vc,fn,"ReadMode");
  t_name_value *v;

  u_char buff[256];

  if(cf_strcmp(rm->values[0],"list") == 0) {
    v = cfg_get_first_value(dc,fn,"PostingURL_List");
    rm_infos->posting_uri[0] = v->values[0];

    v = cfg_get_first_value(dc,fn,"UPostingURL_List");
    rm_infos->posting_uri[1] = v->values[0];

    v = cfg_get_first_value(vc,fn,"TemplateForumBegin");
    cf_gen_tpl_name(buff,256,v->values[0]);
    rm_infos->pre_threadlist_tpl = strdup(buff);

    v = cfg_get_first_value(vc,fn,"TemplateForumThread");
    cf_gen_tpl_name(buff,256,v->values[0]);
    rm_infos->thread_posting_tpl = rm_infos->threadlist_thread_tpl = strdup(buff);

    v = cfg_get_first_value(vc,fn,"TemplateForumEnd");
    cf_gen_tpl_name(buff,256,v->values[0]);
    rm_infos->post_threadlist_tpl = strdup(buff);

    v = cfg_get_first_value(vc,fn,"TemplateForumList");
    cf_gen_tpl_name(buff,256,v->values[0]);
    rm_infos->thread_tpl = strdup(buff);

    return FLT_OK;
  }

  return FLT_DECLINE;
}
/* }}} */

t_conf_opt flt_list_config[] = {
  { NULL, NULL, 0, NULL }
};

t_handler_config flt_list_handlers[] = {
  { RM_COLLECTORS_HANDLER, flt_list_rm_collector },
  { POSTING_HANDLER,       flt_list_execute_filter },
  { 0, NULL }
};

t_module_config flt_list = {
  flt_list_config,
  flt_list_handlers,
  NULL,
  NULL,
  NULL,
  NULL
};

/* eof */
