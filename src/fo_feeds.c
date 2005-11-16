/**
 * \file fo_feeds.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 * \brief The forum feeds manager program
 */

/* {{{ Initial comment */
/*
 * $LastChangedDate$
 * $LastChangedRevision$
 * $LastChangedBy$
 *
 */
/* }}} */

/* {{{ Includes */
#include "cfconfig.h"
#include "defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <errno.h>

#include <sys/types.h>
#include <signal.h>

/* socket includes */
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/un.h>

#include <locale.h>

#ifdef CF_SHARED_MEM
#include <sys/ipc.h>
#include <sys/shm.h>
#endif

#include "readline.h"
#include "hashlib.h"
#include "utils.h"
#include "cfgcomp.h"
#include "cfcgi.h"
#include "template.h"
#include "clientlib.h"
#include "charconvert.h"
#include "htmllib.h"
#include "fo_view.h"
/* }}} */

#define CF_MODE_RSS  0
#define CF_MODE_ATOM 1

/* {{{ bottoms */
void atom_bottom(cf_string_t *str) {
  cf_str_chars_append(str,"</feed>\n",8);
}

void rss_bottom(cf_string_t *str) {
  cf_str_chars_append(str,"</channel></rss>\n",17);
}
/* }}} */

/* {{{ date generation */
void w3c_datetime(cf_string_t *str,time_t date) {
  u_char buff[512];
  size_t len;
  struct tm *tm;


  if((tm = localtime(&date)) == NULL) return;

  len = strftime(buff,512,"%Y-%m-%dT%H:%M:%S%z",tm);
  cf_str_chars_append(str,buff,len-2);
  cf_str_char_append(str,':');
  cf_str_chars_append(str,buff+len-2,2);
}

void rfc822_date(cf_string_t *str,time_t date) {
  u_char *fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);

  u_char buff[512];
  size_t len;
  struct tm *tm;

  cf_name_value_t *lc = cf_cfg_get_first_value(&fo_feeds_conf,fn,"DateLocaleEn");

  setlocale(LC_TIME,lc->values[0]);

  if((tm = localtime(&date)) == NULL) return;

  len = strftime(buff,512,"%a, %d %b %Y %H:%M:%S %z",tm);
  cf_str_chars_append(str,buff,len);
}
/* }}} */

/* {{{ description generation */
void gen_description(cf_string_t *str,const u_char *descr,cl_thread_t *thread) {
  register u_char *ptr;

  for(ptr=(u_char *)descr;*ptr;++ptr) {
    switch(*ptr) {
      case '%':
        switch(*(ptr+1)) {
          case 't':
            cf_str_str_append(str,&thread->messages->subject);
            ++ptr;
            continue;
          case '%':
            cf_str_char_append(str,*ptr);
            ++ptr;
            continue;
        }

      default:
        cf_str_char_append(str,*ptr);
    }
  }
}
/* }}} */

/* {{{ atom_ and rss_head */
void atom_head(cf_string_t *str,cl_thread_t *thread) {
  u_char *fn = cf_hash_get(GlobalValues,"FORUM_NAME",10),*tmp = NULL,*tmp1 = NULL;
  u_char *uname = cf_hash_get(GlobalValues,"UserName",8);

  cf_name_value_t *atom_title  = cf_cfg_get_first_value(&fo_feeds_conf,fn,"AtomTitle");
  cf_name_value_t *atom_tgline = cf_cfg_get_first_value(&fo_feeds_conf,fn,"AtomTagline");
  cf_name_value_t *atom_lang   = cf_cfg_get_first_value(&fo_feeds_conf,fn,"FeedLang");

  cf_name_value_t *burl = cf_cfg_get_first_value(&fo_default_conf,fn,uname ? "UBaseURL":"BaseURL");
  //cf_name_value_t *purl = cf_cfg_get_first_value(&fo_default_conf,fn,cf_hash_get(GlobalValues,"UserName",8) ? "UPostingURL":"PostingURL");

  cf_readmode_t *rm = cf_hash_get(GlobalValues,"RM",2);

  time_t t = time(NULL);

  if(thread) {
    tmp = cf_get_link(rm->posting_uri[uname?1:0],thread->tid,thread->messages->mid);
    tmp1 = htmlentities(tmp,0);
  }

  cf_str_chars_append(str,"<?xml version=\"1.0\"?>\n" \
    "<feed version=\"0.3\" xmlns=\"http://purl.org/atom/ns#",
    73
  );
  if(atom_lang) {
    cf_str_chars_append(str,"\" xml:lang=\"",12);
    cf_str_chars_append(str,atom_lang->values[0],strlen(atom_lang->values[0]));
  }
  cf_str_chars_append(str,"\">\n",3);

  cf_str_chars_append(str,"<title>",7);
  if(thread) {
    cf_str_chars_append(str,"<![CDATA[",9);
    cf_str_str_append(str,&thread->messages->subject);
    cf_str_chars_append(str,"]]>",3);
  }
  else cf_str_chars_append(str,atom_title->values[0],strlen(atom_title->values[0]));
  cf_str_chars_append(str,"</title>",8);

  cf_str_chars_append(str,"<link rel=\"alternate\" type=\"text/html\" href=\"",45);
  if(thread) cf_str_chars_append(str,tmp1,strlen(tmp1));
  else cf_str_chars_append(str,burl->values[0],strlen(burl->values[0]));
  cf_str_chars_append(str,"\"/>",3);

  if(atom_tgline) {
    cf_str_chars_append(str,"<tagline>",9);
    cf_str_chars_append(str,atom_tgline->values[0],strlen(atom_tgline->values[0]));
    cf_str_chars_append(str,"</tagline>",10);
  }

  cf_str_chars_append(str,"<modified>",10);
  if(thread) w3c_datetime(str,thread->newest->date);
  else w3c_datetime(str,t);
  cf_str_chars_append(str,"</modified>",11);

  cf_str_chars_append(str,"<generator>Classic Forum V.",27);
  cf_str_chars_append(str,CF_VERSION,strlen(CF_VERSION));
  cf_str_chars_append(str,"</generator>",12);

  if(thread) {
    cf_str_chars_append(str,"<author>",8);

    cf_str_chars_append(str,"<name><![CDATA[",15);
    cf_str_str_append(str,&thread->messages->author);
    cf_str_chars_append(str,"]]></name>",10);

    if(thread->messages->hp.len) {
      cf_str_chars_append(str,"<url><![CDATA[",14);
      cf_str_str_append(str,&thread->messages->hp);
      cf_str_chars_append(str,"]]></url>",9);
    }

    if(thread->messages->email.len) {
      cf_str_chars_append(str,"<email><![CDATA[",16);
      cf_str_str_append(str,&thread->messages->email);
      cf_str_chars_append(str,"]]></email>",11);
    }

    cf_str_chars_append(str,"</author>",9);
  }

}

