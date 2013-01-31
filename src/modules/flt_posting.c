/**
 * \file flt_posting.c
 * \author Christian Kruse, <cjk@wwwtech.de>
 *
 * This plugin handles a posting read request in thread mode
 */

/* {{{ Initial comments */
/*
 * $LastChangedDate: 2009-01-16 14:32:24 +0100 (Fri, 16 Jan 2009) $
 * $LastChangedRevision: 1639 $
 * $LastChangedBy: ckruse $
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
#include <inttypes.h>

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


static u_char *flt_posting_tpl = NULL;
static u_char *flt_posting_fn = NULL;

/* {{{ flt_posting_replace_placeholders */
void flt_posting_replace_placeholders(const u_char *str,string_t *appender,cl_thread_t *thread,name_value_t *cs) {
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
int flt_posting_execute_filter(cf_hash_t *head,configuration_t *dc,configuration_t *vc,cl_thread_t *thread,cf_template_t *tpl) {
  u_char buff[256],*tmp,*qchars,*msgcnt,*UserName,*forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  name_value_t *ps,*v,*cs = cfg_get_first_value(dc,forum_name,"ExternCharset"),*dq,*st,*qc,*ms,*ss,*locale,*df,*rm = cfg_get_first_value(vc,forum_name,"ReadMode");
  int utf8;
  size_t len,qclen,msgcntlen;
  string_t cite,content,threadlist;
  cf_tpl_variable_t hash;
  cf_readmode_t *rm_infos = cf_hash_get(GlobalValues,"RM",2);

  /* {{{ standard variables, set always, mode doesn't matter */
  if(flt_posting_cfg.TWidth) cf_tpl_setvalue(tpl,"twidth",TPL_VARIABLE_STRING,flt_posting_cfg.TWidth,strlen(flt_posting_cfg.TWidth));
  if(flt_posting_cfg.THeight) cf_tpl_setvalue(tpl,"theight",TPL_VARIABLE_STRING,flt_posting_cfg.THeight,strlen(flt_posting_cfg.THeight));

  if(flt_posting_cfg.ActiveColorF || flt_posting_cfg.ActiveColorB) {
    cf_tpl_setvalue(tpl,"activecolor",TPL_VARIABLE_STRING,"1",1);

    if(flt_posting_cfg.ActiveColorF && *flt_posting_cfg.ActiveColorF) cf_set_variable(tpl,cs,"activecolorf",flt_posting_cfg.ActiveColorF,strlen(flt_posting_cfg.ActiveColorF),1);
    if(flt_posting_cfg.ActiveColorB && *flt_posting_cfg.ActiveColorB) cf_set_variable(tpl,cs,"activecolorb",flt_posting_cfg.ActiveColorB,strlen(flt_posting_cfg.ActiveColorB),1);
  }
  /* }}} */

  /* are we in the right read mode? */
  if(cf_strcmp(rm->values[0],"thread")) return FLT_DECLINE;

  UserName = cf_hash_get(GlobalValues,"UserName",8);

  dq = cfg_get_first_value(vc,forum_name,"DoQuote");
  st = cfg_get_first_value(vc,forum_name,"ShowThread");
  qc = cfg_get_first_value(vc,forum_name,"QuotingChars");
  ms = cfg_get_first_value(vc,forum_name,"MaxSigLines");
  ss = cfg_get_first_value(vc,forum_name,"ShowSig");
  locale = cfg_get_first_value(dc,forum_name,"DateLocale");
  df = cfg_get_first_value(vc,forum_name,"DateFormatThreadView");

  utf8 = cf_strcmp(cs->values[0],"UTF-8") == 0;

  if(utf8 || (qchars = htmlentities_charset_convert(qc->values[0],"UTF-8",cs->values[0],&qclen,0)) == NULL) {
    qchars = htmlentities(qc->values[0],0);
    qclen  = strlen(qchars);
  }

  if(UserName) ps = cfg_get_first_value(dc,forum_name,"UPostScript");
  else         ps = cfg_get_first_value(dc,forum_name,"PostScript");

  cf_tpl_var_init(&hash,TPL_VARIABLE_HASH);

  /* {{{ set some standard variables in thread mode */
  cf_set_variable(tpl,cs,"action",ps->values[0],strlen(ps->values[0]),1);

  cf_tpl_setvalue(tpl,"qchar",TPL_VARIABLE_STRING,"&#255;",6);
  cf_tpl_appendvalue(tpl,"qchar",qchars,qclen);

  len = sprintf(buff,"%"PRIu64",%"PRIu64,thread->tid,thread->threadmsg->mid);
  cf_tpl_setvalue(tpl,"fupto",TPL_VARIABLE_STRING,buff,len);

  len = gen_unid(buff,50);
  cf_tpl_setvalue(tpl,"unid",TPL_VARIABLE_STRING,buff,len);

  len = sprintf(buff,"%"PRIu64,thread->tid);
  cf_tpl_setvalue(tpl,"tid",TPL_VARIABLE_STRING,buff,len);

  len = sprintf(buff,"%"PRIu64,thread->threadmsg->mid);
  cf_tpl_setvalue(tpl,"mid",TPL_VARIABLE_STRING,buff,len);
  /* }}} */

  if((v = cfg_get_first_value(vc,forum_name,"Name")) != NULL) cf_set_variable(tpl,cs,"aname",v->values[0],strlen(v->values[0]),1);
  if((v = cfg_get_first_value(vc,forum_name,"EMail")) != NULL) cf_set_variable(tpl,cs,"aemail",v->values[0],strlen(v->values[0]),1);
  if((v = cfg_get_first_value(vc,forum_name,"HomepageUrl")) != NULL) cf_set_variable(tpl,cs,"aurl",v->values[0],strlen(v->values[0]),1);
  if((v = cfg_get_first_value(vc,forum_name,"ImageUrl")) != NULL) cf_set_variable(tpl,cs,"aimg",v->values[0],strlen(v->values[0]),1);

  /* {{{ set title, name, email, homepage, time and category */
  cf_set_variable(tpl,cs,"name",thread->threadmsg->author.content,thread->threadmsg->author.len,1);
  cf_set_variable(tpl,cs,"title",thread->threadmsg->subject.content,thread->threadmsg->subject.len,1);
  if(thread->threadmsg->email.len) cf_set_variable(tpl,cs,"email",thread->threadmsg->email.content,thread->threadmsg->email.len,1);
  if(thread->threadmsg->category.len) cf_set_variable(tpl,cs,"category",thread->threadmsg->category.content,thread->threadmsg->category.len,1);

  cf_set_variable_hash(&hash,cs,"title",thread->threadmsg->subject.content,thread->threadmsg->subject.len,1);
  if(thread->threadmsg->email.len) cf_set_variable_hash(&hash,cs,"email",thread->threadmsg->email.content,thread->threadmsg->email.len,1);
  cf_set_variable_hash(&hash,cs,"name",thread->threadmsg->author.content,thread->threadmsg->author.len,1);
  if(thread->threadmsg->hp.len) cf_set_variable_hash(&hash,cs,"link",thread->threadmsg->hp.content,thread->threadmsg->hp.len,1);
  if(thread->threadmsg->img.len) cf_set_variable_hash(&hash,cs,"image",thread->threadmsg->img.content,thread->threadmsg->img.len,1);
  if(thread->threadmsg->category.len) cf_set_variable_hash(&hash,cs,"category",thread->threadmsg->category.content,thread->threadmsg->category.len,1);

  tmp = cf_general_get_time(df->values[0],locale->values[0],&len,&thread->threadmsg->date);
  cf_set_variable_hash(&hash,cs,"time",tmp,len,1);
  free(tmp);
  /* }}} */

  /* {{{ has this posting parent postings?
   * If yes, set some variables
   */
  if(thread->threadmsg != thread->messages) {
    message_t *msg;
    if((msg = cf_msg_get_parent(thread->threadmsg)) != NULL) {
      tmp = cf_general_get_time(df->values[0],locale->values[0],&len,&msg->date);

      cf_tpl_hashvar_setvalue(&hash,"messagebefore",TPL_VARIABLE_STRING,"1",1);
      cf_set_variable_hash(&hash,cs,"b_name",msg->author.content,msg->author.len,1);
      cf_set_variable_hash(&hash,cs,"b_title",msg->subject.content,msg->subject.len,1);
      cf_set_variable_hash(&hash,cs,"b_time",tmp,len,1);

      free(tmp);

      tmp = cf_get_link(NULL,thread->tid,msg->mid);
      cf_set_variable_hash(&hash,cs,"b_link",tmp,strlen(tmp),1);
      free(tmp);

      if(msg->category.len) cf_set_variable_hash(&hash,cs,"b_category",msg->category.content,msg->category.len,1);
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

  cf_tpl_hashvar_setvalue(&hash,"message",TPL_VARIABLE_STRING,content.content,content.len);
  if(cf_strcmp(dq->values[0],"yes") == 0) cf_tpl_hashvar_setvalue(&hash,"cite",TPL_VARIABLE_STRING,cite.content,cite.len);
  /* }}} */

  str_cleanup(&cite);
  str_cleanup(&content);
  free(msgcnt);
  free(qchars);

  if(cf_strcmp(st->values[0],"none") != 0) {
    if(cf_gen_threadlist(thread,head,&threadlist,rm_infos->thread_posting_tpl,st->values[0],rm_infos->posting_uri[UserName?1:0],CF_MODE_THREADVIEW) != FLT_EXIT) {
      //cf_gen_threadlist(thread,head,&threadlist,rm_infos->post_threadlist_tpl,st->values[0],NULL,CF_MODE_THREADVIEW);
      cf_tpl_setvalue(tpl,"threadlist",TPL_VARIABLE_STRING,threadlist.content,threadlist.len);
      str_cleanup(&threadlist);
    }
  }

  cf_run_perpost_var_handlers(head,thread,thread->threadmsg,&hash);
  cf_tpl_setvar(tpl,"thread",&hash);

  return FLT_OK;
}
/* }}} */

/* {{{ flt_posting_post_display */
int flt_posting_post_display(cf_hash_t *head,configuration_t *dc,configuration_t *pc,cf_template_t *tpl,message_t *p) {
  u_char *forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  name_value_t *v;
  name_value_t *cs = cfg_get_first_value(dc,forum_name,"ExternCharset");
  name_value_t *qc = cfg_get_first_value(pc,forum_name,"QuotingChars");
  string_t body,tmp;

  cl_thread_t thr;

  if(flt_posting_cfg.TWidth) cf_tpl_setvalue(tpl,"twidth",TPL_VARIABLE_STRING,flt_posting_cfg.TWidth,strlen(flt_posting_cfg.TWidth));
  if(flt_posting_cfg.THeight) cf_tpl_setvalue(tpl,"theight",TPL_VARIABLE_STRING,flt_posting_cfg.THeight,strlen(flt_posting_cfg.THeight));

  if(head) {
    /* set if none of the values have been given */
    if(!cf_cgi_get(head,"Name") && !cf_cgi_get(head,"EMail") && !cf_cgi_get(head,"HomepageUrl") && !cf_cgi_get(head,"ImageUrl") && !cf_cgi_get(head,"body")) {
      if((v = cfg_get_first_value(pc,forum_name,"Name")) != NULL) cf_set_variable(tpl,cs,"Name",v->values[0],strlen(v->values[0]),1);
      if((v = cfg_get_first_value(pc,forum_name,"EMail")) != NULL) cf_set_variable(tpl,cs,"EMail",v->values[0],strlen(v->values[0]),1);
      if((v = cfg_get_first_value(pc,forum_name,"HomepageUrl")) != NULL) cf_set_variable(tpl,cs,"HomepageUrl",v->values[0],strlen(v->values[0]),1);
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
    if((v = cfg_get_first_value(pc,forum_name,"HomepageUrl")) != NULL) cf_set_variable(tpl,cs,"HomepageUrl",v->values[0],strlen(v->values[0]),1);
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
int flt_posting_post_cnt(configuration_t *dc,configuration_t *vc,cl_thread_t *thr,string_t *content,string_t *cite,const u_char *qchars) {
  u_char *forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  name_value_t *cs;
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

int flt_posting_pre_cnt(configuration_t *dc,configuration_t *vc,cl_thread_t *thr,string_t *content,string_t *cite,const u_char *qchars) {
  u_char *forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  name_value_t *cs;
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

/* {{{ flt_posting_rm_collector */
int flt_posting_rm_collector(cf_hash_t *head,configuration_t *dc,configuration_t *vc,cf_readmode_t *rm_infos) {
  name_value_t *rm;
  name_value_t *v;

  u_char buff[256];

  if(flt_posting_fn == NULL) flt_posting_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  rm = cfg_get_first_value(vc,flt_posting_fn,"ReadMode");

  if(cf_strcmp(rm->values[0],"thread") == 0) {
    v = cfg_get_first_value(dc,flt_posting_fn,"PostingURL");
    rm_infos->posting_uri[0] = v->values[0];

    v = cfg_get_first_value(dc,flt_posting_fn,"UPostingURL");
    rm_infos->posting_uri[1] = v->values[0];

    if((v = cfg_get_first_value(vc,flt_posting_fn,"TemplateForumBegin")) != NULL) {
      cf_gen_tpl_name(buff,256,v->values[0]);
      rm_infos->pre_threadlist_tpl = strdup(buff);
    }

    if((v = cfg_get_first_value(vc,flt_posting_fn,"TemplateForumThread")) != NULL) {
      cf_gen_tpl_name(buff,256,v->values[0]);
      rm_infos->thread_posting_tpl = rm_infos->threadlist_thread_tpl = strdup(buff);
    }

    if((v = cfg_get_first_value(vc,flt_posting_fn,"TemplateForumEnd")) != NULL) {
      cf_gen_tpl_name(buff,256,v->values[0]);
      rm_infos->post_threadlist_tpl = strdup(buff);
    }

    if(flt_posting_tpl) {
      cf_gen_tpl_name(buff,256,flt_posting_tpl);
      rm_infos->thread_tpl = strdup(buff);
    }

    return FLT_OK;
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_posting_handle_greet */
int flt_posting_handle_greet(configfile_t *cfile,conf_opt_t *opt,const u_char *context,u_char **args,size_t argnum) {
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
int flt_posting_handle_box(configfile_t *cfile,conf_opt_t *opt,const u_char *context,u_char **args,size_t argnum) {
  if(flt_posting_fn == NULL) flt_posting_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(flt_posting_fn,context) != 0) return 0;

  if(flt_posting_cfg.TWidth) free(flt_posting_cfg.TWidth);
  if(flt_posting_cfg.THeight) free(flt_posting_cfg.THeight);

  if(strlen(args[0]))
    flt_posting_cfg.TWidth  = strdup(args[0]);
  else flt_posting_cfg.TWidth  = NULL;

  if(strlen(args[1]))
    flt_posting_cfg.THeight = strdup(args[1]);
  else flt_posting_cfg.THeight = NULL;

  return 0;
}
/* }}} */

/* {{{ flt_posting_handle_actpcol */
int flt_posting_handle_actpcol(configfile_t *cfile,conf_opt_t *opt,const u_char *context,u_char **args,size_t argnum) {
  if(flt_posting_fn == NULL) flt_posting_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(flt_posting_fn,context) != 0) return 0;

  if(flt_posting_cfg.ActiveColorF) free(flt_posting_cfg.ActiveColorF);
  if(flt_posting_cfg.ActiveColorB) free(flt_posting_cfg.ActiveColorB);

  flt_posting_cfg.ActiveColorF = strdup(args[0]);
  flt_posting_cfg.ActiveColorB = strdup(args[1]);

  return 0;
}
/* }}} */

int flt_posting_handle_tpl(configfile_t *cfile,conf_opt_t *opt,const u_char *context,u_char **args,size_t argnum) {
  if(flt_posting_fn == NULL) flt_posting_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(flt_posting_fn,context) != 0) return 0;

  if(flt_posting_tpl) free(flt_posting_tpl);
  flt_posting_tpl = strdup(args[0]);

  return 0;
}

/* {{{ flt_posting_cleanup */
void flt_posting_cleanup(void) {
  if(flt_posting_cfg.Hi)           free(flt_posting_cfg.Hi);
  if(flt_posting_cfg.Bye)          free(flt_posting_cfg.Bye);
  if(flt_posting_cfg.Signature)    free(flt_posting_cfg.Signature);
  if(flt_posting_cfg.ActiveColorF) free(flt_posting_cfg.ActiveColorF);
  if(flt_posting_cfg.ActiveColorB) free(flt_posting_cfg.ActiveColorB);
}
/* }}} */

conf_opt_t flt_posting_config[] = {
  { "Hi",                 flt_posting_handle_greet,    CFG_OPT_CONFIG|CFG_OPT_USER|CFG_OPT_LOCAL, NULL },
  { "Bye",                flt_posting_handle_greet,    CFG_OPT_CONFIG|CFG_OPT_USER|CFG_OPT_LOCAL, NULL },
  { "Signature",          flt_posting_handle_greet,    CFG_OPT_CONFIG|CFG_OPT_USER|CFG_OPT_LOCAL, NULL },
  { "TextBox",            flt_posting_handle_box,      CFG_OPT_CONFIG|CFG_OPT_USER|CFG_OPT_LOCAL, NULL },
  { "ActivePostingColor", flt_posting_handle_actpcol,  CFG_OPT_CONFIG|CFG_OPT_USER|CFG_OPT_LOCAL, NULL },
  { "TemplatePosting",    flt_posting_handle_tpl,      CFG_OPT_CONFIG|CFG_OPT_LOCAL,              NULL },
  { NULL, NULL, 0, NULL }
};

handler_config_t flt_posting_handlers[] = {
  { RM_COLLECTORS_HANDLER, flt_posting_rm_collector },
  { POSTING_HANDLER,       flt_posting_execute_filter },
  { PRE_CONTENT_FILTER,    flt_posting_pre_cnt },
  { POST_CONTENT_FILTER,   flt_posting_post_cnt },
  { POST_DISPLAY_HANDLER,  flt_posting_post_display },
  { 0, NULL }
};

module_config_t flt_posting = {
  MODULE_MAGIC_COOKIE,
  flt_posting_config,
  flt_posting_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  flt_posting_cleanup
};

/* eof */
