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
/* }}} */

/* {{{ struct Cfg */
struct {
  int ShowSig;
  int DoQuote;
  u_char *QuotingChars;
  u_char *ShowThread;
  u_char *Hi;
  u_char *Bye;
  u_char *Signature;
  u_char *link;
  u_char *TWidth;
  u_char *THeight;
  u_char *ActiveColorF;
  u_char *ActiveColorB;
  int Preview;
  int PreviewSwitchType;
  int IframeAsLink;
  int ImageAsLink;
} Cfg = { 0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 0 };
/* }}} */

/* {{{ convert */
u_char *convert(const u_char *in,size_t len,size_t *olen,const t_name_value *cs) {
  u_char *str = strndup(in,len);
  u_char *ret = htmlentities_charset_convert(str,"UTF-8",cs->values[0],olen,0);
  free(str);
  return ret;
}
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

/* {{{ get_parent_message */
t_message *get_parent_message(t_message *msgs,t_message *tmsg) {
  t_message *msg,*bmsg = NULL;

  for(msg=msgs;msg;msg=msg->next) {
    if(msg->level == tmsg->level - 1) {
      bmsg = msg;
    }

    if(msg->next && msg->next == tmsg) {
      return bmsg;
    }
  }

  return NULL;
}
/* }}} */

/* {{{ next_line_is_no_quote_line */
int next_line_is_no_quote_line(const u_char *ptr) {
  int eq;

  for(;*ptr && ((eq = cf_strncmp(ptr,"<br />",6)) == 0 || *ptr == ' ');ptr++) {
    if(eq == 0) ptr += 5;
  }

  if(*ptr == (u_char)127) return 0;
  return 1;
}
/* }}} */