void rss_head(cf_string_t *str,cl_thread_t *thread) {
  u_char *fn = cf_hash_get(GlobalValues,"FORUM_NAME",10),*tmp;
  cf_name_value_t *rss_title = cf_cfg_get_first_value(&fo_feeds_conf,fn,"RSSTitle");
  cf_name_value_t *rss_descr = cf_cfg_get_first_value(&fo_feeds_conf,fn,thread ? "RSSDescriptionThread" : "RSSDescription");
  cf_name_value_t *rss_copy  = cf_cfg_get_first_value(&fo_feeds_conf,fn,"RSSCopyright");
  cf_name_value_t *rss_lang  = cf_cfg_get_first_value(&fo_feeds_conf,fn,"FeedLang");
  cf_name_value_t *rss_wbm   = cf_cfg_get_first_value(&fo_feeds_conf,fn,"RSSWebMaster");
  cf_name_value_t *rss_cat   = cf_cfg_get_first_value(&fo_feeds_conf,fn,"RSSCategory");

  cf_name_value_t *burl = cf_cfg_get_first_value(&fo_default_conf,fn,cf_hash_get(GlobalValues,"UserName",8) ? "BaseURL":"UBaseURL");
  cf_name_value_t *purl;

  cf_str_chars_append(str,"<?xml version=\"1.0\"?>\n" \
    "<rss version=\"2.0\"\n" \
    "  xmlns:dc=\"http://purl.org/dc/elements/1.1/\"\n" \
    ">\n\n" \
    "<channel>\n",
    100
  );

  cf_str_chars_append(str,"<title>",7);
  if(thread) {
    cf_str_chars_append(str,"<![CDATA[",9);
    cf_str_str_append(str,&thread->messages->subject);
    cf_str_chars_append(str,"]]>",3);
  }
  else cf_str_chars_append(str,rss_title->values[0],strlen(rss_title->values[0]));
  cf_str_chars_append(str,"</title>",8);

  cf_str_chars_append(str,"<link>",6);
  if(thread) {
    purl = cf_cfg_get_first_value(&fo_default_conf,fn,cf_hash_get(GlobalValues,"UserName",8) ? "UPostingURL":"PostingURL");
    tmp = cf_get_link(purl->values[0],thread->tid,thread->messages->mid);
    cf_str_chars_append(str,"<![CDATA[",9);
    cf_str_chars_append(str,tmp,strlen(tmp));
    cf_str_chars_append(str,"]]>",3);
  }
  else cf_str_chars_append(str,burl->values[0],strlen(burl->values[0]));
  cf_str_chars_append(str,"</link>",7);

  cf_str_chars_append(str,"<description><![CDATA[",22);
  if(thread) gen_description(str,rss_descr->values[0],thread);
  else cf_str_chars_append(str,rss_descr->values[0],strlen(rss_descr->values[0]));
  cf_str_chars_append(str,"]]></description>",17);

  if(rss_copy) {
    cf_str_chars_append(str,"<copyright>",11);
    cf_str_chars_append(str,rss_copy->values[0],strlen(rss_copy->values[0]));
    cf_str_chars_append(str,"</copyright>",12);
  }

  if(rss_lang) {
    cf_str_chars_append(str,"<language>",10);
    cf_str_chars_append(str,rss_lang->values[0],strlen(rss_lang->values[0]));
    cf_str_chars_append(str,"</language>",11);
  }

  if(rss_wbm) {
    cf_str_chars_append(str,"<webMaster>",11);
    cf_str_chars_append(str,rss_wbm->values[0],strlen(rss_wbm->values[0]));
    cf_str_chars_append(str,"</webMaster>",12);
  }

  if(rss_cat) {
    cf_str_chars_append(str,"<category>",10);
    cf_str_chars_append(str,rss_cat->values[0],strlen(rss_cat->values[0]));
    cf_str_chars_append(str,"</category>",11);
  }
  if(thread && thread->messages->category.len) {
    cf_str_chars_append(str,"<category>",10);
    cf_str_str_append(str,&thread->messages->category);
    cf_str_chars_append(str,"</category>",11);
  }

  cf_str_chars_append(str,"<generator>Classic Forum V.",27);
  cf_str_chars_append(str,CF_VERSION,strlen(CF_VERSION));
  cf_str_chars_append(str,"</generator>",12);
}
/* }}} */

