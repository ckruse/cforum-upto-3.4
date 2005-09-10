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
} cf_post_flag_t;

%extend cf_post_flag_t {
  cf_post_flag_t(const char *name,const char *value);
}

/* {{{ cf_post_flag_t */
%{
cf_post_flag_t *new_cf_post_flag_t(const char *name,const char *value) {
  cf_post_flag_t *flag = fo_alloc(NULL,1,sizeof(*flag),FO_ALLOC_MALLOC);

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
} message_t;

%extend message_t {
  char *get_mid();

  cf_list_head_t *get_flags();

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

  message_t *get_first_visible();
  message_t *delete_subtree();
  message_t *next_subtree();
  message_t *prev_subtree();
  message_t *get_parent();
  bool has_answers();
}

/* {{{ message_t */
%{
message_t *message_t_get_first_visible(message_t *msg) {
  return cf_msg_get_first_visible(msg);
}

message_t *message_t_delete_subtree(message_t *msg) {
  return cf_msg_delete_subtree(msg);
}
message_t *message_t_next_subtree(message_t *msg) {
  return cf_msg_next_subtree(msg);
}
message_t *message_t_prev_subtree(message_t *msg) {
  return cf_msg_prev_subtree(msg);
}
message_t *message_t_get_parent(message_t *msg) {
  return cf_msg_get_parent(msg);
}
bool message_t_has_answers(message_t *msg) {
  return cf_msg_has_answers(msg);
}

u_char *message_t_get_mid(message_t *msg) {
  string_t str;
  str_init(&str);

  u_int64_to_str(&str,msg->mid);

  return str.content;
}

cf_list_head_t *message_t_get_flags(message_t *msg) {
  return &msg->flags;
}

const char *message_t_get_author(message_t *msg) {
  return msg->author.content;
}
void message_t_set_author(message_t *msg,const u_char *author) {
  str_chars_set(&msg->author,author,strlen(author));
}

const char *message_t_get_email(message_t *msg) {
  return msg->email.content;
}
void message_t_set_email(message_t *msg,const u_char *email) {
  str_chars_set(&msg->email,email,strlen(email));
}

const char *message_t_get_hp(message_t *msg) {
  return msg->hp.content;
}
void message_t_set_hp(message_t *msg,const u_char *hp) {
  str_chars_set(&msg->hp,hp,strlen(hp));
}

const char *message_t_get_img(message_t *msg) {
  return msg->img.content;
}
void message_t_set_img(message_t *msg,const u_char *img) {
  str_chars_set(&msg->img,img,strlen(img));
}

const char *message_t_get_subject(message_t *msg) {
  return msg->subject.content;
}
void message_t_set_subject(message_t *msg,const u_char *subj) {
  str_chars_set(&msg->subject,subj,strlen(subj));
}

const char *message_t_get_category(message_t *msg) {
  return msg->category.content;
}
void message_t_set_category(message_t *msg,const u_char *cat) {
  str_chars_set(&msg->category,cat,strlen(cat));
}

const char *message_t_get_content(message_t *msg) {
  return msg->content.content;
}
void message_t_set_content(message_t *msg,const u_char *cnt) {
  str_chars_set(&msg->content,cnt,strlen(cnt));
}

%}
/* }}} */

typedef struct s_cl_thread {
  int msg_len;

  message_t *messages;
  message_t *last;
  message_t *threadmsg;
  message_t *newest;
} cl_thread_t;

%extend cl_thread_t {
  char *get_tid();
}

/* {{{ cl_thread_t */
%{
char *cl_thread_t_get_tid(cl_thread_t *thr) {
  string_t str;

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

void cf_set_variable(cf_template_t *tpl,name_value_t *cs,const char *vname,const char *val,size_t len,bool html);
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
cl_thread_t *cf_pl_get_message_through_sock(int sock,rline_t *tsd,const u_char *ctid,const u_char *cmid,bool del) {
  cl_thread_t *thr = fo_alloc(NULL,1,sizeof(*thr),FO_ALLOC_CALLOC);

  u_int64_t tid = str_to_u_int64(ctid);
  u_int64_t mid = str_to_u_int64(cmid);

  if(cf_get_message_through_sock(sock,tsd,thr,tid,mid,del) != 0) {
    free(thr);
    return NULL;
  }

  return thr;
}
%}
cl_thread_t *cf_pl_get_message_through_sock(int sock,rline_t *tsd,const char *ctid,const char *cmid,bool del);

%{
cl_thread_t *cf_pl_get_next_thread_through_sock(int sock,rline_t *tsd) {
  cl_thread_t *thr = fo_alloc(NULL,1,sizeof(*thr),FO_ALLOC_CALLOC);

  if(cf_get_next_thread_through_sock(sock,tsd,thr) != 0) {
    free(thr);
    return NULL;
  }

  return thr;
}
%}
cl_thread_t *cf_pl_get_next_thread_through_sock(int sock,rline_t *tsd);

%{
cl_thread_t *cf_pl_get_message_through_shm(void *shm_ptr,const char *ctid,const char *cmid,bool del) {
  cl_thread_t *thr = fo_alloc(NULL,1,sizeof(*thr),FO_ALLOC_CALLOC);

  u_int64_t tid = str_to_u_int64(ctid);
  u_int64_t mid = str_to_u_int64(cmid);

  if(cf_get_message_through_shm(shm_ptr,thr,tid,mid,del) != 0) {
    free(thr);
    return NULL;
  }

  return thr;
}
%}
cl_thread_t *cf_pl_get_message_through_shm(void *shm_ptr,const char *ctid,const char *cmid,bool del);


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
