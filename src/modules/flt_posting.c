/**
 * \file flt_posting.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 *
 * This plugin handles a posting read request
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
#include "validate.h"
#include "htmllib.h"
/* }}} */

struct {
  u_char *Hi;
  u_char *Bye;
  u_char *Signature;
  u_char *TWidth;
  u_char *THeight;
  u_char *ActiveColorF;
  u_char *ActiveColorB;
} flt_posting_cfg = { NULL, NULL, NULL, NULL, NULL, NULL, NULL };

static u_char *flt_posting_fn = NULL;

/* {{{ flt_posting_replace_placeholders */
void flt_posting_replace_placeholders(const u_char *str,t_string *appender,t_cl_thread *thread,t_name_value *cs) {
  register u_char *ptr = (u_char *)str;
  register u_char *ptr1 = NULL;
  u_char *name,*tmp;
  size_t len;

  if(thread) {
    for(ptr1 = thread->threadmsg->author.content;*ptr1 && (!isspace(*ptr1) && *ptr1 != '-');++ptr1);
  }

  for(;*ptr;++ptr) {
    if(cf_strncmp(ptr,"\\n",2) == 0) {
      str_chars_append(appender,"\n",1);
      ptr += 1;
    }
    else if(cf_strncmp(ptr,"{$name}",7) == 0) {
      if(thread) name = htmlentities_charset_convert(thread->threadmsg->author.content,"UTF-8",cs->values[0],&len,0);
      else {
        name = strdup("alle");
        len  = 4;
      }

      str_chars_append(appender,name,len);
      ptr += 6;
      free(name);
    }
    else if(cf_strncmp(ptr,"{$vname}",8) == 0) {
      tmp = NULL;
      ptr += 7;
      if(thread) {
        tmp = strndup(thread->threadmsg->author.content,(size_t)(ptr1-thread->threadmsg->author.content));
        name = htmlentities_charset_convert(tmp,"UTF-8",cs->values[0],&len,0);
      }
      else {
        name = strdup("alle");
        len  = 4;
      }

      str_chars_append(appender,name,len);
      free(name);
      if(tmp) free(tmp);
    }
    else str_chars_append(appender,ptr,1);
  }
}
/* }}} */