/* {{{ atom_thread */
void atom_thread(cf_string_t *str,cl_thread_t *thread,cf_hash_t *head) {
  message_t *msg;

  u_char *tmp,*tmp1,*forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  u_char *uname = cf_hash_get(GlobalValues,"UserName",8);
  size_t len1;

  cf_string_t tmpstr;

  //cf_name_value_t *burl   = cf_cfg_get_first_value(&fo_default_conf,forum_name,uname ? "UPostingURL":"PostingURL");
  cf_name_value_t *qchars = cf_cfg_get_first_value(&fo_view_conf,forum_name,"QuotingChars");
  cf_name_value_t *ms     = cf_cfg_get_first_value(&fo_view_conf,forum_name,"MaxSigLines");
  cf_name_value_t *ss     = cf_cfg_get_first_value(&fo_view_conf,forum_name,"ShowSig");

  cf_readmode_t *rm = cf_hash_get(GlobalValues,"RM",2);

  /* {{{ run handlers in pre and post mode */
  cf_run_view_handlers(thread,head,CF_MODE_THREADVIEW|CF_MODE_PRE|CF_MODE_XML);
  for(msg=thread->messages;msg;msg=msg->next) cf_run_view_list_handlers(msg,head,thread->tid,CF_MODE_THREADVIEW|CF_MODE_XML);
  cf_run_view_handlers(thread,head,CF_MODE_THREADVIEW|CF_MODE_POST|CF_MODE_XML);
  /* }}} */

  for(msg=thread->messages;msg;msg=msg->next) {
    if(msg->may_show && msg->invisible == 0) {
      tmp = cf_get_link(rm->posting_uri[uname?1:0],thread->tid,msg->mid);
      tmp1 = htmlentities(tmp,0);
      len1 = strlen(tmp1);

      cf_str_chars_append(str,"<entry>",7);

      /* {{{ author of this thread */
      cf_str_chars_append(str,"<author>",8);

      cf_str_chars_append(str,"<name><![CDATA[",15);
      cf_str_str_append(str,&thread->messages->author);
      cf_str_chars_append(str,"]]></name>",10);

      if(msg->hp.len) {
        cf_str_chars_append(str,"<url><![CDATA[",14);
        cf_str_str_append(str,&msg->hp);
        cf_str_chars_append(str,"]]></url>",9);
      }

      if(msg->email.len) {
        cf_str_chars_append(str,"<email><![CDATA[",16);
        cf_str_str_append(str,&msg->email);
        cf_str_chars_append(str,"]]></email>",11);
      }

      cf_str_chars_append(str,"</author>",9);
      /* }}} */

      cf_str_chars_append(str,"<title><![CDATA[",16);
      cf_str_str_append(str,&msg->subject);
      cf_str_chars_append(str,"]]></title>",11);

      cf_str_chars_append(str,"<link rel=\"alternate\" type=\"text/html\" href=\"",45);
      cf_str_chars_append(str,tmp1,len1);
      cf_str_chars_append(str,"\"/>",3);

      cf_str_chars_append(str,"<id>",4);
      cf_str_chars_append(str,tmp1,len1);
      cf_str_chars_append(str,"</id>",5);

      cf_str_chars_append(str,"<modified>",10);
      w3c_datetime(str,thread->newest->date);
      cf_str_chars_append(str,"</modified>",11);

      cf_str_chars_append(str,"<issued>",8);
      w3c_datetime(str,thread->messages->date);
      cf_str_chars_append(str,"</issued>",9);

      /* {{{ content */
      cf_str_chars_append(str,"<content type=\"text/html\" mode=\"escaped\"><![CDATA[",50);
      cf_str_init(&tmpstr);
      msg_to_html(
        thread,
        msg->content.content,
        &tmpstr,
        NULL,
        qchars->values[0],
        ms ? atoi(ms->values[0]) : -1,
        ss ? cf_strcmp(ss->values[0],"yes") == 0 : 0
      );
      cf_str_str_append(str,&tmpstr);
      cf_str_chars_append(str,"]]></content>",13);
      cf_str_cleanup(&tmpstr);
      /* }}} */

      cf_str_chars_append(str,"</entry>\n",9);

      free(tmp);
      free(tmp1);
    }
  }
}
/* }}} */

/* {{{ rss_thread */
void rss_thread(cf_string_t *str,cl_thread_t *thread,cf_hash_t *head) {
  message_t *msg;

  u_char *tmp,*tmp1,*forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  u_char *uname = cf_hash_get(GlobalValues,"UserName",8);
  size_t len;

  cf_string_t tmpstr;

  cf_name_value_t *burl   = cf_cfg_get_first_value(&fo_default_conf,forum_name,uname ? "UPostingURL":"PostingURL");
  cf_name_value_t *qchars = cf_cfg_get_first_value(&fo_view_conf,forum_name,"QuotingChars");
  cf_name_value_t *ms     = cf_cfg_get_first_value(&fo_view_conf,forum_name,"MaxSigLines");
  cf_name_value_t *ss     = cf_cfg_get_first_value(&fo_view_conf,forum_name,"ShowSig");

  cf_readmode_t *rm = cf_hash_get(GlobalValues,"RM",2);

  /* {{{ run handlers in pre and post mode */
  cf_run_view_handlers(thread,head,CF_MODE_THREADVIEW|CF_MODE_PRE|CF_MODE_XML);
  for(msg=thread->messages;msg;msg=msg->next) cf_run_view_list_handlers(msg,head,thread->tid,CF_MODE_THREADVIEW|CF_MODE_XML);
  cf_run_view_handlers(thread,head,CF_MODE_THREADVIEW|CF_MODE_POST|CF_MODE_XML);
  /* }}} */

  for(msg=thread->messages;msg;msg=msg->next) {
    if(msg->may_show && msg->invisible == 0) {
      tmp  = cf_get_link(rm->posting_uri[uname?1:0],thread->tid,msg->mid);
      tmp1 = cf_get_link(burl->values[0],thread->tid,msg->mid);

      len = strlen(tmp);

      cf_str_chars_append(str,"<item>",6);

      cf_str_chars_append(str,"<dc:creator><![CDATA[",21);
      cf_str_str_append(str,&msg->author);
      if(msg->email.len) {
        cf_str_chars_append(str," (mailto:",9);
        cf_str_str_append(str,&msg->email);
        cf_str_char_append(str,')');
      }
      cf_str_chars_append(str,"]]></dc:creator>",16);

      cf_str_chars_append(str,"<title><![CDATA[",16);
      cf_str_str_append(str,&msg->subject);
      cf_str_chars_append(str,"]]></title>",11);

      cf_str_chars_append(str,"<link><![CDATA[",15);
      cf_str_chars_append(str,tmp,len);
      cf_str_chars_append(str,"]]></link>",10);

      cf_str_chars_append(str,"<pubDate>",9);
      rfc822_date(str,msg->date);
      cf_str_chars_append(str,"</pubDate>",10);

      if(msg->category.len) {
        cf_str_chars_append(str,"<category>",10);
        cf_str_str_append(str,&msg->category);
        cf_str_chars_append(str,"</category>",11);
      }

      cf_str_chars_append(str,"<guid isPermaLink=\"true\"><![CDATA[",34);
      cf_str_chars_append(str,tmp1,len);
      cf_str_chars_append(str,"]]></guid>",10);

      /* {{{ description */
      cf_str_chars_append(str,"<description>",13);

      cf_str_init(&tmpstr);
      msg_to_html(
        thread,
        msg->content.content,
        &tmpstr,
        NULL,
        qchars->values[0],
        ms ? atoi(ms->values[0]) : -1,
        ss ? cf_strcmp(ss->values[0],"yes") == 0 : 0
      );
      cf_str_chars_append(str,"<![CDATA[",9);
      cf_str_str_append(str,&tmpstr);
      cf_str_cleanup(&tmpstr);

      cf_str_chars_append(str,"]]>",3);
      cf_str_chars_append(str,"</description>",14);
      /* }}} */

      cf_str_chars_append(str,"</item>\n",8);

      free(tmp);
      free(tmp1);
    }
  }
}
/* }}} */