/* {{{ msg_to_html */
void msg_to_html(const u_char *msg,t_string *content,t_string *cite) {
  t_name_value *cs = cfg_get_first_value(&fo_default_conf,"ExternCharset");
  const u_char *ptr,*tmp;
        u_char *qchars;
        size_t qclen;
  int linebrk = 0,quotemode = 0,sig = 0,utf8 = cf_strcmp(cs->values[0],"UTF-8") == 0;

  if(utf8 || (qchars = htmlentities_charset_convert(Cfg.QuotingChars,"UTF-8",cs->values[0],&qclen,0)) == NULL) {
    qchars = strdup(Cfg.QuotingChars);
    qclen  = strlen(qchars);
  }

  /* first line has no linebreak, so append quoting chars to cite */
  if(cite) str_chars_append(cite,qchars,qclen);

  for(ptr=msg;*ptr;ptr++) {
    if(cf_strncmp(ptr,"<br />",6) == 0) {
      linebrk = 1;

      str_chars_append(content,"<br />",6);

      if(sig == 0 && cite) {
        str_chars_append(cite,"\n",1);
        str_chars_append(cite,qchars,qclen);
      }

      if(quotemode && next_line_is_no_quote_line(ptr+6)) {
        str_chars_append(content,"</span>",7);
        quotemode = 0;
      }

      ptr += 5;
    }
    else if(cf_strncmp(ptr,"<a href=\"",9) == 0) {
      ptr    += 9;
      linebrk = 0;
      tmp     = strstr(ptr,"\"");

      if(tmp) {
        str_chars_append(content,"<a href=\"",9);
        str_chars_append(content,ptr,tmp-ptr);

        if(Cfg.link) {
          str_chars_append(content,"\" target=\"",10);
          str_chars_append(content,Cfg.link,strlen(Cfg.link));
        }

        str_chars_append(content,"\">",2);
        str_chars_append(content,ptr,tmp-ptr);
        str_chars_append(content,"</a>",4);

        if(sig == 0 && cite) {
          str_chars_append(cite,"[link:",6);
          str_chars_append(cite,ptr,tmp-ptr);
          str_chars_append(cite,"]",1);
        }

        for(;cf_strncmp(ptr+1,"</a>",4) != 0;ptr++);
        ptr += 4;
      }
      else {
        tmp -= 9;
      }
    }
    else if(cf_strncmp(ptr,"<img src=\"",10) == 0) {
      u_char *tmp;

      ptr    += 10;
      linebrk = 0;

      tmp     = strstr(ptr,"\"");

      if(tmp) {
        if(Cfg.ImageAsLink) {
          str_chars_append(content,"<a href=\"",9);

          str_chars_append(content,ptr,tmp-ptr);
          str_chars_append(content,"\">",2);
          str_chars_append(content,ptr,tmp-ptr);
          str_chars_append(content,"</a>",4);
        }
        else {
          str_chars_append(content,"<img src=\"",10);

          str_chars_append(content,ptr,tmp-ptr);
          str_chars_append(content,"\" alt=\"Externes Bild\">",22);
        }

        if(sig == 0 && cite) {
          str_chars_append(cite,"[image:",7);
          str_chars_append(cite,ptr,tmp-ptr);
          str_chars_append(cite,"]",1);
        }

        ptr = strstr(tmp,">");
      }
      else {
        ptr -= 10;
      }
    }
    else if(cf_strncmp(ptr,"[pref:",6) == 0) {
      ptr += 6;
      for(tmp=ptr;*tmp && (*tmp == 't' || *tmp == 'm' || *tmp == '=' || *tmp == '&' || isdigit(*tmp));tmp++) {
        if(cf_strncmp(tmp,"&amp;",5) == 0) {
          tmp += 5;
        }
      }

      if(sig == 0 && cite) {
        str_chars_append(cite,"[link:?",7);
        str_chars_append(cite,ptr,tmp-ptr);
        str_chars_append(cite,"]",1);
      }

      str_chars_append(content,"<a href=\"?",10);
      str_chars_append(content,ptr,tmp-ptr);

      if(Cfg.link) {
        str_chars_append(content,"\" target=\"",10);
        str_chars_append(content,Cfg.link,strlen(Cfg.link));
      }

      str_chars_append(content,"\">?",3);
      str_chars_append(content,ptr,tmp-ptr);
      str_chars_append(content,"</a>",4);

      ptr = tmp;
    }
    else if(cf_strncmp(ptr,"<iframe",7) == 0) {
      ptr += 13;
      tmp = ptr;
      ptr = strstr(ptr,"\"");

      /* ok, user has the choice: show [iframe:] as a link or show [iframe:] as iframe? */
      if(Cfg.IframeAsLink) {
        str_chars_append(content,"<a href=\"",9);
        str_chars_append(content,tmp,ptr-tmp);
        str_chars_append(content,"\">",2);
        str_chars_append(content,tmp,ptr-tmp);
        str_chars_append(content,"</a>",4);
      }
      else {
        str_chars_append(content,"<iframe src=\"",13);
        str_chars_append(content,tmp,ptr-tmp);
        str_chars_append(content,"\" width=\"90%\" height=\"90%\"><a href=\"",36);
        str_chars_append(content,tmp,ptr-tmp);
        str_chars_append(content,"\">",2);
        str_chars_append(content,tmp,ptr-tmp);
        str_chars_append(content,"</a></iframe>",13);
      }

      if(sig == 0 && cite) {
        str_chars_append(cite,"[iframe:",8);
        str_chars_append(cite,tmp,ptr-tmp);
        str_char_append(cite,']');
      }

      ptr = strstr(ptr,"</iframe>");
      ptr += 8;
    }
    else if(*ptr == (u_char)127) {
      linebrk = 0;

      if(!quotemode) {
        str_chars_append(content,"<span class=\"q\">",16);
      }

      str_chars_append(content,qchars,qclen);
      quotemode = 1;

      if(sig == 0 && cite) {
        str_chars_append(cite,qchars,qclen);
      }
    }
    else if(cf_strncmp(ptr,"_/_SIG_/_",9) == 0) {
      if(quotemode) {
        str_chars_append(content,"</span>",7);
        quotemode = 0;
      }
    
      /* some users don't like sigs */
      if(!Cfg.ShowSig) break;

      sig = 1;

      str_chars_append(content,"<br /><span class=\"sig\">",24);
      str_chars_append(content,"-- <br />",9);

      ptr += 8;
    }
    else {
      str_chars_append(content,ptr,1);

      if(sig == 0 && cite) {
        str_chars_append(cite,ptr,1);
      }
    }
  }

  if(quotemode || sig) {
    str_chars_append(content,"</span>",7);
  }

  free(qchars);

}
/* }}} */

