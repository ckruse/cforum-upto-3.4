/**
 * \file flt_mailonpost.c
 * \author Christian Kruse, <cjk@wwwtech.de>
 * \brief Send a message to user if answer to a message has been sent
 *
 * This file is a plugin for fo_post. If user wants so, a message will be
 * if an answer to his message has been posted
 *
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
#include "cfconfig.h"
#include "defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include <sys/file.h>

#include <errno.h>

#include <time.h>
#include <db.h>

#include <libesmtp.h>

#include "readline.h"
#include "hashlib.h"
#include "utils.h"
#include "cfgcomp.h"
#include "cfcgi.h"
#include "template.h"
#include "clientlib.h"
#include "charconvert.h"
#include "htmllib.h"
#include "fo_post.h"
/* }}} */

static u_char *flt_mailonpost_from   = NULL;
static u_char *flt_mailonpost_host   = NULL;
static u_char *flt_mailonpost_dbname = NULL;
static u_char *flt_mailonpost_fn     = NULL;
static u_char *flt_mailonpost_rvrs   = NULL;
static u_char *flt_mailonpost_udb    = NULL;
static u_char *flt_mailonpost_uemail = NULL;
static u_char *flt_mailonpost_amail  = NULL;

struct s_smtp {
  cf_string_t *msg;
  size_t pos;
};

/* {{{ flt_mailonpost_create */
int flt_mailonpost_create(DB **db,u_char *path) {
  int ret,fd;

  if((ret = db_create(db,NULL,0)) != 0) {
    fprintf(stderr,"flt_mailonpost: db_create() error: %s\n",db_strerror(ret));
    return -1;
  }

  if((ret = (*db)->open(*db,NULL,path,NULL,DB_BTREE,DB_CREATE,0644)) != 0) {
    fprintf(stderr,"flt_mailonpost: db->open(%s) error: %s\n",path,db_strerror(ret));
    return -1;
  }

  if((ret = (*db)->fd(*db,&fd)) != 0) {
    fprintf(stderr,"flt_mailonpost: db->fd() error: %s\n",db_strerror(ret));
    return -1;
  }

  if((ret = flock(fd,LOCK_EX)) != 0) {
    fprintf(stderr,"flt_mailonpost: flock() error: %s\n",strerror(ret));
    return -1;
  }

  return 0;
}
/* }}} */

/* {{{ flt_mailonpost_destroy */
void flt_mailonpost_destroy(DB *db) {
  db->close(db,0);
}
/* }}} */

