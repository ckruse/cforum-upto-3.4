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

/* {{{ struct Cfg */
struct {
  u_char *Hi;
  u_char *Bye;
  u_char *Signature;
  u_char *TWidth;
  u_char *THeight;
  u_char *ActiveColorF;
  u_char *ActiveColorB;
  int Preview;
  int PreviewSwitchType;
} flt_posting_cfg = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0 };
/* }}} */

/* {{{ replace_placeholders */
void replace_placeholders(const u_char *str,t_string *appender,t_cl_thread *thread) {
  register u_char *ptr = (u_char *)str;
  register u_char *ptr1 = NULL;
  u_char *name,*tmp;
  size_t len;
  t_name_value *cs = cfg_get_first_value(&fo_default_conf,"ExternCharset");

  if(thread) {
    for(ptr1 = thread->threadmsg->author;*ptr1;ptr1++) {
      if(isspace(*ptr1)) break;
    }
  }

  for(;*ptr;ptr++) {
    if(cf_strncmp(ptr,"\\n",2) == 0) {
      str_chars_append(appender,"\n",1);
      ptr += 1;
    }
    else if(cf_strncmp(ptr,"{$name}",7) == 0) {
      if(thread) name = htmlentities_charset_convert(thread->threadmsg->author,"UTF-8",cs->values[0],&len,0);
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
        tmp = strndup(thread->threadmsg->author,ptr1-thread->threadmsg->author);
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
    else {
      str_chars_append(appender,ptr,1);
    }
  }
}
/* }}} */

/* {{{ flt_posting_execute_filter */
int flt_posting_execute_filter(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_cl_thread *thread,t_cf_template *tpl) {
  /* {{{ variables */
  t_name_value *ps,
               *cs = cfg_get_first_value(dc,"ExternCharset"),
               *rm = cfg_get_first_value(vc,"ReadMode"),
               *dq = cfg_get_first_value(vc,"DoQuote"),
               *st = cfg_get_first_value(vc,"ShowThread"),
               *qc = cfg_get_first_value(vc,"QuotingChars"),
               *ms = cfg_get_first_value(vc,"MaxSigLines"),
               *ss = cfg_get_first_value(vc,"ShowSig");

  u_char buff[256],
        *tmp,
        *qchars,
        *msgcnt,
        *date,
        *link,
        *UserName = cf_hash_get(GlobalValues,"UserName",8);

  size_t len,
         qclen,
         msgcntlen;

  t_string cite,content;

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
    qchars = strdup(qc->values[0]);
    qclen  = strlen(qchars);
  }

  if(UserName) ps = cfg_get_first_value(dc,"UPostScript");
  else         ps = cfg_get_first_value(dc,"PostScript");

  /* {{{ set some standard variables in thread mode */
  if(flt_posting_cfg.TWidth) tpl_cf_setvar(tpl,"twidth",flt_posting_cfg.TWidth,strlen(flt_posting_cfg.TWidth),1);
  if(flt_posting_cfg.THeight) tpl_cf_setvar(tpl,"theight",flt_posting_cfg.THeight,strlen(flt_posting_cfg.THeight),1);
  if(flt_posting_cfg.Preview) tpl_cf_setvar(tpl,"preview","1",1,0);

  if(flt_posting_cfg.PreviewSwitchType == 0) tpl_cf_setvar(tpl,"previewswitchtype","checkbox",8,0);
  else if(flt_posting_cfg.PreviewSwitchType == 1) tpl_cf_setvar(tpl,"previewswitchtype","button",6,0);

  if(flt_posting_cfg.ActiveColorF || flt_posting_cfg.ActiveColorB) {
    tpl_cf_setvar(tpl,"activecolor","1",1,0);

    if(flt_posting_cfg.ActiveColorF && *flt_posting_cfg.ActiveColorF) cf_set_variable(tpl,cs,"activecolorf",flt_posting_cfg.ActiveColorF,strlen(flt_posting_cfg.ActiveColorF),1);
    if(flt_posting_cfg.ActiveColorB && *flt_posting_cfg.ActiveColorB) cf_set_variable(tpl,cs,"activecolorb",flt_posting_cfg.ActiveColorB,strlen(flt_posting_cfg.ActiveColorB),1);
  }

  cf_set_variable(tpl,cs,"action",ps->values[0],strlen(ps->values[0]),1);

  tpl_cf_setvar(tpl,"qchar","&#255;",6,0);
  tpl_cf_appendvar(tpl,"qchar",qchars,qclen);

  len = sprintf(buff,"%llu,%llu",thread->tid,thread->threadmsg->mid);
  tpl_cf_setvar(tpl,"fupto",buff,len,0);

  len = gen_unid(buff,50);
  tpl_cf_setvar(tpl,"unid",buff,len,1);
  /* }}} */

  /* {{{ set title, name, email, homepage, time and category */
  cf_set_variable(tpl,cs,"title",thread->threadmsg->subject,thread->threadmsg->subject_len,1);
  cf_set_variable(tpl,cs,"name",thread->threadmsg->author,thread->threadmsg->author_len,1);
  if(thread->threadmsg->email) cf_set_variable(tpl,cs,"email",thread->threadmsg->email,thread->threadmsg->email_len,1);
  if(thread->threadmsg->hp) cf_set_variable(tpl,cs,"link",thread->threadmsg->hp,thread->threadmsg->hp_len,1);
  if(thread->threadmsg->img) cf_set_variable(tpl,cs,"image",thread->threadmsg->img,thread->threadmsg->img_len,1);
  if(thread->threadmsg->category) cf_set_variable(tpl,cs,"category",thread->threadmsg->category,thread->threadmsg->category_len,1);

  tmp  = get_time(vc,"DateFormatThreadView",&len,&thread->threadmsg->date);
  cf_set_variable(tpl,cs,"time",tmp,len,1);
  free(tmp);
  /* }}} */

  /* {{{ has this posting parent postings?
   * If yes, set some variables
   */
  if(thread->threadmsg != thread->messages) {
    t_message *msg;
    if((msg = parent_message(thread->threadmsg)) != NULL) {
      tmp = get_time(vc,"DateFormatThreadView",&len,&msg->date);

      tpl_cf_setvar(tpl,"messagebefore","1",1,0);
      cf_set_variable(tpl,cs,"b_name",msg->author,strlen(msg->author),1);
      cf_set_variable(tpl,cs,"b_title",msg->subject,strlen(msg->subject),1);
      cf_set_variable(tpl,cs,"b_time",tmp,len,1);

      free(tmp);

      tmp = get_link(NULL,thread->tid,msg->mid);
      cf_set_variable(tpl,cs,"b_link",tmp,strlen(tmp),1);

      free(tmp);

      if(msg->category) {
        cf_set_variable(tpl,cs,"b_category",msg->category,strlen(msg->category),1);
      }
    }
  }
  /* }}} */

  /* {{{ generate html code for the message and the cite */
  /* ok -- lets convert the message to the target charset with html encoded */
  if(utf8 || (msgcnt = charset_convert_entities(thread->threadmsg->content,thread->threadmsg->content_len,"UTF-8",cs->values[0],&msgcntlen)) == NULL) {
    msgcnt    = strdup(thread->threadmsg->content);
    msgcntlen = thread->threadmsg->content_len;
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

  tpl_cf_setvar(tpl,"message",content.content,content.len,0);
  if(cf_strcmp(dq->values[0],"yes") == 0) tpl_cf_setvar(tpl,"cite",cite.content,cite.len,0);
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
    else tpl_cf_setvar(&thread->threadmsg->tpl,"active","1",1,0);

    handle_thread(thread,head,1);

    tpl_cf_setvar(tpl,"threadlist","",0,0);
    for(msg=thread->messages;msg;msg=msg->next) {
      if((msg->may_show && msg->invisible == 0) || ShowInvisible == 1) {
        rc = handle_thread_list_posting(msg,head,thread->tid,1);

        if(ShowInvisible == 0 && (rc == FLT_EXIT || msg->may_show == 0)) continue;
        else if(slvl == -1) slvl = msg->level;

        date = get_time(vc,"DateFormatThreadList",&len,&msg->date);
        link = get_link(NULL,thread->tid,msg->mid);

        cf_set_variable(&msg->tpl,cs,"author",msg->author,strlen(msg->author),1);
        cf_set_variable(&msg->tpl,cs,"title",msg->subject,strlen(msg->subject),1);

        if(msg->category) cf_set_variable(&msg->tpl,cs,"category",msg->category,strlen(msg->category),1);

        if(date) {
          cf_set_variable(&msg->tpl,cs,"time",date,len,1);
          free(date);
        }

        if(link) {
          cf_set_variable(&msg->tpl,cs,"link",link,strlen(link),1);
          free(link);
        }

        if(msg->level < level) {
          for(;level>msg->level;level--) tpl_cf_appendvar(tpl,"threadlist","</ul></li>",10);
        }

        if(msg->next && has_answers(msg)) { /* this message has at least one answer */
          tpl_cf_appendvar(tpl,"threadlist","<li>",4);

          tpl_cf_parse_to_mem(&msg->tpl);
          tpl_cf_appendvar(tpl,"threadlist",msg->tpl.parsed.content,msg->tpl.parsed.len-1);

          tpl_cf_appendvar(tpl,"threadlist","<ul>",4);

          level++;
        }
        else {
          tpl_cf_appendvar(tpl,"threadlist","<li>",4);

          tpl_cf_parse_to_mem(&msg->tpl);
          tpl_cf_appendvar(tpl,"threadlist",msg->tpl.parsed.content,msg->tpl.parsed.len-1);

          tpl_cf_appendvar(tpl,"threadlist","</li>",5);
        }
      }
    }

    for(;level>slvl;level--) tpl_cf_appendvar(tpl,"threadlist","</ul></li>",10);
  }
  /* }}} */

  return FLT_OK;
}
/* }}} */

