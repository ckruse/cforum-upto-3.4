%module "CForum::Clientlib"
%{
#include "config.h"
#include "defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "readline.h"
#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "cfcgi.h"
#include "template.h"
#include "clientlib.h"
#include "fo_view.h"
%}

%init %{
  cf_init();
%}

typedef struct s_flag {
  char *name;
  char *val;
} t_cf_post_flag;

%extend t_cf_post_flag {
  t_cf_post_flag(const char *name,const char *value);
}

/* {{{ t_cf_post_flag */
%{
t_cf_post_flag *new_t_cf_post_flag(const char *name,const char *value) {
  t_cf_post_flag *flag = fo_alloc(NULL,1,sizeof(*flag),FO_ALLOC_MALLOC);

  flag->name = strdup(name);
  flag->val = strdup(value);

  return flag;
}
%}
/* }}} */

typedef struct s_message {
  int date;
  int level;
  int may_show;
  int invisible;
  int votes_good;
  int votes_bad;

  struct s_message *next,*prev;
} t_message;

%extend t_message {
  char *get_mid();

  t_cf_list_head *get_flags();

  const char *get_author();
  void set_author(const char *author);

  const char *get_email();
  void set_email(const char *email);

  const char *get_hp();
  void set_hp(const char *hp);

  const char *get_img();
  void set_img(const char *img);

  const char *get_subject();
  void set_subject(const char *subj);

  const char *get_category();
  void set_category(const char *cat);

  const char *get_content();
  void set_content(const char *cnt);

  t_message *get_first_visible();
  t_message *delete_subtree();
  t_message *next_subtree();
  t_message *prev_subtree();
  t_message *get_parent();
  bool has_answers();
}

/* {{{ t_message */
%{
t_message *t_message_get_first_visible(t_message *msg) {
  return cf_msg_get_first_visible(msg);
}

t_message *t_message_delete_subtree(t_message *msg) {
  return cf_msg_delete_subtree(msg);
}
t_message *t_message_next_subtree(t_message *msg) {
  return cf_msg_next_subtree(msg);
}
t_message *t_message_prev_subtree(t_message *msg) {
  return cf_msg_prev_subtree(msg);
}
t_message *t_message_get_parent(t_message *msg) {
  return cf_msg_get_parent(msg);
}
bool t_message_has_answers(t_message *msg) {
  return cf_msg_has_answers(msg);
}

u_char *t_message_get_mid(t_message *msg) {
  t_string str;
  str_init(&str);

  u_int64_to_str(&str,msg->mid);

  return str.content;
}

t_cf_list_head *t_message_get_flags(t_message *msg) {
  return &msg->flags;
}

const char *t_message_get_author(t_message *msg) {
  return msg->author.content;
}
void t_message_set_author(t_message *msg,const u_char *author) {
  str_chars_set(&msg->author,author,strlen(author));
}

const char *t_message_get_email(t_message *msg) {
  return msg->email.content;
}
void t_message_set_email(t_message *msg,const u_char *email) {
  str_chars_set(&msg->email,email,strlen(email));
}

const char *t_message_get_hp(t_message *msg) {
  return msg->hp.content;
}
void t_message_set_hp(t_message *msg,const u_char *hp) {
  str_chars_set(&msg->hp,hp,strlen(hp));
}

const char *t_message_get_img(t_message *msg) {
  return msg->img.content;
}
void t_message_set_img(t_message *msg,const u_char *img) {
  str_chars_set(&msg->img,img,strlen(img));
}

const char *t_message_get_subject(t_message *msg) {
  return msg->subject.content;
}
void t_message_set_subject(t_message *msg,const u_char *subj) {
  str_chars_set(&msg->subject,subj,strlen(subj));
}

const char *t_message_get_category(t_message *msg) {
  return msg->category.content;
}
void t_message_set_category(t_message *msg,const u_char *cat) {
  str_chars_set(&msg->category,cat,strlen(cat));
}

const char *t_message_get_content(t_message *msg) {
  return msg->content.content;
}
void t_message_set_content(t_message *msg,const u_char *cnt) {
  str_chars_set(&msg->content,cnt,strlen(cnt));
}

%}
/* }}} */

typedef struct s_cl_thread {
  int msg_len;

  t_message *messages;
  t_message *last;
  t_message *threadmsg;
  t_message *newest;
} t_cl_thread;

%extend t_cl_thread {
  char *get_tid();
}

/* {{{ t_cl_thread */
%{
char *t_cl_thread_get_tid(t_cl_thread *thr) {
  t_string str;

  str_init(&str);
  u_int64_to_str(&str,thr->tid);

  return str.content;
}
%}
/* }}} */

%{
const u_char *get_global_value(const u_char *name) {
  u_char *val = cf_hash_get(GlobalValues,(u_char *)name,strlen(name));
  return val;
}
%}
const char *get_global_value(const char *name);

%immutable;
const char *ErrorString;
%mutable;