/* {{{ show_thread */
#ifndef CF_SHARED_MEM
void show_thread(cf_hash_t *head,int sock,u_int64_t tid)
#else
void show_thread(cf_hash_t *head,void *sock,u_int64_t tid)
#endif
{
  cl_thread_t thread;

  #ifndef CF_SHARED_MEM
  rline_t tsd;
  #endif

  u_char *forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  cf_string_t *tmp;

  cf_name_value_t *cs = cf_cfg_get_first_value(&fo_default_conf,forum_name,"ExternCharset");

  cf_string_t cnt;

  int del = cf_hash_get(GlobalValues,"ShowInvisible",13) == NULL ? CF_KILL_DELETED : CF_KEEP_DELETED,mode = CF_MODE_RSS;

  if(head) {
    if((tmp = cf_cgi_get(head,"m")) != NULL) {
      if(cf_strcmp(tmp->content,"atom") == 0) mode = CF_MODE_ATOM;
      else mode = CF_MODE_RSS;
    }
  }

  cf_str_init(&cnt);
  memset(&thread,0,sizeof(thread));

  /* {{{ init and get message from server */
  #ifndef CF_SHARED_MEM
  memset(&tsd,0,sizeof(tsd));
  #endif

  #ifndef CF_SHARED_MEM
  if(cf_get_message_through_sock(sock,&tsd,&thread,tid,0,del) == -1)
  #else
  if(cf_get_message_through_shm(shm_ptr,&thread,tid,0,del) == -1)
  #endif
  {
    if(cf_strcmp(ErrorString,"E_FO_404") == 0) {
      if(cf_run_404_handlers(head,tid,0) != FLT_EXIT) {
        printf("Status: 404 Not Found\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
        cf_error_message(ErrorString,NULL);
      }
    }
    else {
      printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
      cf_error_message(ErrorString,NULL);
    }
    return;
  }
  /* }}} */

  #ifndef CF_NO_SORTING
  #ifdef CF_SHARED_MEM
  cf_run_thread_sorting_handlers(head,shm_ptr,&thread);
  #else
  cf_run_thread_sorting_handlers(head,sock,&tsd,&thread);
  #endif
  #endif

  /* ok, seems to be all right, send headers */
  switch(mode) {
    case CF_MODE_ATOM:
      printf("Content-Type: application/atom+xml; charset=UTF-8\015\012\015\012");
      atom_head(&cnt,&thread);
      atom_thread(&cnt,&thread,head);
      atom_bottom(&cnt);
      break;

    default:
      printf("Content-Type: application/rss+xml; charset=UTF-8\015\012\015\012");
      rss_head(&cnt,&thread);
      rss_thread(&cnt,&thread,head);
      rss_bottom(&cnt);
      break;
  }

  cf_cleanup_thread(&thread);

  fwrite(cnt.content,1,cnt.len,stdout);
  cf_str_cleanup(&cnt);
}
/* }}} */

/* {{{ threadlist atom */
void gen_threadlist_atom(cl_thread_t *thread,cf_string_t *str) {
  u_char *fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  u_char *tmp,*tmp1,*tmp2,*tmp3,*uname = cf_hash_get(GlobalValues,"UserName",8);
  size_t len1,len2;

  cf_readmode_t *rm = cf_hash_get(GlobalValues,"RM",2);

  cf_name_value_t *burl   = cf_cfg_get_first_value(&fo_default_conf,fn,uname ? "UPostingURL":"PostingURL");
  cf_name_value_t *qchars = cf_cfg_get_first_value(&fo_view_conf,fn,"QuotingChars");
  cf_name_value_t *ms     = cf_cfg_get_first_value(&fo_view_conf,fn,"MaxSigLines");
  cf_name_value_t *ss     = cf_cfg_get_first_value(&fo_view_conf,fn,"ShowSig");

  cf_string_t tmpstr;

  tmp = cf_get_link(rm->posting_uri[uname?1:0],thread->tid,thread->messages->mid);
  tmp1 = htmlentities(tmp,0);
  len1 = strlen(tmp1);

  tmp2 = cf_get_link(burl->values[0],thread->tid,thread->messages->mid);
  tmp3 = htmlentities(tmp2,0);
  len2 = strlen(tmp3);

  cf_str_chars_append(str,"<entry>",7);

  /* {{{ author of this thread */
  cf_str_chars_append(str,"<author>",8);

  cf_str_chars_append(str,"<name><![CDATA[",15);
  cf_str_str_append(str,&thread->messages->author);
  cf_str_chars_append(str,"]]></name>",10);

  if(thread->messages->hp.len) {
    cf_str_chars_append(str,"<url><![CDATA[",14);
    cf_str_str_append(str,&thread->messages->hp);
    cf_str_chars_append(str,"]]></url>",9);
  }

  if(thread->messages->email.len) {
    cf_str_chars_append(str,"<email><![CDATA[",16);
    cf_str_str_append(str,&thread->messages->email);
    cf_str_chars_append(str,"]]></email>",11);
  }

  cf_str_chars_append(str,"</author>",9);
  /* }}} */

  cf_str_chars_append(str,"<title><![CDATA[",16);
  cf_str_str_append(str,&thread->messages->subject);
  cf_str_chars_append(str,"]]></title>",11);

  cf_str_chars_append(str,"<link rel=\"alternate\" type=\"text/html\" href=\"",45);
  cf_str_chars_append(str,tmp1,len1);
  cf_str_chars_append(str,"\"/>",3);

  cf_str_chars_append(str,"<id>",4);
  cf_str_chars_append(str,tmp3,len2);
  cf_str_chars_append(str,"</id>",5);

  cf_str_chars_append(str,"<modified>",10);
  w3c_datetime(str,thread->newest->date);
  cf_str_chars_append(str,"</modified>",11);

  cf_str_chars_append(str,"<issued>",8);
  w3c_datetime(str,thread->messages->date);
  cf_str_chars_append(str,"</issued>",9);

  /* {{{ content */
  cf_str_chars_append(str,"<content type=\"text/html\" mode=\"escaped\"><![CDATA[",50);
  cf_str_init(&tmpstr);
  msg_to_html(
    thread,
    thread->messages->content.content,
    &tmpstr,
    NULL,
    qchars->values[0],
    ms ? atoi(ms->values[0]) : -1,
    ss ? cf_strcmp(ss->values[0],"yes") == 0 : 0
  );
  cf_str_str_append(str,&tmpstr);
  cf_str_chars_append(str,"]]></content>",13);
  cf_str_cleanup(&tmpstr);
  /* }}} */

  cf_str_chars_append(str,"</entry>\n",9);

  free(tmp);
  free(tmp1);
  free(tmp2);
  free(tmp3);
}
/* }}} */

/* {{{ threadlist rss */
void gen_threadlist_rss(cl_thread_t *thread,cf_string_t *str) {
  u_char *fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  u_char *tmp,*tmp1,*uname = cf_hash_get(GlobalValues,"UserName",8);
  size_t len,len1;

  cf_readmode_t *rm = cf_hash_get(GlobalValues,"RM",2);

  cf_string_t tmpstr;

  cf_name_value_t *burl   = cf_cfg_get_first_value(&fo_default_conf,fn,uname ? "UPostingURL":"PostingURL");
  cf_name_value_t *qchars = cf_cfg_get_first_value(&fo_view_conf,fn,"QuotingChars");
  cf_name_value_t *ms     = cf_cfg_get_first_value(&fo_view_conf,fn,"MaxSigLines");
  cf_name_value_t *ss     = cf_cfg_get_first_value(&fo_view_conf,fn,"ShowSig");

  tmp = cf_get_link(burl->values[0],thread->tid,thread->messages->mid);
  len = strlen(tmp);

  tmp1 = cf_get_link(rm->posting_uri[uname?1:0],thread->tid,thread->messages->mid);
  len1 = strlen(tmp1);

  cf_str_chars_append(str,"<item>",6);

  cf_str_chars_append(str,"<dc:creator><![CDATA[",21);
  cf_str_str_append(str,&thread->messages->author);
  if(thread->messages->email.len) {
    cf_str_chars_append(str," (mailto:",9);
    cf_str_str_append(str,&thread->messages->email);
    cf_str_char_append(str,')');
  }
  cf_str_chars_append(str,"]]></dc:creator>",16);

  cf_str_chars_append(str,"<title><![CDATA[",16);
  cf_str_str_append(str,&thread->messages->subject);
  cf_str_chars_append(str,"]]></title>",11);

  cf_str_chars_append(str,"<link><![CDATA[",15);
  cf_str_chars_append(str,tmp1,len1);
  cf_str_chars_append(str,"]]></link>",10);

  cf_str_chars_append(str,"<pubDate>",9);
  rfc822_date(str,thread->messages->date);
  cf_str_chars_append(str,"</pubDate>",10);

  if(thread->messages->category.len) {
    cf_str_chars_append(str,"<category>",10);
    cf_str_str_append(str,&thread->messages->category);
    cf_str_chars_append(str,"</category>",11);
  }

  cf_str_chars_append(str,"<guid isPermaLink=\"true\"><![CDATA[",34);
  cf_str_chars_append(str,tmp,len);
  cf_str_chars_append(str,"]]></guid>",10);

  /* {{{ description */
  cf_str_chars_append(str,"<description>",13);

  cf_str_init(&tmpstr);
  msg_to_html(
    thread,
    thread->messages->content.content,
    &tmpstr,
    NULL,
    qchars->values[0],
    ms ? atoi(ms->values[0]) : -1,
    ss ? cf_strcmp(ss->values[0],"yes") == 0 : 0
  );
  cf_str_chars_append(str,"<![CDATA[",9);
  cf_str_str_append(str,&tmpstr);
  cf_str_cleanup(&tmpstr);

  cf_str_chars_append(str,"]]>",3);
  cf_str_chars_append(str,"</description>",14);
  /* }}} */

  cf_str_chars_append(str,"</item>\n",8);

  free(tmp);
  free(tmp1);
}
/* }}} */

/* {{{ show_threadlist */
#ifndef CF_SHARED_MEM
void show_threadlist(int sock,cf_hash_t *head)
#else
void show_threadlist(void *shm_ptr,cf_hash_t *head)
#endif
{
  /* {{{ variables */
  int ret;
  #ifndef CF_SHARED_MEM
  rline_t tsd;
  u_char *line;
  size_t len;
  u_char buff[512];
  #else
  void *ptr,*ptr1;
  #endif

  u_char *forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10),
    *UserName = cf_hash_get(GlobalValues,"UserName",8);

  cf_name_value_t *fbase,*cs = cf_cfg_get_first_value(&fo_default_conf,forum_name,"ExternCharset");

  cl_thread_t thread,*threadp;
  message_t *msg;
  size_t i;
  int del = cf_hash_get(GlobalValues,"ShowInvisible",13) == NULL ? CF_KILL_DELETED : CF_KEEP_DELETED,mode = CF_MODE_RSS;

  cf_string_t cnt,*tmp;

  #ifndef CF_NO_SORTING
  cf_array_t threads;
  #endif
  /* }}} */

  cf_str_init(&cnt);

  if(head) {
    if((tmp = cf_cgi_get(head,"m")) != NULL) {
      if(cf_strcmp(tmp->content,"atom") == 0) mode = CF_MODE_ATOM;
      else mode = CF_MODE_RSS;
    }
  }

  #ifndef CF_SHARED_MEM
  memset(&tsd,0,sizeof(tsd));
  #endif

  /* {{{ if not in shm mode, request the threadlist from
   * the forum server. If in shm mode, request the
   * shm pointer
   */
  #ifndef CF_SHARED_MEM
  len = snprintf(buff,128,"SELECT %s\n",forum_name);
  writen(sock,buff,len);
  line = readline(sock,&tsd);

  if(line && cf_strncmp(line,"200 Ok",6) == 0) {
    free(line);

    len = snprintf(buff,128,"GET THREADLIST invisible=%d\n",del);
    writen(sock,buff,len);
    line = readline(sock,&tsd);
  }
  #else
  ptr = shm_ptr;
  ptr1 = ptr + sizeof(time_t);
  #endif
  /* }}} */

  /* {{{ Check if request was ok. If not, send error message. */
  #ifndef CF_SHARED_MEM
  if(!line || cf_strcmp(line,"200 Ok\n")) {
    printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);

    if(line) {
      ret = snprintf(buff,128,"E_FO_%d",atoi(line));
      cf_error_message(buff,NULL);
      free(line);
    }
    else cf_error_message("E_NO_THREADLIST",NULL);
  }
  #else
  if(!ptr) {
    printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
    cf_error_message("E_NO_CONN",NULL,strerror(errno));
  }
  #endif
  /* }}} */

  /*
   * Request of shm segment/threadlist wen't through,
   * go on with work
   */
  else {
    fbase    = cf_cfg_get_first_value(&fo_default_conf,forum_name,UserName ? "UBaseURL" : "BaseURL");

    #ifndef CF_SHARED_MEM
    free(line);
    #endif

    thread.tid      = 0;
    thread.messages = NULL;
    thread.last     = NULL;
    thread.msg_len  = 0;

    /* ok, seems to be all right, send headers */
    switch(mode) {
      case CF_MODE_ATOM:
        printf("Content-Type: application/atom+xml; charset=UTF-8\015\012\015\012");
        atom_head(&cnt,NULL);
        break;

      default:
        printf("Content-Type: application/rss+xml; charset=UTF-8\015\012\015\012");
        rss_head(&cnt,NULL);
        break;
    }

    #ifdef CF_NO_SORTING

    #ifndef CF_SHARED_MEM
    while(cf_get_next_thread_through_sock(sock,&tsd,&thread) == 0)
    #else
    while((ptr1 = cf_get_next_thread_through_shm(ptr1,&thread)) != NULL)
    #endif
    {
      if(thread.messages) {
        if((thread.messages->invisible == 0 && thread.messages->may_show) || del == CF_KEEP_DELETED) {
          /* first: run VIEW_HANDLER handlers in pre-mode */
          ret = cf_run_view_handlers(&thread,head,CF_MODE_THREADLIST|CF_MODE_XML|CF_MODE_PRE);

          if(ret == FLT_OK || ret == FLT_DECLINE || del == CF_KEEP_DELETED) {
            /* run list handlers */
            for(msg=thread.messages;msg;msg=msg->next) cf_run_view_list_handlers(msg,head,thread.tid,CF_MODE_THREADLIST|CF_MODE_XML);

            /* after that, run VIEW_HANDLER handlers in post-mode */
            ret = cf_run_view_handlers(&thread,head,CF_MODE_THREADLIST|CF_MODE_POST|CF_MODE_XML);

            /* if thread is still visible print it out */
            if(ret == FLT_OK || ret == FLT_DECLINE || del == CF_KEEP_DELETED) {
              switch(mode) {
                case CF_MODE_ATOM:
                  gen_threadlist_atom(&thread,&cnt);
                  break;
                default:
                  gen_threadlist_rss(&thread,&cnt);
              }
            }
          }

          cf_cleanup_thread(&thread);
        }
      }
    }

    /* sorting algorithms are allowed */
    #else

    #ifdef CF_SHARED_MEM
    if(cf_get_threadlist(&threads,ptr1) == -1)
    #else
    if(cf_get_threadlist(&threads,sock,&tsd) == -1)
    #endif
    {
      if(*ErrorString) cf_error_message(ErrorString,NULL);
      else cf_error_message("E_NO_THREADLIST",NULL);

      return;
    }
    else {
      #ifdef CF_SHARED_MEM
      cf_run_sorting_handlers(head,ptr,&threads);
      #else
      cf_run_sorting_handlers(head,sock,&tsd,&threads);
      #endif

      for(i=0;i<threads.elements;++i) {
        threadp = cf_array_element_at(&threads,i);

        if((threadp->messages->invisible == 0 && threadp->messages->may_show) || del == CF_KEEP_DELETED) {
          /* first: run VIEW_HANDLER handlers in pre-mode */
          ret = cf_run_view_handlers(threadp,head,CF_MODE_THREADLIST|CF_MODE_PRE|CF_MODE_XML);

          if(ret == FLT_OK || ret == FLT_DECLINE || del == CF_KEEP_DELETED) {
            /* run list handlers */
            for(msg=threadp->messages;msg;msg=msg->next) cf_run_view_list_handlers(msg,head,threadp->tid,CF_MODE_THREADLIST|CF_MODE_XML);

            /* after that, run VIEW_HANDLER handlers in post-mode */
            ret = cf_run_view_handlers(threadp,head,CF_MODE_THREADLIST|CF_MODE_POST|CF_MODE_XML);

            /* if thread is still visible print it out */
            if(ret == FLT_OK || ret == FLT_DECLINE || del == CF_KEEP_DELETED) {
              switch(mode) {
                case CF_MODE_ATOM:
                  gen_threadlist_atom(threadp,&cnt);
                  break;
                default:
                  gen_threadlist_rss(threadp,&cnt);
              }
            }
          }
        }
      }

      cf_array_destroy(&threads);
    }
    #endif

    /* {{{ write content */
    switch(mode) {
      case CF_MODE_ATOM:
        atom_bottom(&cnt);
        break;
      default:
        rss_bottom(&cnt);
    }

    fwrite(cnt.content,1,cnt.len,stdout);
    cf_str_cleanup(&cnt);
    /* }}} */

    #ifndef CF_SHARED_MEM
    if(*ErrorString) {
      cf_error_message(ErrorString,NULL);
      return;
    }
    #else
    if(ptr1 == NULL && *ErrorString) {
      cf_error_message(ErrorString,NULL);
      return;
    }
    #endif
  }
}
/* }}} */

