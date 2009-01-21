/**
 * \file fo_feeds.c
 * \author Christian Kruse, <cjk@wwwtech.de>
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

void rfc822_date(cf_cfg_config_t *cfg,cf_string_t *str,time_t date) {
  u_char buff[512];
  size_t len;
  struct tm *tm;

  cf_cfg_config_value_t *lc = cf_cfg_get_value(cfg,"FF:DateLocaleEn");

  setlocale(LC_TIME,lc->sval);

  if((tm = localtime(&date)) == NULL) return;

  len = strftime(buff,512,"%a, %d %b %Y %H:%M:%S %z",tm);
  cf_str_chars_append(str,buff,len);
}
/* }}} */

/* {{{ description generation */
void gen_description(cf_string_t *str,const u_char *descr,cf_cl_thread_t *thread) {
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

/* {{{ replace problematic ]]> in CDATA sections with ]]]><![CDATA[]> */
void str_str_cdata_append(cf_string_t *dest, cf_string_t *src) {
  // don't be binary-safe! xml doesn't allow 0-bytes anyway
  u_char *ptr = src->content;
  u_char *found;

  while((found = strstr(ptr,"]]>")) != NULL) {
    cf_str_chars_append(dest,ptr,(size_t)(found-ptr));
    cf_str_chars_append(dest,"]]]><![CDATA[]>",15);
    ptr = found + 3;
  }

  cf_str_cstr_append(dest,ptr);
}
/* }}} */

/* {{{ atom_ and rss_head */
void atom_head(cf_cfg_config_t *cfg,cf_string_t *str,cf_cl_thread_t *thread) {
  u_char *tmp = NULL,*tmp1 = NULL,*tmp2 = NULL,*uname = cf_hash_get(GlobalValues,"UserName",8);

  int authed = uname ? 1 : 0;

  cf_cfg_config_value_t *atom_title = cf_cfg_get_value(cfg,"FF:AtomTitle"),
    *atom_tgline = cf_cfg_get_value(cfg,"FF:AtomTagline"),
    *atom_lang = cf_cfg_get_value(cfg,"FF:FeedLang"),
    *atom_uri = cf_cfg_get_value(cfg,thread?"FF:AtomUriThread":"FF:AtomUri"),
    *atom_id = cf_cfg_get_value(cfg,"FF:AtomId"),
    *burl = cf_cfg_get_value(cfg,uname ? "UDF:BaseURL":"DF:BaseURL");

  cf_readmode_t *rm = cf_hash_get(GlobalValues,"RM",2);

  time_t t = time(NULL);

  if(thread) {
    tmp = cf_get_link(rm->posting_uri[authed],thread->tid,thread->messages->mid);
    tmp1 = htmlentities(tmp,0);
  }

  cf_str_chars_append(str,"<?xml version=\"1.0\"?>\n" \
    "<feed xmlns=\"http://www.w3.org/2005/Atom",
    62
  );
  if(atom_lang) {
    cf_str_chars_append(str,"\" xml:lang=\"",12);
    cf_str_chars_append(str,atom_lang->sval,strlen(atom_lang->sval));
  }
  cf_str_chars_append(str,"\">\n",3);

  cf_str_chars_append(str,"<title>",7);
  if(thread) {
    cf_str_chars_append(str,"<![CDATA[",9);
    cf_str_str_append(str,&thread->messages->subject);
    cf_str_chars_append(str,"]]>",3);
  }
  else cf_str_chars_append(str,atom_title->sval,strlen(atom_title->sval));
  cf_str_chars_append(str,"</title>",8);

  if(atom_tgline) {
    cf_str_chars_append(str,"<subtitle>",10);
    cf_str_chars_append(str,atom_tgline->sval,strlen(atom_tgline->sval));
    cf_str_chars_append(str,"</subtitle>",11);
  }

  cf_str_chars_append(str,"<id>",4);
  cf_str_cstr_append(str,atom_id->sval);
  cf_str_chars_append(str,"</id>",5);

  cf_str_chars_append(str,"<link rel=\"self\" href=\"",23);
  if(thread) tmp2 = cf_get_link(atom_uri->sval,thread->tid,thread->messages->mid);
  else tmp2 = cf_get_link(atom_uri->sval,0,0);
  cf_str_chars_append(str,tmp2,strlen(tmp2));
  free(tmp2);
  cf_str_chars_append(str,"\"/>",3);

  cf_str_chars_append(str,"<link rel=\"alternate\" type=\"text/html\" href=\"",45);
  if(thread) cf_str_chars_append(str,tmp1,strlen(tmp1));
  else cf_str_chars_append(str,burl->sval,strlen(burl->sval));
  cf_str_chars_append(str,"\"/>",3);

  cf_str_chars_append(str,"<updated>",9);
  if(thread) w3c_datetime(str,thread->newest->date);
  else w3c_datetime(str,t);
  cf_str_chars_append(str,"</updated>",10);

  cf_str_chars_append(str,"<generator>Classic Forum V.",27);
  cf_str_chars_append(str,CF_VERSION,strlen(CF_VERSION));
  cf_str_chars_append(str,"</generator>",12);

  if(thread) {
    cf_str_chars_append(str,"<author>",8);

    cf_str_chars_append(str,"<name><![CDATA[",15);
    cf_str_str_append(str,&thread->messages->author);
    cf_str_chars_append(str,"]]></name>",10);

    if(thread->messages->hp.len) {
      cf_str_chars_append(str,"<uri><![CDATA[",14);
      cf_str_str_append(str,&thread->messages->hp);
      cf_str_chars_append(str,"]]></uri>",9);
    }

    if(thread->messages->email.len) {
      cf_str_chars_append(str,"<email><![CDATA[",16);
      cf_str_str_append(str,&thread->messages->email);
      cf_str_chars_append(str,"]]></email>",11);
    }

    cf_str_chars_append(str,"</author>",9);
  }

}

void rss_head(cf_cfg_config_t *cfg,cf_string_t *str,cf_cl_thread_t *thread) {
  u_char *tmp;
  int authed = cf_hash_get(GlobalValues,"UserName",8) != NULL;

  cf_cfg_config_value_t *rss_title = cf_cfg_get_value(cfg,"FF:RSSTitle"),
    *rss_descr = cf_cfg_get_value(cfg,thread ? "FF:RSSDescriptionThread" : "FF:RSSDescription"),
    *rss_copy  = cf_cfg_get_value(cfg,"FF:RSSCopyright"),
    *rss_lang  = cf_cfg_get_value(cfg,"FF:FeedLang"),
    *rss_wbm   = cf_cfg_get_value(cfg,"FF:RSSWebMaster"),
    *rss_cat   = cf_cfg_get_value(cfg,"FF:RSSCategory"),
    *burl      = cf_cfg_get_value(cfg,"DF:BaseURL"),
    *purl;

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
  else cf_str_chars_append(str,rss_title->sval,strlen(rss_title->sval));
  cf_str_chars_append(str,"</title>",8);

  cf_str_chars_append(str,"<link>",6);
  if(thread) {
    purl = cf_cfg_get_value(cfg,"DF:PostingURL");
    tmp = cf_get_link(purl->avals[authed].sval,thread->tid,thread->messages->mid);
    cf_str_chars_append(str,"<![CDATA[",9);
    cf_str_chars_append(str,tmp,strlen(tmp));
    cf_str_chars_append(str,"]]>",3);
  }
  else cf_str_chars_append(str,burl->avals[authed].sval,strlen(burl->avals[authed].sval));
  cf_str_chars_append(str,"</link>",7);

  cf_str_chars_append(str,"<description><![CDATA[",22);
  if(thread) gen_description(str,rss_descr->sval,thread);
  else cf_str_chars_append(str,rss_descr->sval,strlen(rss_descr->sval));
  cf_str_chars_append(str,"]]></description>",17);

  if(rss_copy) {
    cf_str_chars_append(str,"<copyright>",11);
    cf_str_chars_append(str,rss_copy->sval,strlen(rss_copy->sval));
    cf_str_chars_append(str,"</copyright>",12);
  }

  if(rss_lang) {
    cf_str_chars_append(str,"<language>",10);
    cf_str_chars_append(str,rss_lang->sval,strlen(rss_lang->sval));
    cf_str_chars_append(str,"</language>",11);
  }

  if(rss_wbm) {
    cf_str_chars_append(str,"<webMaster>",11);
    cf_str_chars_append(str,rss_wbm->sval,strlen(rss_wbm->sval));
    cf_str_chars_append(str,"</webMaster>",12);
  }

  if(rss_cat) {
    cf_str_chars_append(str,"<category>",10);
    cf_str_chars_append(str,rss_cat->sval,strlen(rss_cat->sval));
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
void atom_thread(cf_cfg_config_t *cfg,cf_string_t *str,cf_cl_thread_t *thread,cf_hash_t *head) {
  cf_message_t *msg;

  u_char *tmp,*tmp1,*uname = cf_hash_get(GlobalValues,"UserName",8);
  size_t len1;

  cf_string_t tmpstr;

  cf_cfg_config_value_t *qchars = cf_cfg_get_value(cfg,"DF:QuotingChars"),
    *ms     = cf_cfg_get_value(cfg,"MaxSigLines"),
    *ss     = cf_cfg_get_value(cfg,"ShowSig");

  cf_readmode_t *rm = cf_hash_get(GlobalValues,"RM",2);

  /* {{{ run handlers in pre and post mode */
  cf_run_view_handlers(cfg,thread,head,CF_MODE_THREADVIEW|CF_MODE_PRE|CF_MODE_XML);
  for(msg=thread->messages;msg;msg=msg->next) cf_run_view_list_handlers(cfg,msg,head,thread->tid,CF_MODE_THREADVIEW|CF_MODE_XML);
  cf_run_view_handlers(cfg,thread,head,CF_MODE_THREADVIEW|CF_MODE_POST|CF_MODE_XML);
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
        cf_str_chars_append(str,"<uri><![CDATA[",14);
        cf_str_str_append(str,&msg->hp);
        cf_str_chars_append(str,"]]></uri>",9);
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

      cf_str_chars_append(str,"<updated>",9);
      w3c_datetime(str,thread->newest->date);
      cf_str_chars_append(str,"</updated>",10);

      cf_str_chars_append(str,"<published>",11);
      w3c_datetime(str,thread->messages->date);
      cf_str_chars_append(str,"</published>",12);

      /* {{{ content */
      cf_str_chars_append(str,"<content type=\"html\"><![CDATA[",30);
      cf_str_init(&tmpstr);
      cf_msg_to_html(
        cfg,
        thread,
        msg->content.content,
        &tmpstr,
        NULL,
        qchars->sval,
        ms ? ms->ival : -1,
        ss ? cf_strcmp(ss->sval,"yes") == 0 : 0
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
void rss_thread(cf_cfg_config_t *cfg,cf_string_t *str,cf_cl_thread_t *thread,cf_hash_t *head) {
  cf_message_t *msg;

  u_char *tmp,*tmp1;
  u_char *uname = cf_hash_get(GlobalValues,"UserName",8);
  size_t len;
  int authed = uname ? 1 : 0;

  cf_string_t tmpstr;

  cf_cfg_config_value_t *burl   = cf_cfg_get_value(cfg,"DF:PostingURL"),
    *qchars = cf_cfg_get_value(cfg,"DF:QuotingChars"),
    *ms     = cf_cfg_get_value(cfg,"MaxSigLines"),
    *ss     = cf_cfg_get_value(cfg,"ShowSig");

  cf_readmode_t *rm = cf_hash_get(GlobalValues,"RM",2);

  /* {{{ run handlers in pre and post mode */
  cf_run_view_handlers(cfg,thread,head,CF_MODE_THREADVIEW|CF_MODE_PRE|CF_MODE_XML);
  for(msg=thread->messages;msg;msg=msg->next) cf_run_view_list_handlers(cfg,msg,head,thread->tid,CF_MODE_THREADVIEW|CF_MODE_XML);
  cf_run_view_handlers(cfg,thread,head,CF_MODE_THREADVIEW|CF_MODE_POST|CF_MODE_XML);
  /* }}} */

  for(msg=thread->messages;msg;msg=msg->next) {
    if(msg->may_show && msg->invisible == 0) {
      tmp  = cf_get_link(rm->posting_uri[authed],thread->tid,msg->mid);
      tmp1 = cf_get_link(burl->avals[authed].sval,thread->tid,msg->mid);

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
      rfc822_date(cfg,str,msg->date);
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
      cf_str_chars_append(str,"<description><![CDATA[",22);

      cf_str_init(&tmpstr);
      cf_msg_to_html(
        cfg,
        thread,
        msg->content.content,
        &tmpstr,
        NULL,
        qchars->sval,
        ms ? ms->ival : -1,
        ss ? cf_strcmp(ss->sval,"yes") == 0 : 0
      );
      cf_str_str_append(str,&tmpstr);
      cf_str_cleanup(&tmpstr);

      cf_str_chars_append(str,"]]></description>",17);
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
void show_thread(cf_cfg_config_t *cfg,cf_hash_t *head,int sock,u_int64_t tid)
#else
void show_thread(cf_cfg_config_t *cfg,cf_hash_t *head,void *sock,u_int64_t tid)
#endif
{
  cf_cl_thread_t thread;

  #ifndef CF_SHARED_MEM
  rline_t tsd;
  #else
  cf_cfg_config_value_t *shminf = cf_cfg_get_value(cfg,"DF:SharedMemIds");
  int shmids[3] = { shminf->avals[0].ival,shminf->avals[1].ival,shminf->avals[2].ival };
  #endif

  cf_string_t *tmp;

  cf_cfg_config_value_t *cs = cf_cfg_get_value(cfg,"DF:ExternCharset");

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
  if(cf_get_message_through_shm(shmids,shm_ptr,&thread,tid,0,del) == -1)
  #endif
  {
    if(cf_strcmp(ErrorString,"E_FO_404") == 0) {
      if(cf_run_404_handlers(cfg,head,tid,0) != FLT_EXIT) {
        printf("Status: 404 Not Found\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->sval);
        cf_error_message(cfg,ErrorString,NULL);
      }
    }
    else {
      printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->sval);
      cf_error_message(cfg,ErrorString,NULL);
    }
    return;
  }
  /* }}} */

  #ifndef CF_NO_SORTING
  #ifdef CF_SHARED_MEM
  cf_run_thread_sorting_handlers(cfg,head,shm_ptr,&thread);
  #else
  cf_run_thread_sorting_handlers(cfg,head,sock,&tsd,&thread);
  #endif
  #endif

  /* ok, seems to be all right, send headers */
  switch(mode) {
    case CF_MODE_ATOM:
      printf("Content-Type: application/atom+xml; charset=UTF-8\015\012\015\012");
      atom_head(cfg,&cnt,&thread);
      atom_thread(cfg,&cnt,&thread,head);
      atom_bottom(&cnt);
      break;

    default:
      printf("Content-Type: application/rss+xml; charset=UTF-8\015\012\015\012");
      rss_head(cfg,&cnt,&thread);
      rss_thread(cfg,&cnt,&thread,head);
      rss_bottom(&cnt);
      break;
  }

  cf_cleanup_thread(&thread);

  fwrite(cnt.content,1,cnt.len,stdout);
  cf_str_cleanup(&cnt);
}
/* }}} */

/* {{{ threadlist atom */
void gen_threadlist_atom(cf_cfg_config_t *cfg,cf_cl_thread_t *thread,cf_string_t *str) {
  u_char *tmp,*tmp1,*tmp2,*tmp3,*uname = cf_hash_get(GlobalValues,"UserName",8);
  size_t len1,len2;
  int authed = uname ? 1 : 0;

  cf_readmode_t *rm = cf_hash_get(GlobalValues,"RM",2);

  cf_cfg_config_value_t *burl = cf_cfg_get_value(cfg,"DF:PostingURL"),
    *qchars = cf_cfg_get_value(cfg,"DF:QuotingChars"),
    *ms     = cf_cfg_get_value(cfg,"MaxSigLines"),
    *ss     = cf_cfg_get_value(cfg,"ShowSig");

  cf_string_t tmpstr;

  tmp = cf_get_link(rm->posting_uri[authed],thread->tid,thread->messages->mid);
  tmp1 = htmlentities(tmp,0);
  len1 = strlen(tmp1);

  tmp2 = cf_get_link(burl->avals[authed].sval,thread->tid,thread->messages->mid);
  tmp3 = htmlentities(tmp2,0);
  len2 = strlen(tmp3);

  cf_str_chars_append(str,"<entry>",7);

  /* {{{ author of this thread */
  cf_str_chars_append(str,"<author>",8);

  cf_str_chars_append(str,"<name><![CDATA[",15);
  cf_str_str_append(str,&thread->messages->author);
  cf_str_chars_append(str,"]]></name>",10);

  if(thread->messages->hp.len) {
    cf_str_chars_append(str,"<uri><![CDATA[",14);
    cf_str_str_append(str,&thread->messages->hp);
    cf_str_chars_append(str,"]]></uri>",9);
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

  cf_str_chars_append(str,"<updated>",10);
  w3c_datetime(str,thread->newest->date);
  cf_str_chars_append(str,"</updated>",11);

  cf_str_chars_append(str,"<published>",11);
  w3c_datetime(str,thread->messages->date);
  cf_str_chars_append(str,"</published>",12);

  /* {{{ content */
  cf_str_chars_append(str,"<content type=\"html\"><![CDATA[",30);
  cf_str_init(&tmpstr);
  cf_msg_to_html(
    cfg,
    thread,
    thread->messages->content.content,
    &tmpstr,
    NULL,
    qchars->sval,
    ms ? ms->ival : -1,
    ss ? cf_strcmp(ss->sval,"yes") == 0 : 0
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
void gen_threadlist_rss(cf_cfg_config_t *cfg,cf_cl_thread_t *thread,cf_string_t *str) {
  u_char *tmp,*tmp1,*uname = cf_hash_get(GlobalValues,"UserName",8);
  size_t len,len1;

  cf_readmode_t *rm = cf_hash_get(GlobalValues,"RM",2);

  cf_string_t tmpstr;
  int authed = uname ? 1 : 0;

  cf_cfg_config_value_t *burl   = cf_cfg_get_value(cfg,"DF:PostingURL"),
    *qchars = cf_cfg_get_value(cfg,"DF:QuotingChars"),
    *ms     = cf_cfg_get_value(cfg,"MaxSigLines"),
    *ss     = cf_cfg_get_value(cfg,"ShowSig");

  tmp = cf_get_link(burl->avals[authed].sval,thread->tid,thread->messages->mid);
  len = strlen(tmp);

  tmp1 = cf_get_link(rm->posting_uri[authed],thread->tid,thread->messages->mid);
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
  rfc822_date(cfg,str,thread->messages->date);
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
  cf_str_chars_append(str,"<description><![CDATA[",22);

  cf_str_init(&tmpstr);
  cf_msg_to_html(
    cfg,
    thread,
    thread->messages->content.content,
    &tmpstr,
    NULL,
    qchars->sval,
    ms ? ms->ival : -1,
    ss ? cf_strcmp(ss->sval,"yes") == 0 : 0
  );
  cf_str_str_append(str,&tmpstr);
  cf_str_cleanup(&tmpstr);

  cf_str_chars_append(str,"]]></description>",17);
  /* }}} */

  cf_str_chars_append(str,"</item>\n",8);

  free(tmp);
  free(tmp1);
}
/* }}} */

/* {{{ show_threadlist */

#ifndef CF_SHARED_MEM
void show_threadlist(cf_cfg_config_t *cfg,int sock,cf_hash_t *head)
#else
void show_threadlist(cf_cfg_config_t *cfg,void *shm_ptr,cf_hash_t *head)
#endif
{
  int ret;
  #ifndef CF_SHARED_MEM
  rline_t tsd;
  u_char *line,*forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  size_t len;
  u_char buff[512];
  #else
  cf_cfg_config_value_t *shminf = cf_cfg_get_value(cfg,"DF:SharedMemIds");
  int shmids[3] = { shminf->avals[0].ival,shminf->avals[1].ival,shminf->avals[2].ival };
  void *ptr,*ptr1;
  #endif

  cf_cfg_config_value_t *cs = cf_cfg_get_value(cfg,"DF:ExternCharset");

  cf_cl_thread_t thread;
  cf_message_t *msg;
  int del = cf_hash_get(GlobalValues,"ShowInvisible",13) == NULL ? CF_KILL_DELETED : CF_KEEP_DELETED,mode = CF_MODE_RSS;

  cf_string_t cnt,*tmp;

  #ifndef CF_NO_SORTING
  cf_array_t threads;
  cf_cl_thread_t *threadp
  size_t i;
  #endif

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
    printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->sval);

    if(line) {
      ret = snprintf(buff,128,"E_FO_%d",atoi(line));
      cf_error_message(cfg,buff,NULL);
      free(line);
    }
    else cf_error_message(cfg,"E_NO_THREADLIST",NULL);
  }
  #else
  if(!ptr) {
    printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->sval);
    cf_error_message(cfg,"E_NO_CONN",NULL,strerror(errno));
  }
  #endif
  /* }}} */

  /*
   * Request of shm segment/threadlist wen't through,
   * go on with work
   */
  else {
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
        atom_head(cfg,&cnt,NULL);
        break;

      default:
        printf("Content-Type: application/rss+xml; charset=UTF-8\015\012\015\012");
        rss_head(cfg,&cnt,NULL);
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
          ret = cf_run_view_handlers(cfg,&thread,head,CF_MODE_THREADLIST|CF_MODE_XML|CF_MODE_PRE);

          if(ret == FLT_OK || ret == FLT_DECLINE || del == CF_KEEP_DELETED) {
            /* run list handlers */
            for(msg=thread.messages;msg;msg=msg->next) cf_run_view_list_handlers(cfg,msg,head,thread.tid,CF_MODE_THREADLIST|CF_MODE_XML);

            /* after that, run VIEW_HANDLER handlers in post-mode */
            ret = cf_run_view_handlers(cfg,&thread,head,CF_MODE_THREADLIST|CF_MODE_POST|CF_MODE_XML);

            /* if thread is still visible print it out */
            if(ret == FLT_OK || ret == FLT_DECLINE || del == CF_KEEP_DELETED) {
              switch(mode) {
                case CF_MODE_ATOM:
                  gen_threadlist_atom(cfg,&thread,&cnt);
                  break;
                default:
                  gen_threadlist_rss(cfg,&thread,&cnt);
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
    if(cf_get_threadlist(&threads,shmids,ptr1) == -1)
    #else
    if(cf_get_threadlist(&threads,sock,&tsd) == -1)
    #endif
    {
      if(*ErrorString) cf_error_message(cfg,ErrorString,NULL);
      else cf_error_message(cfg,"E_NO_THREADLIST",NULL);

      return;
    }
    else {
      #ifdef CF_SHARED_MEM
      cf_run_sorting_handlers(cfg,head,ptr,&threads);
      #else
      cf_run_sorting_handlers(cfg,head,sock,&tsd,&threads);
      #endif

      for(i=0;i<threads.elements;++i) {
        threadp = cf_array_element_at(&threads,i);

        if((threadp->messages->invisible == 0 && threadp->messages->may_show) || del == CF_KEEP_DELETED) {
          /* first: run VIEW_HANDLER handlers in pre-mode */
          ret = cf_run_view_handlers(cfg,threadp,head,CF_MODE_THREADLIST|CF_MODE_PRE|CF_MODE_XML);

          if(ret == FLT_OK || ret == FLT_DECLINE || del == CF_KEEP_DELETED) {
            /* run list handlers */
            for(msg=threadp->messages;msg;msg=msg->next) cf_run_view_list_handlers(cfg,msg,head,threadp->tid,CF_MODE_THREADLIST|CF_MODE_XML);

            /* after that, run VIEW_HANDLER handlers in post-mode */
            ret = cf_run_view_handlers(cfg,threadp,head,CF_MODE_THREADLIST|CF_MODE_POST|CF_MODE_XML);

            /* if thread is still visible print it out */
            if(ret == FLT_OK || ret == FLT_DECLINE || del == CF_KEEP_DELETED) {
              switch(mode) {
                case CF_MODE_ATOM:
                  gen_threadlist_atom(cfg,threadp,&cnt);
                  break;
                default:
                  gen_threadlist_rss(cfg,threadp,&cnt);
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
      cf_error_message(cfg,ErrorString,NULL);
      return;
    }
    #else
    if(ptr1 == NULL && *ErrorString) {
      cf_error_message(cfg,ErrorString,NULL);
      return;
    }
    #endif
  }
}

/* }}} */

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
  #ifndef CF_SHARED_MEM
  int sock;
  cf_cfg_config_value_t *sockpath;
  #else
  void *sock;
  int shmids[3];
  cf_cfg_config_value_t *shminf;
  #endif

  static const u_char *wanted[] = {
    "fo_default", "fo_view", "fo_feeds"
  };

  int ret;
  u_char  *ucfg,*UserName,*forum_name;
  cf_hash_t *head;
  cf_cfg_config_value_t *cs = NULL,*uconfpath,*pt;

  u_int64_t tid = 0;

  cf_readmode_t rm_infos;

  cf_cfg_config_t cfg;

  cf_string_t *t = NULL;

  signal(SIGSEGV,sighandler);
  signal(SIGILL,sighandler);
  signal(SIGFPE,sighandler);
  signal(SIGBUS,sighandler);

  /* initialization */
  cf_init();
  cf_htmllib_init();

  if((forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10)) == NULL) {
    fprintf(stderr,"Could not get forum name!");
    return EXIT_FAILURE;
  }

  #ifndef CF_SHARED_MEM
  sock = 0;
  #else
  sock = NULL;
  #endif

  ret  = FLT_OK;

  /* {{{ read configuration */
  if(cf_cfg_get_conf(&cfg,wanted,3) != 0) {
    fprintf(stderr,"config file error!\n");
    return EXIT_FAILURE;
  }
  /* }}} */

  pt = cf_cfg_get_value(&cfg,"FV:ParamType");
  uconfpath = cf_cfg_get_value(&cfg,"DF:ConfigDirectory");

  head = cf_cgi_new();
  if(*pt->sval == 'P') cf_cgi_parse_path_info_nv(head);

  /* first action: authorization modules */
  ret = cf_run_auth_handlers(&cfg,head);

  /* {{{ read user configuration */
  if(ret != FLT_EXIT && (UserName = cf_hash_get(GlobalValues,"UserName",8)) != NULL) {
    /* get user config */
    if((ucfg = cf_get_uconf_name(uconfpath->sval,UserName)) != NULL) {
      if(cf_cfg_read_conffile(&cfg,ucfg) != 0) {
        fprintf(stderr,"config file error!\n");
        cf_fini();
        return EXIT_FAILURE;
      }
    }
  }
  /* }}} */

  /* run init handlers */
  if(ret != FLT_EXIT) ret = cf_run_init_handlers(&cfg,head);

  cs = cf_cfg_get_value(&cfg,"DF:ExternCharset");

  /* {{{ get readmode information */
  if(ret != FLT_EXIT) {
    memset(&rm_infos,0,sizeof(rm_infos));
    if((ret = cf_run_readmode_collectors(&cfg,head,&rm_infos)) != FLT_OK) {
      printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->sval);
      fprintf(stderr,"cf_run_readmode_collectors() returned %d!\n",ret);
      cf_error_message(&cfg,"E_CONFIG_ERR",NULL);
      ret = FLT_EXIT;
    }
    else cf_hash_set(GlobalValues,"RM",2,&rm_infos,sizeof(rm_infos));
  }
  /* }}} */

  if(ret != FLT_EXIT) {
    /* now, we need a socket connection/shared mem pointer */
    #ifndef CF_SHARED_MEM
    sockpath = cf_cfg_get_value(&cfg,"DF:SocketName");
    if((sock = cf_socket_setup(sockpath->sval)) < 0) {
      printf("Content-Type: text/html; charset=%s\015\012Status: 500 Internal Server Error\015\012\015\012",cs->sval);
      cf_error_message(&cfg,"E_NO_SOCK",NULL,strerror(errno));
      exit(0);
    }
    #else
    shminf = cf_cfg_get_value(&cfg,"DF:SharedMemIds");
    shmids[0] = shminf->avals[0].ival;
    shmids[1] = shminf->avals[1].ival;
    shmids[2] = shminf->avals[2].ival;
    if((sock = cf_get_shm_ptr(shmids)) == NULL) {
      printf("Content-Type: text/html; charset=%s\015\012Status: 500 Internal Server Error\015\012\015\012",cs->sval);
      cf_error_message(&cfg,"E_NO_CONN",NULL,strerror(errno));
      exit(0);
    }
    #endif

    /* run connect init handlers */
    ret = cf_run_connect_init_handlers(&cfg,head,sock);

    if(ret != FLT_EXIT) {
      /* after that, look for m= and t= */
      if(head) t = cf_cgi_get(head,"t");
      if(t) tid = cf_str_to_uint64(t->content);

      if(tid)   show_thread(&cfg,head,sock,tid);
      else      show_threadlist(&cfg,sock,head);
    }

    #ifndef CF_SHARED_MEM
    writen(sock,"QUIT\n",5);
    close(sock);
    #endif
  }

  /* cleanup source */
  cf_cfg_config_destroy(&cfg);

  cf_fini();

  if(head) cf_hash_destroy(head);

  #ifdef CF_SHARED_MEM
  if(sock) shmdt((void *)sock);
  #endif

  return EXIT_SUCCESS;
}


/* eof */