%{
#ifndef CF_SHARED_MEM
void *cf_get_shm_ptr(void) {
  return NULL;
}
void *cf_reget_shm_ptr(void) {
  return NULL;
}
#endif
%}

void *cf_get_shm_ptr();
void *cf_reget_shm_ptr();

char *cf_get_uconf_name(const char *uname);
int cf_socket_setup(void);

%{
u_char *cf_pl_tpl_name(const u_char *name) {
  u_char *buff = fo_alloc(NULL,1,1,FO_ALLOC_MALLOC);
  cf_gen_tpl_name(buff,256,name);

  return buff;
}
%}
char *cf_pl_tpl_name(const char *name);

void cf_set_variable(t_cf_template *tpl,t_name_value *cs,const char *vname,const char *val,size_t len,bool html);
void cf_error_message(const char *msg,FILE *out, ...);
char *cf_get_error_message(const char *msg,int *len, ...);

%{
char *cf_pl_get_link(const char *link,const char *ctid,const char *cmid) {
  u_int64_t tid,mid;

  tid = str_to_u_int64(ctid);
  mid = str_to_u_int64(cmid);
  return cf_get_link(link,tid,mid);
}
%}
char *cf_pl_get_link(const char *link,const char *ctid,const char *cmid);

%{
char *cf_pl_advanced_get_link(const u_char *link,const char *ctid,const char *cmid,const char *parameters) {
  u_int64_t tid,mid;

  tid = str_to_u_int64(ctid);
  mid = str_to_u_int64(cmid);

  return cf_advanced_get_link(link,tid,mid,NULL,1,NULL,parameters,"");
}
%}
char *cf_pl_advanced_get_link(const u_char *link,const char *ctid,const char *cmid,const char *parameters);

%{
char *cf_pl_general_get_time(const char *fmt,const char *locale,unsigned int date) {
  return cf_general_get_time((u_char *)fmt,(u_char *)locale,NULL,(time_t *)&date);
}
%}
char *cf_pl_general_get_time(const char *fmt,const char *locale,unsigned int date);

%{
t_cl_thread *cf_pl_get_message_through_sock(int sock,rline_t *tsd,const u_char *ctid,const u_char *cmid,bool del) {
  t_cl_thread *thr = fo_alloc(NULL,1,sizeof(*thr),FO_ALLOC_CALLOC);

  u_int64_t tid = str_to_u_int64(ctid);
  u_int64_t mid = str_to_u_int64(cmid);

  if(cf_get_message_through_sock(sock,tsd,thr,tid,mid,del) != 0) {
    free(thr);
    return NULL;
  }

  return thr;
}
%}
t_cl_thread *cf_pl_get_message_through_sock(int sock,rline_t *tsd,const char *ctid,const char *cmid,bool del);

%{
t_cl_thread *cf_pl_get_next_thread_through_sock(int sock,rline_t *tsd) {
  t_cl_thread *thr = fo_alloc(NULL,1,sizeof(*thr),FO_ALLOC_CALLOC);

  if(cf_get_next_thread_through_sock(sock,tsd,thr) != 0) {
    free(thr);
    return NULL;
  }

  return thr;
}
%}
t_cl_thread *cf_pl_get_next_thread_through_sock(int sock,rline_t *tsd);

%{
t_cl_thread *cf_pl_get_message_through_shm(void *shm_ptr,const char *ctid,const char *cmid,bool del) {
  t_cl_thread *thr = fo_alloc(NULL,1,sizeof(*thr),FO_ALLOC_CALLOC);

  u_int64_t tid = str_to_u_int64(ctid);
  u_int64_t mid = str_to_u_int64(cmid);

  if(cf_get_message_through_shm(shm_ptr,thr,tid,mid,del) != 0) {
    free(thr);
    return NULL;
  }

  return thr;
}
%}
t_cl_thread *cf_pl_get_message_through_shm(void *shm_ptr,const char *ctid,const char *cmid,bool del);


char *charset_convert(const char *toencode,int in_len,const char *from_charset,const char *to_charset,int *out_len_p);
char *htmlentities(const char *string,int sq);
int utf8_to_unicode(const char *s,int n,int *num);
char *htmlentities_charset_convert(const char *toencode, const char *from, const char *to,int *outlen,bool sq);
char *charset_convert_entities(const char *toencode, int in_len,const char *from, const char *to,size_t *outlen);
int is_valid_utf8_string(const char *str,int len);
char *htmlentities_decode(const char *string);

%{
bool cf_has_shm(void) {
  #ifdef CF_SHARED_MEM
  return 1;
  #else
  return 0;
  #endif
}
%}

bool cf_has_shm(void);

typedef struct s_rline_t {
  int rl_len;
} rline_t;

%{
rline_t *new_rlinet(void) {
  rline_t *ret = fo_alloc(NULL,1,sizeof(*ret),FO_ALLOC_CALLOC);
  return ret;
}
%}

rline_t *new_rlinet(void);

/* eof */