/**
 * Dummy function, for ignoring unknown directives
 */
int ignre(cf_configfile_t *cfile,const u_char *context,u_char *name,u_char **args,size_t len) {
  return 0;
}


/* {{{ signal handler for bad signals */
void sighandler(int segnum) {
  FILE *fd = fopen(PROTOCOL_FILE,"a");
  u_char buff[10],*uname = NULL,*qs = NULL;

  if(fd) {
    qs    = getenv("QUERY_STRING");
    if(GlobalValues) uname = cf_hash_get(GlobalValues,"UserName",8);

    switch(segnum) {
      case SIGSEGV:
        snprintf(buff,10,"SIGSEGV");
        break;
      case SIGILL:
        snprintf(buff,10,"SIGILL");
        break;
      case SIGFPE:
        snprintf(buff,10,"SIGFPE");
        break;
      case SIGBUS:
        snprintf(buff,10,"SIGBUS");
        break;
      default:
        snprintf(buff,10,"UKNOWN");
        break;
    }

    fprintf(fd,"fo_feeds: Got signal %s!\nUsername: %s\nQuery-String: %s\n----\n",buff,uname?uname:(u_char *)"(null)",qs?qs:(u_char *)"(null)");
    fclose(fd);
  }

  exit(0);
}
/* }}} */