/* {{{ pre and post content filters */
int flt_posting_post_cnt(t_configuration *dc,t_configuration *vc,t_cl_thread *thr,t_string *content,t_string *cite,const u_char *qchars) {
  t_name_value *cs;
  u_char *tmp;

  if(cite) {
    cs = cfg_get_first_value(dc,"ExternCharset");

    if(flt_posting_cfg.Bye) {
      if(cf_strcasecmp(cs->values[0],"utf-8") == 0 || (tmp = htmlentities_charset_convert(flt_posting_cfg.Bye,"UTF-8",cs->values[0],NULL,0)) == NULL) tmp = strdup(flt_posting_cfg.Bye);
      str_char_append(cite,'\n');
      replace_placeholders(tmp,cite,thr);

      free(tmp);
    }
    if(flt_posting_cfg.Signature) {
      if(cf_strcasecmp(cs->values[0],"utf-8") == 0 || (tmp = htmlentities_charset_convert(flt_posting_cfg.Signature,"UTF-8",cs->values[0],NULL,0)) == NULL) tmp = strdup(flt_posting_cfg.Signature);
      str_chars_append(cite,"\n-- \n",5);
      replace_placeholders(tmp,cite,thr);

      free(tmp);
    }

    return FLT_OK;
  }

  return FLT_DECLINE;
}