/* {{{ flt_mailonpost_init_handler */
int flt_mailonpost_init_handler(cf_hash_t *head,cf_configuration_t *dc,cf_configuration_t *vc) {
  u_char *email,buff[256],*user = cf_hash_get(GlobalValues,"UserName",8),**list = NULL;
  DB *db = NULL,*udb = NULL;
  DBT key,data;
  size_t n,i;
  u_int64_t tid = 0;
  int ret;
  cf_string_t str;
  cf_name_value_t *v = NULL;

  cf_string_t *val,*cgival;

  if(!head) return FLT_DECLINE;

  if((val = cf_cgi_get(head,"t")) == NULL) {
    if((val = cf_cgi_get(head,"fupto")) == NULL) return FLT_DECLINE;
  }

  tid = cf_str_to_uint64(val->content);

  if((val = cf_cgi_get(head,"mailonpost")) != NULL && (cf_strcmp(val->content,"yes") == 0 || cf_strcmp(val->content,"no") == 0) && tid != 0) {
    /* {{{ get email address; either from CGI parameter or from config file, depends */
    if((cgival = cf_cgi_get(head,"EMail")) == NULL) {
      if(user == NULL) return FLT_DECLINE;

      if(flt_mailonpost_uemail) email = flt_mailonpost_uemail;
      else {
        if((v = cf_cfg_get_first_value(vc,flt_mailonpost_fn,"EMail")) == NULL) return FLT_DECLINE;
        email = v->values[0];
      }
    }
    else email = cgival->content;
    /* }}} */

    if(flt_mailonpost_create(&db,flt_mailonpost_dbname) == -1) return FLT_DECLINE;
    if(flt_mailonpost_udb && flt_mailonpost_create(&udb,flt_mailonpost_udb) == -1) {
      flt_mailonpost_destroy(db);
      return FLT_DECLINE;
    }

    n = snprintf(buff,256,"t%"PRIu64,tid);

    memset(&key,0,sizeof(key));
    memset(&data,0,sizeof(data));

    cf_str_init(&str);

    key.data = buff;
    key.size = n;

    if((ret = db->get(db,NULL,&key,&data,0)) != 0) {
      if(ret != DB_NOTFOUND) {
        if(udb) flt_mailonpost_destroy(udb);
        flt_mailonpost_destroy(db);

        fprintf(stderr,"flt_mailonpost: db->get() error: %s\n",db_strerror(ret));
        return FLT_DECLINE;
      }
      else {
        if(cf_strcmp(val->content,"yes") == 0) {
          cf_str_chars_append(&str,email,strlen(email));

          data.data = "1";
          data.size = sizeof("1");

          if(udb && (ret = udb->put(udb,NULL,&key,&data,DB_NODUPDATA|DB_NOOVERWRITE)) != 0) {
            if(ret != DB_KEYEXIST) {
              flt_mailonpost_destroy(udb);
              flt_mailonpost_destroy(db);
              fprintf(stderr,"flt_mailonpost: db->put() error: %s\n",db_strerror(ret));
              return FLT_DECLINE;
            }
          }
        }
      }
    }
    else {
      cf_str_char_set(&str,data.data,data.size);

      if(cf_strcmp(val->content,"yes") == 0) {
        /* {{{ check if mail already exists in data entry */
        n = cf_split(str.content,"\x7F",&list);
        for(i=0,ret=0;i<n;++i) {
          if(ret == 0 && cf_strcmp(email,list[i]) == 0) ret = 1;
          free(list[i]);
        }

        free(list);
        if(ret) {
          if(udb) flt_mailonpost_destroy(udb);
          flt_mailonpost_destroy(db);
          return FLT_DECLINE;
        }
        /* }}} */

        cf_str_char_append(&str,'\x7F');
        cf_str_chars_append(&str,email,strlen(email));

        data.data = "1";
        data.size = sizeof("1");

        if(udb && (ret = udb->put(udb,NULL,&key,&data,DB_NODUPDATA|DB_NOOVERWRITE)) != 0) {
          if(ret != DB_KEYEXIST) {
            flt_mailonpost_destroy(udb);
            flt_mailonpost_destroy(db);
            fprintf(stderr,"flt_mailonpost: db->put() error: %s\n",db_strerror(ret));
            return FLT_DECLINE;
          }
        }
      }
      else {
        n = cf_split(str.content,"\x7F",&list);
        cf_str_cleanup(&str);

        for(i=0,ret=0;i<n;++i) {
          if(cf_strcmp(list[i],email) != 0) {
            if(ret) cf_str_char_append(&str,'\x74');
            ret = 1;
            cf_str_chars_append(&str,list[i],strlen(list[i]));
          }
          free(list[i]);
        }

        free(list);

        if(udb && (ret = udb->del(udb,NULL,&key,0)) != 0) {
          if(ret != DB_NOTFOUND) {
            flt_mailonpost_destroy(udb);
            flt_mailonpost_destroy(db);
            fprintf(stderr,"flt_mailonpost: db->del() error: %s\n",db_strerror(ret));
            return FLT_DECLINE;
          }
        }

        if(str.len == 0) {
          if((ret = db->del(db,NULL,&key,0)) != 0) {
            if(ret != DB_NOTFOUND) {
              if(udb) flt_mailonpost_destroy(udb);
              flt_mailonpost_destroy(db);
              fprintf(stderr,"flt_mailonpost: db->del() error: %s\n",db_strerror(ret));
              return FLT_DECLINE;
            }
          }
        }
      }
    }

    if(str.len) {
      memset(&data,0,sizeof(data));
      data.data = str.content;
      data.size = str.len;

      if((ret = db->put(db,NULL,&key,&data,0)) != 0) {
        flt_mailonpost_destroy(db);
        if(udb) flt_mailonpost_destroy(udb);

        fprintf(stderr,"flt_mailonpost: db->put() error: %s\n",db_strerror(ret));
        return FLT_DECLINE;
      }

      cf_str_cleanup(&str);
    }

    flt_mailonpost_destroy(db);
    if(udb) flt_mailonpost_destroy(udb);

    return FLT_OK;
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_mailonpost_msgcb */
const char *flt_mailonpost_msgcb(void **buf, int *len, void *arg) {
  struct s_smtp *inf = (struct s_smtp *)arg;

  if(len == NULL) {
    inf->pos = 0;
    return NULL;
  }

  if(inf->pos == inf->msg->len) {
    *len = 0;
    return NULL;
  }

  inf->pos = inf->msg->len;

  *len = inf->msg->len;
  return inf->msg->content;
}
/* }}} */

/* {{{ flt_mailonpost_encode_header */
void flt_mailonpost_encode_header(cf_string_t *str,const u_char *enc,size_t len) {
  static const u_char hex[] = "0123456789ABCDEF";
  int di = 0;
  unsigned x;
  u_char *ptr;
  size_t llen,clen;
  u_int32_t num;

  for(ptr=(u_char *)enc;*ptr && di == 0;++ptr) {
    if(((u_int8_t)*ptr) & 0x80) ++di;
  }

  if(di) {
    cf_str_chars_append(str,"=?UTF-8?Q?",10);

    for(llen=0,ptr=(u_char *)enc;*ptr;++ptr,++llen) {
      if(*ptr == ' ') cf_str_char_append(str,'_');
      else if(*ptr >= 0x7f || *ptr < 0x20 || *ptr == '_') {
        clen = utf8_to_unicode(ptr,len-(ptr-enc),&num);

        if(llen == 73 - clen) {
          cf_str_chars_append(str,"?=\n =?UTF-8?Q?",14);
          llen = 0;
        }

        for(x=0;x<clen;++x,++ptr) {
          cf_str_char_append(str,'=');
          cf_str_char_append(str,hex[(*ptr & 0xf0) >> 4]);
          cf_str_char_append(str,hex[*ptr & 0x0f]);
        }

        --ptr;
      }
      else cf_str_char_append(str,*ptr);

      if(llen == 75) {
        cf_str_chars_append(str,"?=\n =?UTF-8?Q?",14);
        llen = 0;
      }
    }

    cf_str_chars_append(str,"?=",2);
  }
  else cf_str_chars_append(str,enc,len);
}
/* }}} */

/* {{{ flt_mailonpost_parsestr */
void flt_mailonpost_parsestr(cl_thread_t *t,message_t *p,cf_string_t *str,const u_char *pars,const u_char *link,const u_char *mlink,int mode) {
  register u_char *ptr;
  u_char *l;

  cf_string_t str1,*str2 = NULL;

  cf_readmode_t *rm = cf_hash_get(GlobalValues,"RM",2);

  if(mode) {
    cf_str_init(&str1);
    str2 = str;
    str = &str1;
  }


  for(ptr=(u_char *)pars;*ptr;++ptr) {
    switch(*ptr) {
      case '%':
        switch(*(ptr+1)) {
          case 's':
            cf_str_str_append(str,&p->subject);
            ++ptr;
            continue;

          case 'a':
            cf_str_str_append(str,&p->author);
            ++ptr;
            continue;

          case 'u':
            cf_str_chars_append(str,link,strlen(link));
            ++ptr;
            continue;

          case 'm':
            cf_str_chars_append(str,mlink,strlen(mlink));
            ++ptr;
            continue;

          case 'b':
            if(t->threadmsg->prev) {
              switch(*(ptr+2)) {
                case 's':
                  cf_str_str_append(str,&t->threadmsg->prev->subject);
                  ptr += 2;
                  continue;

                case 'a':
                  cf_str_str_append(str,&t->threadmsg->prev->author);
                  ptr += 2;
                  continue;

                case 'u':
                  l = cf_get_link(rm->posting_uri[0],t->tid,t->threadmsg->mid);
                  cf_str_chars_append(str,l,strlen(l));
                  free(l);
                  ptr += 2;
                  continue;

                case 'm':
                  l = cf_get_link(rm->posting_uri[1],t->tid,t->threadmsg->mid);
                  cf_str_chars_append(str,l,strlen(l));
                  free(l);
                  ptr += 2;
                  continue;
              }
            }

            cf_str_char_append(str,'%');
            continue;

          case 'o':
            switch(*(ptr+2)) {
              case 's':
                cf_str_str_append(str,&t->messages->subject);
                ptr += 2;
                continue;

              case 'a':
                cf_str_str_append(str,&t->messages->author);
                ptr += 2;
                continue;

              case 'u':
                l = cf_get_link(rm->posting_uri[0],t->tid,t->messages->mid);
                cf_str_chars_append(str,l,strlen(l));
                free(l);
                ptr += 2;
                continue;

              case 'm':
                l = cf_get_link(rm->posting_uri[1],t->tid,t->messages->mid);
                cf_str_chars_append(str,l,strlen(l));
                free(l);
                ptr += 2;
                continue;
            }

            cf_str_char_append(str,'%');
            continue;
        }

      case '\\':
        if(*(ptr+1) == 'n') {
          cf_str_char_append(str,'\n');
          ++ptr;
          continue;
        }
      default:
        cf_str_char_append(str,*ptr);
    }
  }

  if(mode) {
    str = str2;
    flt_mailonpost_encode_header(str,str1.content,str1.len);
    cf_str_cleanup(&str1);
  }

}
/* }}} */

/* {{{ flt_mailonpost_mail */
void flt_mailonpost_mail(u_char **emails,u_int64_t len,message_t *p,cl_thread_t *t) {
  smtp_session_t sess;
  smtp_message_t msg;

  size_t i;

  struct s_smtp *inf;

  cf_readmode_t *rm = cf_hash_get(GlobalValues,"RM",2);

  u_char *msg_subj = cf_get_error_message("Send_Answer_Subject",NULL),
         *msg_body = cf_get_error_message("Send_Answer_Body",NULL),
         *link = cf_get_link(rm->posting_uri[0],t->tid,p->mid),
         *mylink = cf_get_link(rm->posting_uri[1],t->tid,p->mid);

  cf_string_t str;

  cf_str_init(&str);

  /* {{{ parse subject and message body */
  cf_str_chars_append(&str,"Content-Type: text/plain; charset=UTF-8\015\012Subject: ",50);
  flt_mailonpost_parsestr(t,p,&str,msg_subj,link,mylink,1);
  cf_str_chars_append(&str,"\015\012\015\012",4);
  flt_mailonpost_parsestr(t,p,&str,msg_body,link,mylink,0);

  free(mylink);
  free(link);
  /* }}} */

  if((sess = smtp_create_session()) == NULL) {
    cf_str_cleanup(&str);
    return;
  }

  smtp_set_server(sess,flt_mailonpost_host);

  for(i=0;i<len;++i) {
    msg = smtp_add_message(sess);
    smtp_set_reverse_path(msg,flt_mailonpost_rvrs);
    smtp_set_header(msg,"To",NULL,emails[i]);
    smtp_set_header(msg,"From",NULL,flt_mailonpost_from);

    smtp_add_recipient(msg,emails[i]);

    inf = cf_alloc(NULL,1,sizeof(*inf),CF_ALLOC_MALLOC);
    inf->pos = 0;
    inf->msg = &str;

    smtp_set_messagecb(msg,flt_mailonpost_msgcb,inf);
  }

  if(smtp_start_session(sess) == 0) fprintf(stderr,"flt_mailonpost: smtp_start: could not initiate smtp session: %s\n",strerror(errno));
  smtp_destroy_session(sess);

  cf_str_cleanup(&str);
}
/* }}} */

/* {{{ flt_mailonpost_execute */
#ifdef CF_SHARED_MEM
int flt_mailonpost_execute(cf_hash_t *head,cf_configuration_t *dc,cf_configuration_t *pc,message_t *p,u_int64_t tid,int sock,void *shm)
#else
int flt_mailonpost_execute(cf_hash_t *head,cf_configuration_t *dc,cf_configuration_t *pc,message_t *p,u_int64_t tid,int sock)
#endif
{
  int ret;
  u_char buff[256],**list = NULL;
  size_t n,i;
  cf_string_t str;
  cl_thread_t thr;

  DB *db = NULL;
  DBT key,data;

  rline_t rl;

  if(!flt_mailonpost_from || !flt_mailonpost_host || flt_mailonpost_create(&db,flt_mailonpost_dbname) == -1) return FLT_DECLINE;

  memset(&key,0,sizeof(key));
  memset(&data,0,sizeof(data));

  n = snprintf(buff,256,"t%"PRIu64,tid);
  key.data = buff;
  key.size = n;

  if((ret = db->get(db,NULL,&key,&data,0)) == 0 || flt_mailonpost_amail) {
    memset(&thr,0,sizeof(thr));
    memset(&rl,0,sizeof(rl));

    if(cf_get_message_through_sock(sock,&rl,&thr,tid,p->mid,CF_KILL_DELETED) == -1) {
      fprintf(stderr,"flt_mailonpost: Error getting thread: %s\n",ErrorString);
      return FLT_DECLINE;
    }

    if(ret == 0) {
      /* send mails... */
      cf_str_init(&str);
      cf_str_char_set(&str,data.data,data.size);

      n = cf_split(str.content,"\x7F",&list);
      if(n > 0) {
        flt_mailonpost_mail(list,n,p,&thr);

        for(i=0;i<n;++i) free(list[i]);
        free(list);
      }
    }

    if(flt_mailonpost_amail) flt_mailonpost_mail(&flt_mailonpost_amail,1,p,&thr);
  }

  flt_mailonpost_destroy(db);

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_mailonpost_post_handler */

int flt_mailonpost_post_handler(cf_hash_t *head,cf_configuration_t *dc,cf_configuration_t *vc,cl_thread_t *thread,cf_template_t *tpl) {
  u_char *link,buff[256];
  cf_name_value_t *cs,*email;
  size_t len;
  DB *db = NULL;
  int ret;

  cf_readmode_t *rm = cf_hash_get(GlobalValues,"RM",2);

  DBT key,data;

  if(flt_mailonpost_udb == NULL) return FLT_DECLINE;
  if(cf_hash_get(GlobalValues,"UserName",8) == NULL) return FLT_DECLINE;
  if((email = cf_cfg_get_first_value(vc,flt_mailonpost_fn,"EMail")) == NULL && flt_mailonpost_uemail == NULL) return FLT_DECLINE;
  if(flt_mailonpost_create(&db,flt_mailonpost_udb) == -1) return FLT_DECLINE;

  memset(&key,0,sizeof(key));
  memset(&data,0,sizeof(data));

  len = snprintf(buff,256,"t%"PRIu64,thread->tid);

  key.data = buff;
  key.size = len;

  if((ret = db->get(db,NULL,&key,&data,0)) != 0) {
    if(ret != DB_NOTFOUND) {
      fprintf(stderr,"flt_mailonpost: db->get() error: %s\n",db_strerror(ret));
      flt_mailonpost_destroy(db);
      return FLT_DECLINE;
    }
  }

  cs = cf_cfg_get_first_value(dc,flt_mailonpost_fn,"ExternCharset");

  if(ret == DB_NOTFOUND) {
    link = cf_advanced_get_link(rm->posting_uri[1],thread->tid,thread->messages->mid,NULL,1,&len,"mailonpost","yes");
    cf_set_variable(tpl,cs,"abolink",link,len,1);
  }
  else {
    link = cf_advanced_get_link(rm->posting_uri[1],thread->tid,thread->messages->mid,NULL,1,&len,"mailonpost","no");
    cf_set_variable(tpl,cs,"unabolink",link,len,1);
  }

  free(link);

  flt_mailonpost_destroy(db);

  return FLT_OK;
}

/* }}} */

/* {{{ flt_mailonpost_cmd */
int flt_mailonpost_cmd(cf_configfile_t *cfile,cf_conf_opt_t *opt,const u_char *context,u_char **args,size_t argnum) {
  if(flt_mailonpost_fn == NULL) flt_mailonpost_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(flt_mailonpost_fn,context) != 0) return 0;

  if(cf_strcmp(opt->name,"SMTPHost") == 0) {
    if(flt_mailonpost_host) free(flt_mailonpost_host);
    flt_mailonpost_host = strdup(args[0]);
  }
  else if(cf_strcmp(opt->name,"MailDatabase") == 0) {
    if(flt_mailonpost_dbname) free(flt_mailonpost_dbname);
    flt_mailonpost_dbname = strdup(args[0]);
  }
  else if(cf_strcmp(opt->name,"SMTPReverse") == 0) {
    if(flt_mailonpost_rvrs) free(flt_mailonpost_rvrs);
    flt_mailonpost_rvrs = strdup(args[0]);
  }
  else if(cf_strcmp(opt->name,"MailUserDB") == 0) {
    if(flt_mailonpost_udb) free(flt_mailonpost_udb);
    flt_mailonpost_udb = strdup(args[0]);
  }
  else if(cf_strcmp(opt->name,"UserMail") == 0) {
    if(flt_mailonpost_uemail) free(flt_mailonpost_uemail);
    flt_mailonpost_uemail = strdup(args[0]);
  }
  else if(cf_strcmp(opt->name,"AlwaysMail") == 0) {
    if(flt_mailonpost_amail) free(flt_mailonpost_amail);
    flt_mailonpost_amail = strdup(args[0]);
  }
  else {
    if(flt_mailonpost_from) free(flt_mailonpost_from);
    flt_mailonpost_from = strdup(args[0]);
  }

  return 0;
}
/* }}} */

void flt_mailonpost_cleanup(void) {
  if(flt_mailonpost_from) free(flt_mailonpost_from);
  if(flt_mailonpost_host) free(flt_mailonpost_host);
  if(flt_mailonpost_rvrs) free(flt_mailonpost_rvrs);
  if(flt_mailonpost_dbname) free(flt_mailonpost_dbname);
}

cf_conf_opt_t flt_mailonpost_config[] = {
  { "SMTPHost",     flt_mailonpost_cmd, CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, NULL },
  { "SMTPFrom",     flt_mailonpost_cmd, CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, NULL },
  { "SMTPReverse",  flt_mailonpost_cmd, CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, NULL },
  { "MailDatabase", flt_mailonpost_cmd, CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL|CF_CFG_OPT_NEEDED, NULL },
  { "MailUserDB",   flt_mailonpost_cmd, CF_CFG_OPT_USER|CF_CFG_OPT_LOCAL,   NULL },
  { "UserMail",     flt_mailonpost_cmd, CF_CFG_OPT_USER|CF_CFG_OPT_LOCAL,   NULL },
  { "AlwaysMail",   flt_mailonpost_cmd, CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, NULL },
  { NULL, NULL, 0, NULL }
};

cf_handler_config_t flt_mailonpost_handlers[] = {
  { INIT_HANDLER,         flt_mailonpost_init_handler },
  { AFTER_POST_HANDLER,   flt_mailonpost_execute },
  { POSTING_HANDLER,      flt_mailonpost_post_handler },
  { 0, NULL }
};

cf_module_config_t flt_mailonpost = {
  MODULE_MAGIC_COOKIE,
  flt_mailonpost_config,
  flt_mailonpost_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  flt_mailonpost_cleanup
};

/* eof */
