/**
 * \file flt_mailonpost.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
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
#include "config.h"
#include "defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include <sys/file.h>

#include <time.h>
#include <db.h>

#include <libesmtp.h>

#include "readline.h"
#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "cfcgi.h"
#include "template.h"
#include "clientlib.h"
#include "htmllib.h"
#include "fo_post.h"
/* }}} */

static u_char *flt_mailonpost_from   = NULL;
static u_char *flt_mailonpost_host   = NULL;
static u_char *flt_mailonpost_dbname = NULL;
static u_char *flt_mailonpost_fn     = NULL;
static u_char *flt_mailonpost_rvrs   = NULL;

struct s_smtp {
  t_string *msg;
  size_t pos;
};

/* {{{ flt_mailonpost_create */
int flt_mailonpost_create(DB **db) {
  int ret,fd;

  if((ret = db_create(db,NULL,0)) != 0) {
    fprintf(stderr,"DB error: %s\n",db_strerror(ret));
    return -1;
  }

  if((ret = (*db)->open(*db,NULL,flt_mailonpost_dbname,NULL,DB_BTREE,DB_CREATE,0644)) != 0) {
    fprintf(stderr,"DB error: %s\n",db_strerror(ret));
    return -1;
  }

  if((ret = (*db)->fd(*db,&fd)) != 0) {
    fprintf(stderr,"DB error: %s\n",db_strerror(ret));
    return -1;
  }

  if((ret = flock(fd,LOCK_EX)) != 0) {
    fprintf(stderr,"DB error: %s\n",db_strerror(ret));
    return -1;
  }

  return 0;
}
/* }}} */