/* {{{ flt_posting_execute_filter */
int flt_posting_execute_filter(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_cl_thread *thread,t_cf_template *tpl) {
  /* {{{ variables */
  u_char buff[256],
        *tmp,
        *qchars,
        *msgcnt,
        *date,
        *link,
        *UserName = cf_hash_get(GlobalValues,"UserName",8),
        *forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);

  t_name_value *ps,
               *cs = cfg_get_first_value(dc,forum_name,"ExternCharset"),
               *rm = cfg_get_first_value(vc,forum_name,"ReadMode"),
               *dq = cfg_get_first_value(vc,forum_name,"DoQuote"),
               *st = cfg_get_first_value(vc,forum_name,"ShowThread"),
               *qc = cfg_get_first_value(vc,forum_name,"QuotingChars"),
               *ms = cfg_get_first_value(vc,forum_name,"MaxSigLines"),
               *ss = cfg_get_first_value(vc,forum_name,"ShowSig"),
               *locale = cfg_get_first_value(dc,forum_name,"DateLocale"),
               *df = cfg_get_first_value(vc,forum_name,"DateFormatThreadView"),
               *dft = cfg_get_first_value(vc,forum_name,"DateFormatThreadList"),
               *ot  = cfg_get_first_value(&fo_view_conf,forum_name,"OpenThread"),
               *op  = cfg_get_first_value(&fo_view_conf,forum_name,"OpenPosting"),
               *ost = cfg_get_first_value(&fo_view_conf,forum_name,"OpenSubtree"),
               *cst = cfg_get_first_value(&fo_view_conf,forum_name,"CloseSubtree"),
               *cp  = cfg_get_first_value(&fo_view_conf,forum_name,"ClosePosting"),
               *ct  = cfg_get_first_value(&fo_view_conf,forum_name,"CloseThread"),
               *v;

  size_t len,
         qclen,
         msgcntlen,
         ot_l  = strlen(ot->values[0]),
         op_l  = strlen(op->values[0]),
         ost_l = strlen(ost->values[0]),
         cst_l = strlen(cst->values[0]),
         cp_l  = strlen(cp->values[0]),
         ct_l  = strlen(ct->values[0]);

  t_string cite,content,threadlist;

  int level = 0,
      slvl = -1,
      rc,
      utf8,
      ShowInvisible = cf_hash_get(GlobalValues,"ShowInvisible",13) == NULL ? 0 : 1;

  t_message *msg;
  /* }}} */

  /* are we in the right read mode? */
  if(cf_strcmp(rm->values[0],"thread")) return FLT_DECLINE;

  utf8 = cf_strcmp(cs->values[0],"UTF-8") == 0;

  if(utf8 || (qchars = htmlentities_charset_convert(qc->values[0],"UTF-8",cs->values[0],&qclen,0)) == NULL) {
    qchars = htmlentities(qc->values[0],0);
    qclen  = strlen(qchars);
  }

  if(UserName) ps = cfg_get_first_value(dc,forum_name,"UPostScript");
  else         ps = cfg_get_first_value(dc,forum_name,"PostScript");

  /* {{{ set some standard variables in thread mode */
  if(flt_posting_cfg.TWidth) cf_tpl_setvalue(tpl,"twidth",TPL_VARIABLE_STRING,flt_posting_cfg.TWidth,strlen(flt_posting_cfg.TWidth));
  if(flt_posting_cfg.THeight) cf_tpl_setvalue(tpl,"theight",TPL_VARIABLE_STRING,flt_posting_cfg.THeight,strlen(flt_posting_cfg.THeight));

  if(flt_posting_cfg.ActiveColorF || flt_posting_cfg.ActiveColorB) {
    cf_tpl_setvalue(tpl,"activecolor",TPL_VARIABLE_STRING,"1",1);

    if(flt_posting_cfg.ActiveColorF && *flt_posting_cfg.ActiveColorF) cf_set_variable(tpl,cs,"activecolorf",flt_posting_cfg.ActiveColorF,strlen(flt_posting_cfg.ActiveColorF),1);
    if(flt_posting_cfg.ActiveColorB && *flt_posting_cfg.ActiveColorB) cf_set_variable(tpl,cs,"activecolorb",flt_posting_cfg.ActiveColorB,strlen(flt_posting_cfg.ActiveColorB),1);
  }

  cf_set_variable(tpl,cs,"action",ps->values[0],strlen(ps->values[0]),1);

  cf_tpl_setvalue(tpl,"qchar",TPL_VARIABLE_STRING,"&#255;",6);
  cf_tpl_appendvalue(tpl,"qchar",qchars,qclen);

  len = sprintf(buff,"%llu,%llu",thread->tid,thread->threadmsg->mid);
  cf_tpl_setvalue(tpl,"fupto",TPL_VARIABLE_STRING,buff,len);

  len = gen_unid(buff,50);
  cf_tpl_setvalue(tpl,"unid",TPL_VARIABLE_STRING,buff,len);
  /* }}} */

  if((v = cfg_get_first_value(vc,forum_name,"Name")) != NULL) cf_set_variable(tpl,cs,"aname",v->values[0],strlen(v->values[0]),1);
  if((v = cfg_get_first_value(vc,forum_name,"EMail")) != NULL) cf_set_variable(tpl,cs,"aemail",v->values[0],strlen(v->values[0]),1);
  if((v = cfg_get_first_value(vc,forum_name,"HomepageUrl")) != NULL) cf_set_variable(tpl,cs,"aurl",v->values[0],strlen(v->values[0]),1);
  if((v = cfg_get_first_value(vc,forum_name,"ImageUrl")) != NULL) cf_set_variable(tpl,cs,"aimg",v->values[0],strlen(v->values[0]),1);

  /* {{{ set title, name, email, homepage, time and category */
  cf_set_variable(tpl,cs,"title",thread->threadmsg->subject.content,thread->threadmsg->subject.len,1);
  cf_set_variable(tpl,cs,"name",thread->threadmsg->author.content,thread->threadmsg->author.len,1);
  if(thread->threadmsg->email.len) cf_set_variable(tpl,cs,"email",thread->threadmsg->email.content,thread->threadmsg->email.len,1);
  if(thread->threadmsg->hp.len) cf_set_variable(tpl,cs,"link",thread->threadmsg->hp.content,thread->threadmsg->hp.len,1);
  if(thread->threadmsg->img.len) cf_set_variable(tpl,cs,"image",thread->threadmsg->img.content,thread->threadmsg->img.len,1);
  if(thread->threadmsg->category.len) cf_set_variable(tpl,cs,"category",thread->threadmsg->category.content,thread->threadmsg->category.len,1);

  tmp = cf_general_get_time(df->values[0],locale->values[0],&len,&thread->threadmsg->date);
  cf_set_variable(tpl,cs,"time",tmp,len,1);
  free(tmp);
  /* }}} */

  /* {{{ has this posting parent postings?
   * If yes, set some variables
   */
  if(thread->threadmsg != thread->messages) {
    t_message *msg;
    if((msg = cf_msg_get_parent(thread->threadmsg)) != NULL) {
      tmp = cf_general_get_time(df->values[0],locale->values[0],&len,&msg->date);

      cf_tpl_setvalue(tpl,"messagebefore",TPL_VARIABLE_STRING,"1",1);
      cf_set_variable(tpl,cs,"b_name",msg->author.content,msg->author.len,1);
      cf_set_variable(tpl,cs,"b_title",msg->subject.content,msg->subject.len,1);
      cf_set_variable(tpl,cs,"b_time",tmp,len,1);

      free(tmp);

      tmp = cf_get_link(NULL,forum_name,thread->tid,msg->mid);
      cf_set_variable(tpl,cs,"b_link",tmp,strlen(tmp),1);
      free(tmp);

      if(msg->category.len) cf_set_variable(tpl,cs,"b_category",msg->category.content,msg->category.len,1);
    }
  }
  /* }}} */

  /* {{{ generate html code for the message and the cite */
  /* ok -- lets convert the message to the target charset with html encoded */
  if(utf8 || (msgcnt = charset_convert_entities(thread->threadmsg->content.content,thread->threadmsg->content.len,"UTF-8",cs->values[0],&msgcntlen)) == NULL) {
    msgcnt    = strdup(thread->threadmsg->content.content);
    msgcntlen = thread->threadmsg->content.len;
  }

  str_init(&content);
  str_init(&cite);

  msg_to_html(
    thread,
    msgcnt,
    &content,
    cf_strcmp(dq->values[0],"yes") == 0 ? &cite : NULL,
    qc->values[0],
    ms ? atoi(ms->values[0]) : -1,
    ss ? cf_strcmp(ss->values[0],"yes") == 0 : 0
  );

  cf_tpl_setvalue(tpl,"message",TPL_VARIABLE_STRING,content.content,content.len);
  if(cf_strcmp(dq->values[0],"yes") == 0) cf_tpl_setvalue(tpl,"cite",TPL_VARIABLE_STRING,cite.content,cite.len);
  /* }}} */

  str_cleanup(&cite);
  str_cleanup(&content);
  free(msgcnt);
  free(qchars);

  /* {{{ generate thread list */
  if(cf_strcmp(st->values[0],"none") != 0) {
    if(cf_strcmp(st->values[0],"partitial") == 0) {
      for(msg=thread->messages;msg && msg->mid != thread->threadmsg->mid;msg=msg->next) msg->may_show = 0;

      level = msg->level;
      msg->may_show = 0;

      for(msg=msg->next;msg && msg->level > level;msg=msg->next);
      for(;msg;msg=msg->next) msg->may_show = 0;
    }
    else cf_tpl_setvalue(&thread->threadmsg->tpl,"active",TPL_VARIABLE_STRING,"1",1);

    /* {{{ run handlers in pre and post mode */
    cf_run_view_handlers(thread,head,CF_MODE_THREADVIEW|CF_MODE_PRE);
    for(msg=thread->messages;msg;msg=msg->next) cf_run_view_list_handlers(msg,head,thread->tid,CF_MODE_THREADVIEW);
    cf_run_view_handlers(thread,head,CF_MODE_THREADVIEW|CF_MODE_POST);
    /* }}} */

    str_init(&threadlist);
    for(msg=thread->messages;msg;msg=msg->next) {
      if((msg->may_show && msg->invisible == 0) || ShowInvisible == 1) {
        if(slvl == -1) slvl = msg->level;

        date = cf_general_get_time(dft->values[0],locale->values[0],&len,&msg->date);
        link = cf_get_link(NULL,forum_name,thread->tid,msg->mid);

        cf_set_variable(&msg->tpl,cs,"author",msg->author.content,msg->author.len,1);
        cf_set_variable(&msg->tpl,cs,"title",msg->subject.content,msg->subject.len,1);

        if(msg->category.len) cf_set_variable(&msg->tpl,cs,"category",msg->category.content,msg->category.len,1);

        if(date) {
          cf_set_variable(&msg->tpl,cs,"time",date,len,1);
          free(date);
        }

        if(link) {
          cf_set_variable(&msg->tpl,cs,"link",link,strlen(link),1);
          free(link);
        }

        if(msg->level < level) {
          for(;level>msg->level;level--) str_chars_append(&threadlist,"</ul></li>",10);
        }

        level = msg->level;

        if(msg->next && cf_msg_has_answers(msg)) { /* this message has at least one answer */
          if(msg == thread->messages) str_chars_append(&threadlist,ot->values[0],ot_l);
          else str_chars_append(&threadlist,op->values[0],op_l);

          cf_tpl_parse_to_mem(&msg->tpl);
          str_str_append(&threadlist,&msg->tpl.parsed);

          str_chars_append(&threadlist,ost->values[0],ost_l);

          level++;
        }
        else {
          if(msg == thread->messages) str_chars_append(&threadlist,ot->values[0],ot_l);
          else str_chars_append(&threadlist,op->values[0],op_l);

          cf_tpl_parse_to_mem(&msg->tpl);
          str_str_append(&threadlist,&msg->tpl.parsed);

          if(msg == thread->messages) str_chars_append(&threadlist,ct->values[0],ct_l);
          else str_chars_append(&threadlist,cp->values[0],cp_l);
        }
      }
    }

    for(;level > 1 && level>slvl+1;level--) {
      str_chars_append(&threadlist,cst->values[0],cst_l);
      str_chars_append(&threadlist,cp->values[0],cp_l);
    }
    if(level == 1 || level == slvl+1) {
      str_chars_append(&threadlist,cst->values[0],cst_l);
      str_chars_append(&threadlist,ct->values[0],ct_l);
    }

    cf_tpl_setvalue(tpl,"threadlist",TPL_VARIABLE_STRING,threadlist.content,threadlist.len);
    str_cleanup(&threadlist);
  }
  /* }}} */

  return FLT_OK;
}
/* }}} */