/* {{{ execute_filter */
int execute_filter(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_cl_thread *thread,t_cf_template *tpl) {
  t_string content,cite;
  u_char *ptr;
  t_name_value *ps        = NULL;
  t_name_value *name      = cfg_get_first_value(vc,"Name");
  t_name_value *email     = cfg_get_first_value(vc,"EMail");
  t_name_value *hpurl     = cfg_get_first_value(vc,"HomepageUrl");
  t_name_value *imgurl    = cfg_get_first_value(vc,"ImageUrl");
  t_name_value *fbase     = NULL;
  t_name_value *cs        = cfg_get_first_value(dc,"ExternCharset");
  t_message *msg;
  u_char *UserName = cf_hash_get(GlobalValues,"UserName",8);
  int ShowInvisible = cf_hash_get(GlobalValues,"ShowInvisible",13) == NULL ? 0 : 1;

  int utf8;
  int   len       = 0;
  u_char *tmp     = NULL;
  u_char  buff[50];
  u_char *date,*link,*qchars,*msgcnt = NULL;
  int rc = 0;
  size_t qclen,msgcntlen;

  if(thread) {
    ptr  = thread->threadmsg->content;
    tmp  = get_time(vc,"DateFormatThreadView",&len,&thread->threadmsg->date);
  }

  utf8 = cf_strcmp(cs->values[0],"UTF-8") == 0;

  #ifdef CF_SHARED_MEM
  /* needed in shared memory */
  if(thread) {
    if((thread->messages->invisible == 1 || thread->threadmsg->invisible == 1) && !ShowInvisible) {
      str_error_message("E_FO_404",NULL,8);
      return FLT_EXIT;
    }
  }
  #endif

  tpl_cf_setvar(tpl,"charset",cs->values[0],strlen(cs->values[0]),0);

  if(UserName) {
    fbase     = cfg_get_first_value(dc,"UBaseURL");
    ps        = cfg_get_first_value(dc,"UPostScript");
  }
  else {
    fbase     = cfg_get_first_value(dc,"BaseURL");
    ps        = cfg_get_first_value(dc,"PostScript");
  }

  cf_set_variable(tpl,cs,"forumbase",fbase->values[0],strlen(fbase->values[0]),1);

  if(thread) {
    cf_set_variable(tpl,cs,"title",thread->threadmsg->subject,strlen(thread->threadmsg->subject),1);
    cf_set_variable(tpl,cs,"name",thread->threadmsg->author,strlen(thread->threadmsg->author),1);
    cf_set_variable(tpl,cs,"time",tmp,len,1);
    free(tmp);
  }

  if(utf8 || (qchars = htmlentities_charset_convert(Cfg.QuotingChars,"UTF-8",cs->values[0],&qclen,0)) == NULL) {
    qchars = strdup(Cfg.QuotingChars);
    qclen  = strlen(qchars);
  }

  if(thread) {
    if(thread->threadmsg->email && *thread->threadmsg->email) {
      cf_set_variable(tpl,cs,"email",thread->threadmsg->email,strlen(thread->threadmsg->email),1);
    }

    if(thread->threadmsg->category && *thread->threadmsg->category) {
      cf_set_variable(tpl,cs,"category",thread->threadmsg->category,strlen(thread->threadmsg->category),1);
    }

    if(thread->threadmsg->hp && *thread->threadmsg->hp) {
      cf_set_variable(tpl,cs,"link",thread->threadmsg->hp,strlen(thread->threadmsg->hp),1);
    }

    if(thread->threadmsg->img && *thread->threadmsg->img) {
      cf_set_variable(tpl,cs,"image",thread->threadmsg->img,strlen(thread->threadmsg->img),1);
    }
  }

  if(Cfg.TWidth) {
    tpl_cf_setvar(tpl,"twidth",Cfg.TWidth,strlen(Cfg.TWidth),1);
  }
  if(Cfg.THeight) {
    tpl_cf_setvar(tpl,"theight",Cfg.THeight,strlen(Cfg.THeight),1);
  }

  if(Cfg.Preview) {
    tpl_cf_setvar(tpl,"preview","1",1,0);
  }
  if(Cfg.PreviewSwitchType == 0) {
    tpl_cf_setvar(tpl,"previewswitchtype","checkbox",8,0);
  }
  else if(Cfg.PreviewSwitchType == 1) {
    tpl_cf_setvar(tpl,"previewswitchtype","button",6,0);
  }

  if(Cfg.ActiveColorF && *Cfg.ActiveColorF) {
    cf_set_variable(tpl,cs,"activecolorf",Cfg.ActiveColorF,strlen(Cfg.ActiveColorF),1);
  }
  if(Cfg.ActiveColorB && *Cfg.ActiveColorB) {
    cf_set_variable(tpl,cs,"activecolorb",Cfg.ActiveColorB,strlen(Cfg.ActiveColorB),1);
  }

  /*
   * has this posting parent postings?
   * If yes, set some variables
   */
  if(thread) {
    if(thread->threadmsg != thread->messages) {
      t_message *msg;
      if((msg = get_parent_message(thread->messages,thread->threadmsg)) != NULL) {
        tmp = get_time(vc,"DateFormatThreadView",&len,&msg->date);

        tpl_cf_setvar(tpl,"messagebefore","1",1,0);
        cf_set_variable(tpl,cs,"b_name",msg->author,strlen(msg->author),1);
        cf_set_variable(tpl,cs,"b_title",msg->subject,strlen(msg->subject),1);
        cf_set_variable(tpl,cs,"b_time",tmp,len,1);

        free(tmp);

        tmp = get_link(thread->tid,msg->mid);
        cf_set_variable(tpl,cs,"b_link",tmp,strlen(tmp),1);

        free(tmp);

        if(msg->category) {
          cf_set_variable(tpl,cs,"b_category",msg->category,strlen(msg->category),1);
        }
      }
    }
  }

  /* user values */
  if(name && *name->values[0]) {
    cf_set_variable(tpl,cs,thread?"aname":"Name",name->values[0],strlen(name->values[0]),1);
  }
  if(email && *email->values[0]) {
    cf_set_variable(tpl,cs,thread?"aemail":"EMail",email->values[0],strlen(email->values[0]),1);
  }
  if(hpurl && *hpurl->values[0]) {
    cf_set_variable(tpl,cs,thread?"aurl":"HomepageURL",hpurl->values[0],strlen(hpurl->values[0]),1);
  }
  if(imgurl && *imgurl->values[0]) {
    cf_set_variable(tpl,cs,thread?"aimg":"ImageURL",imgurl->values[0],strlen(imgurl->values[0]),1);
  }

  tpl_cf_setvar(tpl,"qchar","&#255;",6,0);
  tpl_cf_appendvar(tpl,"qchar",qchars,qclen);

  if(thread) {
    len = sprintf(buff,"%lld,%lld",thread->tid,thread->threadmsg->mid);
    tpl_cf_setvar(tpl,"fupto",buff,len,0);
  }

  len = gen_unid(buff,50);
  tpl_cf_setvar(tpl,"unid",buff,len,1);

  if(thread) str_init(&content);
  str_init(&cite);

  if(thread) {
  /* ok -- lets convert the message to the target charset with html encoded */
    if(utf8 || (msgcnt = charset_convert_entities(thread->threadmsg->content,thread->threadmsg->content_len,"UTF-8",cs->values[0],&msgcntlen)) == NULL) {
      msgcnt    = strdup(thread->threadmsg->content);
      msgcntlen = thread->threadmsg->content_len;
    }
  }

  /* greetings */
  if(Cfg.Hi && *Cfg.Hi) {
    if(utf8 || (tmp = htmlentities_charset_convert(Cfg.Hi,"UTF-8",cs->values[0],NULL,0)) == NULL) tmp = strdup(Cfg.Hi);
    replace_placeholders(tmp,&cite,thread);
    free(tmp);
  }

  /* transform message to html */
  if(thread) {
    msg_to_html(msgcnt,&content,&cite);
  }

  /* adoption */
  if(Cfg.Bye && *Cfg.Bye) {
    str_char_append(&cite,'\n');
    if(utf8 || (tmp = htmlentities_charset_convert(Cfg.Bye,"UTF-8",cs->values[0],NULL,0)) == NULL) tmp = strdup(Cfg.Bye);
    replace_placeholders(tmp,&cite,thread);
    free(tmp);
  }

  /* signature */
  if(Cfg.Signature && *Cfg.Signature) {
    str_chars_append(&cite,"\n-- \n",5);
    if(utf8 || (tmp = htmlentities_charset_convert(Cfg.Signature,"UTF-8",cs->values[0],NULL,0)) == NULL) tmp = strdup(Cfg.Signature);
    replace_placeholders(tmp,&cite,thread);
    free(tmp);
  }

  /*
   * we already convertet the message to our native charset some
   * lines above, so we use tpl_cf_setvar() instead of cf_set_variable()
   */
  if(thread) tpl_cf_setvar(tpl,"message",content.content,content.len,0);
  if(Cfg.DoQuote || !thread) tpl_cf_setvar(tpl,"cite",cite.content,cite.len,0);

  cf_set_variable(tpl,cs,"action",ps->values[0],strlen(ps->values[0]),1);

  str_cleanup(&cite);
  if(thread) str_cleanup(&content);
  if(thread) free(msgcnt);
  free(qchars);

  /* we're done in fo_post */
  if(!thread) return FLT_OK;

  /*
   * now, do the posting tree
   */
  if(cf_strcmp(Cfg.ShowThread,"none") != 0) {
    int level = 0;
    int slvl  = -1;

    if(cf_strcmp(Cfg.ShowThread,"partitial") == 0) {
      t_message *msg;
      int level = 0;

      for(msg=thread->messages;msg && msg->mid != thread->threadmsg->mid;msg=msg->next) {
        msg->may_show = 0;
      }

      level = msg->level;
      msg->may_show = 0;

      for(msg=msg->next;msg && msg->level > level;msg=msg->next);

      for(;msg;msg=msg->next) {
        msg->may_show = 0;
      }
    }
    else {
      tpl_cf_setvar(&thread->threadmsg->tpl,"active","1",1,0);
    }

    handle_thread(thread,head,1);

    tpl_cf_setvar(tpl,"threadlist","",0,0);
    for(msg=thread->messages;msg;msg=msg->next) {
      if((msg->may_show && msg->invisible == 0) || ShowInvisible == 1) {
        rc = handle_thread_list_posting(msg,head,thread->tid,1);
        if(ShowInvisible == 0 && (rc == FLT_EXIT || msg->may_show == 0)) continue;
        else if(slvl == -1) slvl = msg->level;

        date = get_time(vc,"DateFormatThreadList",&len,&msg->date);
        link = get_link(thread->tid,msg->mid);
        cf_set_variable(&msg->tpl,cs,"author",msg->author,strlen(msg->author),1);
        cf_set_variable(&msg->tpl,cs,"title",msg->subject,strlen(msg->subject),1);

        if(msg->category) {
          cf_set_variable(&msg->tpl,cs,"category",msg->category,strlen(msg->category),1);
        }

        if(date) {
          cf_set_variable(&msg->tpl,cs,"time",date,len,1);
          free(date);
        }

        if(link) {
          cf_set_variable(&msg->tpl,cs,"link",link,strlen(link),1);
          free(link);
        }

        if(msg->level < level) {
          for(;level>msg->level;level--) {
            tpl_cf_appendvar(tpl,"threadlist","</ul></li>",10);
          }
        }

        level = msg->level;

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

    for(;level>slvl;level--) {
      tpl_cf_appendvar(tpl,"threadlist","</ul></li>",10);
    }
  }

  return FLT_OK;
}
/* }}} */

/* {{{ module api functions */
/* {{{ flt_posting_api_get_qchars */
void *flt_posting_api_get_qchars(void *arg) {
  return (void *)Cfg.QuotingChars;
}
/* }}} */

/* {{{ flt_posting_api_msg_to_html */
void *flt_posting_api_msg_to_html(void *arg) {
  t_string *content,*cite;
  const u_char *msg;

        msg      = *((const u_char **)arg);
        arg     += sizeof(const u_char *);
        content  = *((t_string **)arg);
        arg     += sizeof(t_string *);
        cite     = *((t_string **)arg);

        msg_to_html(msg,content,cite);

        return NULL;
}
/* }}} */
/* }}} */

/* {{{ module configuration */
/* {{{ handle_q */
int handle_q(t_configfile *cfile,t_conf_opt *opt,u_char **args,int argnum) {
  if(cf_strcmp(opt->name,"DoQuote") == 0) {
    Cfg.DoQuote = cf_strcmp(args[0],"yes") == 0 ? 1 : 0;
  }
  else if(cf_strcmp(opt->name,"QuotingChars") == 0) {
    if(Cfg.QuotingChars) free(Cfg.QuotingChars);
    Cfg.QuotingChars = strdup(args[0]);
  }

  return 0;
}
/* }}} */

/* {{{ handle_st */
int handle_st(t_configfile *cfile,t_conf_opt *opt,u_char **args,int argnum) {
  if(Cfg.ShowThread) free(Cfg.ShowThread);
  Cfg.ShowThread = strdup(args[0]);

  return 0;
}
/* }}} */

/* {{{ handle_greet */
int handle_greet(t_configfile *cfile,t_conf_opt *opt,u_char **args,int argnum) {
  u_char *tmp = strdup(args[0]);

  if(cf_strcmp(opt->name,"Hi") == 0) {
    if(Cfg.Hi) free(Cfg.Hi);
    Cfg.Hi          = tmp;
  }
  else if(cf_strcmp(opt->name,"Bye") == 0) {
    if(Cfg.Bye) free(Cfg.Bye);
    Cfg.Bye         = tmp;
  }
  else if(cf_strcmp(opt->name,"Signature") == 0) {
    if(Cfg.Signature) free(Cfg.Signature);
    Cfg.Signature   = tmp;
  }

  return 0;
}
/* }}} */

/* {{{ handle_link */
int handle_link(t_configfile *cfile,t_conf_opt *opt,u_char **args,int argnum) {
  if(Cfg.link) free(Cfg.link);
  Cfg.link = strdup(args[0]);

  return 0;
}
/* }}} */

/* {{{ handle_sig */
int handle_sig(t_configfile *cfile,t_conf_opt *opt,u_char **args,int argnum) {
  Cfg.ShowSig = cf_strcmp(args[0],"yes") == 0 ? 1 : 0;

  return 0;
}
/* }}} */

/* {{{ handle_box */
int handle_box(t_configfile *cfile,t_conf_opt *opt,u_char **args,int argnum) {
  if(Cfg.TWidth) free(Cfg.TWidth);
  if(Cfg.THeight) free(Cfg.THeight);

  Cfg.TWidth  = strdup(args[0]);
  Cfg.THeight = strdup(args[1]);

  return 0;
}
/* }}} */

/* {{{ handle_prev */
int handle_prev(t_configfile *cfile,t_conf_opt *opt,u_char **args,int argnum) {
  Cfg.Preview = cf_strcmp(args[0],"yes") == 0;

  return 0;
}
/* }}} */

/* {{{ handle_prevt */
int handle_prevt(t_configfile *cfile,t_conf_opt *opt,u_char **args,int argnum) {
  if(cf_strcmp(args[0],"button") == 0) {
    Cfg.PreviewSwitchType = 1;
  }
  else if(cf_strcmp(args[0],"checkbox") == 0) {
    Cfg.PreviewSwitchType = 0;
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
  if(Cfg.ActiveColorF) free(Cfg.ActiveColorF);
  if(Cfg.ActiveColorB) free(Cfg.ActiveColorB);

  Cfg.ActiveColorF = strdup(args[0]);
  Cfg.ActiveColorB = strdup(args[1]);

  return 0;
}
/* }}} */

/* {{{ handle_iframe */
int handle_iframe(t_configfile *cfile,t_conf_opt *opt,u_char **args,int argnum) {
  Cfg.IframeAsLink = cf_strcmp(args[0],"yes") == 0;
  return 0;
}
/* }}} */

/* {{{ handle_image */
int handle_image(t_configfile *cfile,t_conf_opt *opt,u_char **args,int argnum) {
  Cfg.ImageAsLink = cf_strcmp(args[0],"yes") == 0;
  return 0;
}
/* }}} */

/* {{{ cleanup */
void cleanup(void) {
  if(Cfg.Hi)           free(Cfg.Hi);
  if(Cfg.Bye)          free(Cfg.Bye);
  if(Cfg.Signature)    free(Cfg.Signature);
  if(Cfg.ShowThread)   free(Cfg.ShowThread);
  if(Cfg.QuotingChars) free(Cfg.QuotingChars);
  if(Cfg.link)         free(Cfg.link);
  if(Cfg.ActiveColorF) free(Cfg.ActiveColorF);
  if(Cfg.ActiveColorB) free(Cfg.ActiveColorB);
}
/* }}} */

/* {{{ register_hooks */
int register_hooks(t_cf_hash *cgi,t_configuration *dc,t_configuration *vc) {
  cf_register_mod_api_ent("flt_posting","get_qchars",flt_posting_api_get_qchars);
  cf_register_mod_api_ent("flt_posting","msg_to_html",flt_posting_api_msg_to_html);

  return FLT_OK;
}
/* }}} */

/* {{{ t_conf_opt config[] */
t_conf_opt config[] = {
  { "DoQuote",                    handle_q,        NULL },
  { "QuotingChars",               handle_q,        NULL },
  { "ShowThread",                 handle_st,       NULL },
  { "Hi",                         handle_greet,    NULL },
  { "Bye",                        handle_greet,    NULL },
  { "Signature",                  handle_greet,    NULL },
  { "PostingLinkTarget",          handle_link,     NULL },
  { "ShowSig",                    handle_sig,      NULL },
  { "TextBox",                    handle_box,      NULL },
  { "GeneratePreview",            handle_prev,     NULL },
  { "PreviewSwitchType",          handle_prevt,    NULL },
  { "ActivePostingColor",         handle_actpcol,  NULL },
  { "ShowIframeAsLink",           handle_iframe,   NULL },
  { "ShowImageAsLink",            handle_image,    NULL },
  { NULL, NULL, NULL }
};
/* }}} */

/* {{{ t_handler_config handlers[] */
t_handler_config handlers[] = {
  { INIT_HANDLER, register_hooks    },
  { POSTING_HANDLER, execute_filter },
  { 0, NULL }
};
/* }}} */

/* {{{ t_module_config flt_posting */
t_module_config flt_posting = {
  config,
  handlers,
  NULL,
  NULL,
  NULL,
  cleanup
};
/* }}} */
/* }}} */

/* eof */