/* {{{ flt_mailonpost_init_handler */
int flt_mailonpost_init_handler(t_cf_hash *head,t_configuration *dc,t_configuration *vc) {
  u_char *val,*email,*ctid,buff[256],*user = cf_hash_get(GlobalValues,"UserName",8);
  DB *db = NULL;
  DBT key,data;
  size_t n;
  u_int64_t tid;
  int ret;
  t_string str;
  t_name_value *v;

  if(!head) return FLT_DECLINE;

  if((val = cf_cgi_get(head,"mailonpost")) != NULL && cf_strcmp(val,"yes") == 0 && (ctid = cf_cgi_get(head,"t")) != NULL) {
    tid = str_to_u_int64(ctid);

    if(tid) {
      /* {{{ get email address; either from CGI parameter or from config file, depends */
      if((email = cf_cgi_get(head,"EMail")) == NULL) {
        if(user == NULL) return FLT_DECLINE;
        if((v = cfg_get_first_value(vc,flt_mailonpost_fn,"EMail")) == NULL) return FLT_DECLINE;

        email = v->values[0];
      }
      /* }}} */

      if(flt_mailonpost_create(&db) == -1) return FLT_DECLINE;

      n = snprintf(buff,256,"t%llu",tid);

      memset(&key,0,sizeof(key));
      memset(&data,0,sizeof(data));

      str_init(&str);

      key.data = buff;
      key.size = n;

      if((ret = db->get(db,NULL,&key,&data,0)) != 0) {
        if(ret != DB_NOTFOUND) {
          fprintf(stderr,"DB error: %s\n",db_strerror(ret));
          return FLT_DECLINE;
        }
        else str_chars_append(&str,email,strlen(email));
      }
      else {
        str_char_set(&str,data.data,data.size);
        str_char_append(&str,127);
        str_chars_append(&str,email,strlen(email));
      }

      memset(&data,0,sizeof(data));
      data.data = str.content;
      data.size = str.len;

      if((ret = db->put(db,NULL,&key,&data,0)) != 0) {
        fprintf(stderr,"DB error: %s\n",db_strerror(ret));
        return FLT_DECLINE;
      }

      db->close(db,0);
      str_cleanup(&str);

      return FLT_OK;
    }
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_mailonpost_create_msg */
void flt_mailonpost_create_msg(t_string *str,t_string *subj,t_string *body) {
  str_chars_append(str,"Content-Type: text/plain; charset=UTF-8\015\012Subject: ",50);
  str_str_append(str,subj);
  str_chars_append(str,"\015\012\015\012",4);
  str_str_append(str,body);
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

/* {{{ flt_mailonpost_mail */
void flt_mailonpost_mail(u_char **emails,u_int64_t len,t_message *p,u_int64_t tid) {
  smtp_session_t sess;
  smtp_message_t msg;

  size_t i;

  struct s_smtp *inf;

  t_name_value *v = cfg_get_first_value(&fo_default_conf,flt_mailonpost_fn,"PostingURL");

  u_char *msg_subj = cf_get_error_message("Send_Answer_Subject",NULL),
         *msg_body = cf_get_error_message("Send_Answer_Body",NULL),
         *ptr,
         *link = cf_get_link(v->values[0],NULL,tid,p->mid);

  t_string subj,body,str;

  str_init(&subj);
  str_init(&body);

  for(ptr=msg_subj;*ptr;++ptr) {
    if(cf_strncmp(ptr,"%u",2) == 0) str_chars_append(&subj,link,strlen(link));
    else if(cf_strncmp(ptr,"%s",2) == 0) str_str_append(&subj,&p->subject);
    else if(cf_strncmp(ptr,"\\n",2) == 0) str_char_append(&body,'\n');
    else str_char_append(&subj,*ptr);
  }
  for(ptr=msg_body;*ptr;++ptr) {
    if(cf_strncmp(ptr,"%u",2) == 0) str_chars_append(&body,link,strlen(link));
    else if(cf_strncmp(ptr,"%s",2) == 0) str_str_append(&body,&p->subject);
    else if(cf_strncmp(ptr,"\\n",2) == 0) str_char_append(&body,'\n');
    else str_char_append(&body,*ptr);
  }

  free(link);

  str_init(&str);
  flt_mailonpost_create_msg(&str,&subj,&body);

  str_cleanup(&subj);
  str_cleanup(&body);

  if((sess = smtp_create_session()) == NULL) {
    str_cleanup(&subj);
    str_cleanup(&body);
    return;
  }

  smtp_set_server(sess,flt_mailonpost_host);

  for(i=0;i<len;++i) {
    msg = smtp_add_message(sess);
    smtp_set_reverse_path(msg,flt_mailonpost_rvrs);
    smtp_set_header(msg,"To",NULL,emails[i]);
    smtp_set_header(msg,"From",NULL,flt_mailonpost_from);

    smtp_add_recipient(msg,emails[i]);

    inf = fo_alloc(NULL,1,sizeof(*inf),FO_ALLOC_MALLOC);
    inf->pos = 0;
    inf->msg = &str;

    smtp_set_messagecb(msg,flt_mailonpost_msgcb,inf);
  }

  smtp_start_session(sess);
  smtp_destroy_session(sess);

  str_cleanup(&str);
}
/* }}} */

/* {{{ flt_mailonpost_execute */
#ifndef CF_SHARED_MEM
int flt_mailonpost_execute(t_cf_hash *head,t_configuration *dc,t_configuration *pc,t_message *p,u_int64_t tid,int sock,void *shm)
#else
int flt_mailonpost_execute(t_cf_hash *head,t_configuration *dc,t_configuration *pc,t_message *p,u_int64_t tid,int sock)
#endif
{
  int ret;
  u_char buff[256],**list = NULL;
  size_t n,i;
  t_string str;

  struct s_smtp *inf;

  DB *db = NULL;
  DBT key,data;

  if(!flt_mailonpost_from || !flt_mailonpost_host || flt_mailonpost_create(&db) == -1) return FLT_DECLINE;

  memset(&key,0,sizeof(key));
  memset(&data,0,sizeof(data));

  n = snprintf(buff,256,"t%llu",tid);
  key.data = buff;
  key.size = n;

  if(db->get(db,NULL,&key,&data,0) == 0) {
    /* send mails... */
    str_init(&str);
    str_char_set(&str,data.data,data.size);

    n = split(str.content,"\127",&list);

    if(n > 0) {
      flt_mailonpost_mail(list,n,p,tid);

      for(i=0;i<n;++i) free(list[i]);
      free(list);
    }
  }

  db->close(db,0);

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_mailonpost_cmd */
int flt_mailonpost_cmd(t_configfile *cfile,t_conf_opt *opt,const u_char *context,u_char **args,size_t argnum) {
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

t_conf_opt flt_mailonpost_config[] = {
  { "SMTPHost",     flt_mailonpost_cmd, CFG_OPT_CONFIG|CFG_OPT_LOCAL, NULL },
  { "SMTPFrom",     flt_mailonpost_cmd, CFG_OPT_CONFIG|CFG_OPT_LOCAL, NULL },
  { "SMTPReverse",  flt_mailonpost_cmd, CFG_OPT_CONFIG|CFG_OPT_LOCAL, NULL },
  { "MailDatabase", flt_mailonpost_cmd, CFG_OPT_CONFIG|CFG_OPT_LOCAL|CFG_OPT_NEEDED, NULL },
  { NULL, NULL, 0, NULL }
};

t_handler_config flt_mailonpost_handlers[] = {
  { INIT_HANDLER,         flt_mailonpost_init_handler },
  { AFTER_POST_HANDLER,   flt_mailonpost_execute },
  { 0, NULL }
};

t_module_config flt_mailonpost = {
  flt_mailonpost_config,
  flt_mailonpost_handlers,
  NULL,
  NULL,
  NULL,
  flt_mailonpost_cleanup
};

/* eof */