int flt_posting_pre_cnt(t_configuration *dc,t_configuration *vc,t_cl_thread *thr,t_string *content,t_string *cite,const u_char *qchars) {
  t_name_value *cs;
  u_char *tmp;

  if(cite) {
    if(flt_posting_cfg.Hi) {
      cs = cfg_get_first_value(dc,"ExternCharset");

      if(cf_strcasecmp(cs->values[0],"utf-8") == 0 || (tmp = htmlentities_charset_convert(flt_posting_cfg.Hi,"UTF-8",cs->values[0],NULL,0)) == NULL) tmp = strdup(flt_posting_cfg.Hi);
      replace_placeholders(tmp,cite,thr);
      free(tmp);

      return FLT_OK;
    }
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ module configuration */
/* {{{ handle_greet */
int handle_greet(t_configfile *cfile,t_conf_opt *opt,u_char **args,int argnum) {
  u_char *tmp = strdup(args[0]);

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

/* {{{ handle_box */
int handle_box(t_configfile *cfile,t_conf_opt *opt,u_char **args,int argnum) {
  if(flt_posting_cfg.TWidth) free(flt_posting_cfg.TWidth);
  if(flt_posting_cfg.THeight) free(flt_posting_cfg.THeight);

  flt_posting_cfg.TWidth  = strdup(args[0]);
  flt_posting_cfg.THeight = strdup(args[1]);

  return 0;
}
/* }}} */

/* {{{ handle_prev */
int handle_prev(t_configfile *cfile,t_conf_opt *opt,u_char **args,int argnum) {
  flt_posting_cfg.Preview = cf_strcmp(args[0],"yes") == 0;

  return 0;
}
/* }}} */

/* {{{ handle_prevt */
int handle_prevt(t_configfile *cfile,t_conf_opt *opt,u_char **args,int argnum) {
  if(cf_strcmp(args[0],"button") == 0) {
    flt_posting_cfg.PreviewSwitchType = 1;
  }
  else if(cf_strcmp(args[0],"checkbox") == 0) {
    flt_posting_cfg.PreviewSwitchType = 0;
  }
  else {
    fprintf(stderr,"Error: wrong value for PreviewSwitchType\n");
    return 1;
  }

  return 0;
}
/* }}} */

/* {{{ handle_actpcol */
int handle_actpcol(t_configfile *cfile,t_conf_opt *opt,u_char **args,int argnum) {
  if(flt_posting_cfg.ActiveColorF) free(flt_posting_cfg.ActiveColorF);
  if(flt_posting_cfg.ActiveColorB) free(flt_posting_cfg.ActiveColorB);

  flt_posting_cfg.ActiveColorF = strdup(args[0]);
  flt_posting_cfg.ActiveColorB = strdup(args[1]);

  return 0;
}
/* }}} */

/* {{{ cleanup */
void flt_posting_cleanup(void) {
  if(flt_posting_cfg.Hi)           free(flt_posting_cfg.Hi);
  if(flt_posting_cfg.Bye)          free(flt_posting_cfg.Bye);
  if(flt_posting_cfg.Signature)    free(flt_posting_cfg.Signature);
  if(flt_posting_cfg.ActiveColorF) free(flt_posting_cfg.ActiveColorF);
  if(flt_posting_cfg.ActiveColorB) free(flt_posting_cfg.ActiveColorB);
}
/* }}} */

/* {{{ t_conf_opt config[] */
t_conf_opt flt_posting_config[] = {
  { "Hi",                         handle_greet,    CFG_OPT_CONFIG|CFG_OPT_USER,                NULL },
  { "Bye",                        handle_greet,    CFG_OPT_CONFIG|CFG_OPT_USER,                NULL },
  { "Signature",                  handle_greet,    CFG_OPT_CONFIG|CFG_OPT_USER,                NULL },
  { "TextBox",                    handle_box,      CFG_OPT_CONFIG|CFG_OPT_USER,                NULL },
  { "GeneratePreview",            handle_prev,     CFG_OPT_CONFIG|CFG_OPT_USER,                NULL },
  { "PreviewSwitchType",          handle_prevt,    CFG_OPT_CONFIG|CFG_OPT_USER,                NULL },
  { "ActivePostingColor",         handle_actpcol,  CFG_OPT_CONFIG|CFG_OPT_USER,                NULL },
  { NULL, NULL, 0, NULL }
};
/* }}} */

/* {{{ t_handler_config handlers[] */
t_handler_config flt_posting_handlers[] = {
  { POSTING_HANDLER,     flt_posting_execute_filter },
  { PRE_CONTENT_FILTER,  flt_posting_pre_cnt        },
  { POST_CONTENT_FILTER, flt_posting_post_cnt       },
  { 0, NULL }
};
/* }}} */

/* {{{ t_module_config flt_posting */
t_module_config flt_posting = {
  flt_posting_config,
  flt_posting_handlers,
  NULL,
  NULL,
  NULL,
  flt_posting_cleanup
};
/* }}} */
/* }}} */

/* eof */