/* {{{ flt_posting_post_display */
int flt_posting_post_display(t_cf_hash *head,t_configuration *dc,t_configuration *pc,t_cf_template *tpl,t_message *p) {
  u_char *forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  t_name_value *v;
  t_name_value *cs = cfg_get_first_value(dc,forum_name,"ExternCharset");
  t_name_value *qc = cfg_get_first_value(pc,forum_name,"QuotingChars");
  t_string body,tmp;

  t_cl_thread thr;

  if(head) {
    /* set if none of the values have been given */
    if(!cf_cgi_get(head,"Name") && !cf_cgi_get(head,"EMail") && !cf_cgi_get(head,"HomepageUrl") && !cf_cgi_get(head,"ImageUrl") && !cf_cgi_get(head,"body")) {
      if((v = cfg_get_first_value(pc,forum_name,"Name")) != NULL) cf_set_variable(tpl,cs,"Name",v->values[0],strlen(v->values[0]),1);
      if((v = cfg_get_first_value(pc,forum_name,"EMail")) != NULL) cf_set_variable(tpl,cs,"EMail",v->values[0],strlen(v->values[0]),1);
      if((v = cfg_get_first_value(pc,forum_name,"HomepageUrl")) != NULL) cf_set_variable(tpl,cs,"HomageUrl",v->values[0],strlen(v->values[0]),1);
      if((v = cfg_get_first_value(pc,forum_name,"ImageUrl")) != NULL) cf_set_variable(tpl,cs,"ImageUrl",v->values[0],strlen(v->values[0]),1);

      str_init(&body);
      if(p) {
        memset(&thr,0,sizeof(thr));
        thr.messages = thr.last = thr.threadmsg = thr.newest = p;

        str_init(&tmp);
        msg_to_html(&thr,p->content.content,&tmp,&body,qc->values[0],-1,1);
        str_cleanup(&tmp);
      }
      else {
        if(flt_posting_cfg.Hi) flt_posting_replace_placeholders(flt_posting_cfg.Hi,&body,NULL,cs);
        if(flt_posting_cfg.Bye) flt_posting_replace_placeholders(flt_posting_cfg.Bye,&body,NULL,cs);
        if(flt_posting_cfg.Signature) {
          str_chars_append(&body,"\n-- \n",5);
          flt_posting_replace_placeholders(flt_posting_cfg.Signature,&body,NULL,cs);
        }
      }

      if(body.len) {
        cf_set_variable(tpl,cs,"body",body.content,body.len,0);
        str_cleanup(&body);
      }

      return FLT_OK;
    }
  }
  else {
    if((v = cfg_get_first_value(pc,forum_name,"Name")) != NULL) cf_set_variable(tpl,cs,"Name",v->values[0],strlen(v->values[0]),1);
    if((v = cfg_get_first_value(pc,forum_name,"EMail")) != NULL) cf_set_variable(tpl,cs,"EMail",v->values[0],strlen(v->values[0]),1);
    if((v = cfg_get_first_value(pc,forum_name,"HomepageUrl")) != NULL) cf_set_variable(tpl,cs,"HomageUrl",v->values[0],strlen(v->values[0]),1);
    if((v = cfg_get_first_value(pc,forum_name,"ImageUrl")) != NULL) cf_set_variable(tpl,cs,"ImageUrl",v->values[0],strlen(v->values[0]),1);

    str_init(&body);
    if(flt_posting_cfg.Hi) flt_posting_replace_placeholders(flt_posting_cfg.Hi,&body,NULL,cs);
    if(flt_posting_cfg.Bye) flt_posting_replace_placeholders(flt_posting_cfg.Bye,&body,NULL,cs);
    if(flt_posting_cfg.Signature) {
      str_chars_append(&body,"\n-- \n",5);
      flt_posting_replace_placeholders(flt_posting_cfg.Signature,&body,NULL,cs);
    }

    if(body.len) {
      cf_set_variable(tpl,cs,"body",body.content,body.len,0);
      str_cleanup(&body);
    }

    return FLT_OK;
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ pre and post content filters */
int flt_posting_post_cnt(t_configuration *dc,t_configuration *vc,t_cl_thread *thr,t_string *content,t_string *cite,const u_char *qchars) {
  u_char *forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  t_name_value *cs;
  u_char *tmp;

  if(cite) {
    cs = cfg_get_first_value(dc,forum_name,"ExternCharset");

    if(flt_posting_cfg.Bye) {
      if(cf_strcasecmp(cs->values[0],"utf-8") == 0 || (tmp = htmlentities_charset_convert(flt_posting_cfg.Bye,"UTF-8",cs->values[0],NULL,0)) == NULL) tmp = strdup(flt_posting_cfg.Bye);
      str_char_append(cite,'\n');
      flt_posting_replace_placeholders(tmp,cite,thr,cs);

      free(tmp);
    }
    if(flt_posting_cfg.Signature) {
      if(cf_strcasecmp(cs->values[0],"utf-8") == 0 || (tmp = htmlentities_charset_convert(flt_posting_cfg.Signature,"UTF-8",cs->values[0],NULL,0)) == NULL) tmp = strdup(flt_posting_cfg.Signature);
      str_chars_append(cite,"\n-- \n",5);
      flt_posting_replace_placeholders(tmp,cite,thr,cs);

      free(tmp);
    }

    return FLT_OK;
  }

  return FLT_DECLINE;
}

int flt_posting_pre_cnt(t_configuration *dc,t_configuration *vc,t_cl_thread *thr,t_string *content,t_string *cite,const u_char *qchars) {
  u_char *forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  t_name_value *cs;
  u_char *tmp;

  if(cite) {
    if(flt_posting_cfg.Hi) {
      cs = cfg_get_first_value(dc,forum_name,"ExternCharset");

      if(cf_strcasecmp(cs->values[0],"utf-8") == 0 || (tmp = htmlentities_charset_convert(flt_posting_cfg.Hi,"UTF-8",cs->values[0],NULL,0)) == NULL) tmp = strdup(flt_posting_cfg.Hi);
      flt_posting_replace_placeholders(tmp,cite,thr,cs);
      free(tmp);

      return FLT_OK;
    }
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_posting_handle_greet */
int flt_posting_handle_greet(t_configfile *cfile,t_conf_opt *opt,const u_char *context,u_char **args,size_t argnum) {
  u_char *tmp;

  if(flt_posting_fn == NULL) flt_posting_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(flt_posting_fn,context) != 0) return 0;

  tmp = strdup(args[0]);

  if(cf_strcmp(opt->name,"Hi") == 0) {
    if(flt_posting_cfg.Hi) free(flt_posting_cfg.Hi);
    flt_posting_cfg.Hi          = tmp;
  }
  else if(cf_strcmp(opt->name,"Bye") == 0) {
    if(flt_posting_cfg.Bye) free(flt_posting_cfg.Bye);
    flt_posting_cfg.Bye         = tmp;
  }
  else if(cf_strcmp(opt->name,"Signature") == 0) {
    if(flt_posting_cfg.Signature) free(flt_posting_cfg.Signature);
    flt_posting_cfg.Signature   = tmp;
  }

  return 0;
}
/* }}} */

/* {{{ flt_posting_handle_box */
int flt_posting_handle_box(t_configfile *cfile,t_conf_opt *opt,const u_char *context,u_char **args,size_t argnum) {
  if(flt_posting_fn == NULL) flt_posting_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(flt_posting_fn,context) != 0) return 0;

  if(flt_posting_cfg.TWidth) free(flt_posting_cfg.TWidth);
  if(flt_posting_cfg.THeight) free(flt_posting_cfg.THeight);

  flt_posting_cfg.TWidth  = strdup(args[0]);
  flt_posting_cfg.THeight = strdup(args[1]);

  return 0;
}
/* }}} */

/* {{{ flt_posting_handle_actpcol */
int flt_posting_handle_actpcol(t_configfile *cfile,t_conf_opt *opt,const u_char *context,u_char **args,size_t argnum) {
  if(flt_posting_fn == NULL) flt_posting_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(flt_posting_fn,context) != 0) return 0;

  if(flt_posting_cfg.ActiveColorF) free(flt_posting_cfg.ActiveColorF);
  if(flt_posting_cfg.ActiveColorB) free(flt_posting_cfg.ActiveColorB);

  flt_posting_cfg.ActiveColorF = strdup(args[0]);
  flt_posting_cfg.ActiveColorB = strdup(args[1]);

  return 0;
}
/* }}} */

/* {{{ flt_posting_cleanup */
void flt_posting_cleanup(void) {
  if(flt_posting_cfg.Hi)           free(flt_posting_cfg.Hi);
  if(flt_posting_cfg.Bye)          free(flt_posting_cfg.Bye);
  if(flt_posting_cfg.Signature)    free(flt_posting_cfg.Signature);
  if(flt_posting_cfg.ActiveColorF) free(flt_posting_cfg.ActiveColorF);
  if(flt_posting_cfg.ActiveColorB) free(flt_posting_cfg.ActiveColorB);
}
/* }}} */

t_conf_opt flt_posting_config[] = {
  { "Hi",                 flt_posting_handle_greet,    CFG_OPT_CONFIG|CFG_OPT_USER|CFG_OPT_LOCAL, NULL },
  { "Bye",                flt_posting_handle_greet,    CFG_OPT_CONFIG|CFG_OPT_USER|CFG_OPT_LOCAL, NULL },
  { "Signature",          flt_posting_handle_greet,    CFG_OPT_CONFIG|CFG_OPT_USER|CFG_OPT_LOCAL, NULL },
  { "TextBox",            flt_posting_handle_box,      CFG_OPT_CONFIG|CFG_OPT_USER|CFG_OPT_LOCAL, NULL },
  { "ActivePostingColor", flt_posting_handle_actpcol,  CFG_OPT_CONFIG|CFG_OPT_USER|CFG_OPT_LOCAL, NULL },
  { NULL, NULL, 0, NULL }
};

t_handler_config flt_posting_handlers[] = {
  { POSTING_HANDLER,      flt_posting_execute_filter },
  { PRE_CONTENT_FILTER,   flt_posting_pre_cnt },
  { POST_CONTENT_FILTER,  flt_posting_post_cnt },
  { POST_DISPLAY_HANDLER, flt_posting_post_display },
  { 0, NULL }
};

t_module_config flt_posting = {
  flt_posting_config,
  flt_posting_handlers,
  NULL,
  NULL,
  NULL,
  flt_posting_cleanup
};

/* eof */