/**
 * The main function of the forum viewer. No command line switches used.
 * \param argc The argument count
 * \param argv The argument vector
 * \param env The environment vector
 * \return EXIT_SUCCESS on success, EXIT_FAILURE on error
 */
int main(int argc,char *argv[],char *env[]) {
  /* {{{ variables */
  #ifndef CF_SHARED_MEM
  int sock;
  #else
  void *sock;
  #endif

  static const u_char *wanted[] = {
    "fo_default", "fo_view", "fo_feeds"
  };

  int ret;
  u_char  *ucfg,*UserName,*fname;
  cf_array_t *cfgfiles;
  cf_hash_t *head;
  cf_configfile_t conf,dconf,feedsconf;
  cf_name_value_t *cs = NULL;
  cf_name_value_t *pt;
  u_char *forum_name = NULL;

  u_int64_t tid = 0;

  cf_readmode_t rm_infos;

  cf_string_t *t = NULL;
  /* }}} */

  /* {{{ set signal handler for SIGSEGV (for error reporting) */
  signal(SIGSEGV,sighandler);
  signal(SIGILL,sighandler);
  signal(SIGFPE,sighandler);
  signal(SIGBUS,sighandler);
  /* }}} */

  /* {{{ initialization */
  if((cfgfiles = cf_get_conf_file(wanted,3)) == NULL) {
    fprintf(stderr,"Could not find configuration files...\n");
    return EXIT_FAILURE;
  }

  cf_cfg_init();
  init_modules();
  cf_init();
  cf_htmllib_init();

  #ifndef CF_SHARED_MEM
  sock = 0;
  #else
  sock = NULL;
  #endif

  ret  = FLT_OK;
  /* }}} */

  /* {{{ read configuration */
  fname = *((u_char **)cf_array_element_at(cfgfiles,0));
  cf_cfg_init_file(&dconf,fname);
  free(fname);

  fname = *((u_char **)cf_array_element_at(cfgfiles,1));
  cf_cfg_init_file(&conf,fname);
  free(fname);

  fname = *((u_char **)cf_array_element_at(cfgfiles,2));
  cf_cfg_init_file(&feedsconf,fname);
  free(fname);

  cf_cfg_register_options(&dconf,default_options);
  cf_cfg_register_options(&conf,fo_view_options);
  cf_cfg_register_options(&feedsconf,fo_feeds_options);

  if(cf_read_config(&dconf,NULL,CF_CFG_MODE_CONFIG) != 0 || cf_read_config(&conf,NULL,CF_CFG_MODE_CONFIG) != 0 || cf_read_config(&feedsconf,NULL,CF_CFG_MODE_CONFIG) != 0) {
    fprintf(stderr,"config file error!\n");

    cf_cfg_cleanup_file(&conf);
    cf_cfg_cleanup_file(&dconf);
    cf_cfg_cleanup_file(&feedsconf);

    return EXIT_FAILURE;
  }
  /* }}} */

  /* {{{ ensure that CF_FORUM_NAME is set and we have got a context in every file */
  if((forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10)) == NULL) {
    fprintf(stderr,"Could not get forum name!");

    cf_cfg_cleanup_file(&conf);
    cf_cfg_cleanup_file(&dconf);

    cf_cfg_destroy();
    cf_fini();

    return EXIT_FAILURE;
  }

  if(cf_cfg_get_first_value(&fo_default_conf,forum_name,"ThreadIndexFile") == NULL) {
    fprintf(stderr,"Have no context for forum %s in default configuration file!\n",forum_name);

    cf_cfg_cleanup_file(&conf);
    cf_cfg_cleanup_file(&dconf);

    cf_cfg_destroy();
    cf_fini();

    return EXIT_FAILURE;
  }

  if(cf_cfg_get_first_value(&fo_view_conf,forum_name,"ParamType") == NULL) {
    fprintf(stderr,"Have no context for forum %s in fo_view configuration file!\n",forum_name);

    cf_cfg_cleanup_file(&conf);
    cf_cfg_cleanup_file(&dconf);

    cf_cfg_destroy();
    cf_fini();

    return EXIT_FAILURE;
  }
  /* }}} */

  pt = cf_cfg_get_first_value(&fo_view_conf,forum_name,"ParamType");
  head = cf_cgi_new();
  if(*pt->values[0] == 'P') cf_cgi_parse_path_info_nv(head);

  /* first action: authorization modules */
  ret = cf_run_auth_handlers(head);

  /* {{{ read user configuration */
  if(ret != FLT_EXIT && (UserName = cf_hash_get(GlobalValues,"UserName",8)) != NULL) {
    /* get user config */
    ucfg = cf_get_uconf_name(UserName);
    if(ucfg) {
      free(conf.filename);
      conf.filename = ucfg;

      if(cf_read_config(&conf,ignre,CF_CFG_MODE_USER) != 0) {
        fprintf(stderr,"config file error!\n");

        cf_cfg_cleanup_file(&conf);
        cf_cfg_cleanup_file(&dconf);

        cf_cfg_destroy();
        cf_fini();

        return EXIT_FAILURE;
      }
    }
  }
  /* }}} */

  /* run init handlers */
  if(ret != FLT_EXIT) ret = cf_run_init_handlers(head);

  cs = cf_cfg_get_first_value(&fo_default_conf,forum_name,"ExternCharset");

  /* {{{ get readmode information */
  if(ret != FLT_EXIT) {
    memset(&rm_infos,0,sizeof(rm_infos));
    if((ret = cf_run_readmode_collectors(head,&fo_view_conf,&rm_infos)) != FLT_OK) {
      printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
      fprintf(stderr,"cf_run_readmode_collectors() returned %d!\n",ret);
      cf_error_message("E_CONFIG_ERR",NULL);
      ret = FLT_EXIT;
    }
    else cf_hash_set(GlobalValues,"RM",2,&rm_infos,sizeof(rm_infos));
  }
  /* }}} */

  if(ret != FLT_EXIT) {
    /* {{{ now, we need a socket connection/shared mem pointer */
    #ifndef CF_SHARED_MEM
    if((sock = cf_socket_setup()) < 0) {
      printf("Content-Type: text/html; charset=%s\015\012Status: 500 Internal Server Error\015\012\015\012",cs?cs->values[0]?cs->values[0]:(u_char *)"UTF-8":(u_char *)"UTF-8");
      cf_error_message("E_NO_SOCK",NULL,strerror(errno));
      exit(0);
    }
    #else
    if((sock = cf_get_shm_ptr()) == NULL) {
      printf("Content-Type: text/html; charset=%s\015\012Status: 500 Internal Server Error\015\012\015\012",cs?cs->values[0]?cs->values[0]:(u_char *)"UTF-8":(u_char *)"UTF-8");
      cf_error_message("E_NO_CONN",NULL,strerror(errno));
      exit(0);
    }
    #endif
    /* }}} */

    /* run connect init handlers */
    ret = cf_run_connect_init_handlers(head,sock);

    if(ret != FLT_EXIT) {
      /* after that, look for m= and t= */
      if(head) t = cf_cgi_get(head,"t");
      if(t) tid = cf_str_to_uint64(t->content);

      if(tid)   show_thread(head,sock,tid);
      else      show_threadlist(sock,head);
    }

    #ifndef CF_SHARED_MEM
    writen(sock,"QUIT\n",5);
    close(sock);
    #endif
  }

  /* cleanup source */
  cf_cfg_cleanup_file(&dconf);
  cf_cfg_cleanup_file(&conf);
  cf_cfg_cleanup_file(&feedsconf);

  cf_array_destroy(cfgfiles);
  free(cfgfiles);

  cf_cleanup_modules(Modules);
  cf_fini();
  cf_cfg_destroy();

  if(head) cf_hash_destroy(head);

  #ifdef CF_SHARED_MEM
  if(sock) shmdt((void *)sock);
  #endif

  return EXIT_SUCCESS;
}


/* eof */
