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

static u_char *flt_nested_pt_tpl = NULL;
static u_char *flt_nested_fn = NULL;

/* {{{ flt_nested_execute_filter */
int flt_nested_execute_filter(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_cl_thread *thread,t_cf_template *tpl) {
  t_cf_tpl_variable hash;

  u_char *qchars,*UserName,*tmp,*msgcnt,buff[256],tplname[256],*forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  t_name_value *cs,*st,*qc,*ms,*ss,*locale,*df,*dft,*ot,*op,*ost,*cst,*cp,*ct,*rm = cfg_get_first_value(vc,forum_name,"ReadMode"),*lt,*fbase,*ps,*reg;
  size_t len,qclen,msgcntlen,ot_l,op_l,ost_l,cst_l,cp_l,ct_l;
  t_string content,threadlist,allcnt;
  int utf8,ShowInvisible,slvl = -1,level = 0;
  t_message *msg;

  t_cf_template pt_tpl;

  /* are we in the right read mode? */
  if(cf_strcmp(rm->values[0],"nested") != 0) return FLT_DECLINE;

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

  ot  = cfg_get_first_value(&fo_view_conf,forum_name,"OpenThread");
  op  = cfg_get_first_value(&fo_view_conf,forum_name,"OpenPosting");
  ost = cfg_get_first_value(&fo_view_conf,forum_name,"OpenSubtree");
  cst = cfg_get_first_value(&fo_view_conf,forum_name,"CloseSubtree");
  cp  = cfg_get_first_value(&fo_view_conf,forum_name,"ClosePosting");
  ct  = cfg_get_first_value(&fo_view_conf,forum_name,"CloseThread");

  ot_l  = strlen(ot->values[0]);
  op_l  = strlen(op->values[0]);
  ost_l = strlen(ost->values[0]);
  cst_l = strlen(cst->values[0]);
  cp_l  = strlen(cp->values[0]);
  ct_l  = strlen(ct->values[0]);

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

  tmp = cf_get_link(fbase->values[0],NULL,0,0);
  cf_set_variable(&pt_tpl,cs,"forumbase",tmp,strlen(tmp),1);
  free(tmp);

  cf_set_variable(&pt_tpl,cs,"postscript",ps->values[0],strlen(ps->values[0]),1);
  cf_set_variable(&pt_tpl,cs,"regscript",reg->values[0],strlen(reg->values[0]),1);

  for(msg=thread->messages;msg;msg=msg->next) {
    if((msg->may_show && msg->invisible == 0) || ShowInvisible == 1) {
      if(slvl == -1) slvl = msg->level;

      if(msg->level < level) {
        for(;level>msg->level;level--) {
          str_chars_append(&allcnt,cst->values[0],cst_l);
          str_chars_append(&allcnt,cp->values[0],cp_l);
        }
      }

      level = msg->level;

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

      cf_tpl_setvar(&pt_tpl,"thread",&hash);
      if(thread->last == thread->messages) cf_tpl_setvalue(&pt_tpl,"last",TPL_VARIABLE_INT,1);

      if(msg->next && cf_msg_has_answers(msg)) { /* this message has at least one answer */
        if(msg == thread->messages) str_chars_append(&allcnt,ot->values[0],ot_l);
        else str_chars_append(&allcnt,op->values[0],op_l);

        cf_tpl_parse_to_mem(&pt_tpl);
        str_str_append(&allcnt,&pt_tpl.parsed);

        str_chars_append(&allcnt,ost->values[0],ost_l);

        level++;
      }
      else {
        if(msg == thread->messages) str_chars_append(&allcnt,ot->values[0],ot_l);
        else str_chars_append(&allcnt,op->values[0],op_l);

        cf_tpl_parse_to_mem(&pt_tpl);
        str_str_append(&allcnt,&pt_tpl.parsed);

        if(msg == thread->messages) str_chars_append(&allcnt,ct->values[0],ct_l);
        else str_chars_append(&allcnt,cp->values[0],cp_l);
      }

      pt_tpl.parsed.len = 0;
    }
  }

  cf_tpl_finish(&pt_tpl);

  for(;level > 1 && level>slvl+1;level--) {
    str_chars_append(&allcnt,cst->values[0],cst_l);
    str_chars_append(&allcnt,cp->values[0],cp_l);
  }
  if(level == 1 || level == slvl+1) {
    str_chars_append(&allcnt,cst->values[0],cst_l);
    str_chars_append(&allcnt,ct->values[0],ct_l);
  }

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

/* {{{ flt_nested_handle */
int flt_nested_handle(t_configfile *cfile,t_conf_opt *opt,const u_char *context,u_char **args,size_t argnum) {
  if(flt_nested_fn == NULL) flt_nested_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(flt_nested_fn,context) != 0) return 0;

  if(flt_nested_pt_tpl) free(flt_nested_pt_tpl);
  flt_nested_pt_tpl = strdup(args[0]);

  return 0;
}
/* }}} */

t_conf_opt flt_nested_config[] = {
  { "PerThreadTemplate", flt_nested_handle,  CFG_OPT_CONFIG|CFG_OPT_LOCAL|CFG_OPT_NEEDED, NULL },
  { NULL, NULL, 0, NULL }
};

t_handler_config flt_nested_handlers[] = {
  { POSTING_HANDLER,      flt_nested_execute_filter },
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
