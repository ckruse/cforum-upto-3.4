/**
 * \file flt_replace.c
 * \author Christian Kruse, <cjk@wwwtech.de>
 *
 * Replace text pre and post write process
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
#include <string.h>
#include <time.h>
#include <sys/types.h>

#include <ctype.h>

#include <pcre.h>

#include "readline.h"
#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "cfcgi.h"
#include "template.h"
#include "clientlib.h"
#include "charconvert.h"
#include "validate.h"
#include "fo_post.h"
/* }}} */

#define FLT_REPLACE_BYHTTP 1

typedef struct {
  pcre *pattern;
  u_char *replacement;
  int http;
} flt_replace_entry_t;

static array_t flt_replace_prepost;
static array_t flt_replace_postpost;
static int flt_replace_mustinit = 1;
static u_char *flt_replace_fn = NULL;

/* {{{ flt_replace_body_plain2coded */
string_t *flt_replace_body_plain2coded(const u_char *text,configuration_t *cfg) {
  u_char *body;
  string_t *str = fo_alloc(NULL,1,sizeof(*str),FO_ALLOC_CALLOC);
  u_char *ptr,*end,*qchars;
  name_value_t *v;
  size_t len;
  int sig = 0;

  str_init(str);

  v = cfg_get_first_value(cfg,flt_replace_fn,"QuotingChars");

  qchars = htmlentities(v->values[0],0);
  len = strlen(qchars);

  /* first phase: we need the string entity encoded */
  body = htmlentities(text,0);

  /*
   * second phase:
   * - normalize newlines
   * - normalize [message:], [image:], [iframe:] and [link:]
   * - normalize quoting characters
   */
  for(ptr=body;*ptr;++ptr) {
    switch(*ptr) {
      case '\015':
        if(*(ptr+1) == '\012') ++ptr;

      case '\012':
        str_char_append(str,'\012');

        if(cf_strncmp(ptr+1,qchars,len) == 0) {
          str_char_append(str,(u_char)127);
          ptr += len;
        }

        break;

      default:
        if(ptr == body && cf_strncmp(ptr,qchars,len) == 0) {
          str_char_append(str,(u_char)127);
          ptr += len - 1;
        }
        else str_char_append(str,*ptr);
    }
  }

  /*
   * ok, third phase:
   * - strip whitespaces at the end of a line and string
   * - change more than one whitespace at the beginning to &nbsp;
   * - transform the signature to internal representation
   * - transform \n to <br />
   */

  free(body);
  body = str->content;
  str_init(str);

  for(ptr=body;*ptr;ptr++) {
    if(cf_strncmp(ptr,"\012-- \012",5) == 0 && sig == 0 && strstr(ptr+1,"\012-- \012") == NULL) { /* we prereserve \n-- \n for sig handling; special cases suck! */
      sig = 1;
      ptr += 4;
      str_chars_append(str,"<br />-- <br />",15);
    }
    else if(*ptr == '\012') str_chars_append(str,"<br />",6);
    else if(isspace(*ptr)) {
      for(end=ptr;*end && isspace(*end) && *end != '\012';++end);

      /* strip newlines at the end of the line */
      if(*end == '\012') {
        ptr = end - 1;
        continue;
      }

      if(end-ptr == 1 && (ptr - 1 >= body && *(ptr-1) != '\012')) {
        str_char_append(str,' ');
        continue;
      }

      /* transform spaces after newlines to &nbsp; */
      for(len=0;len<(size_t)(end-ptr);++len) str_chars_append(str,"\xC2\xA0",2);

      ptr = end - 1;
    }
    else str_char_append(str,*ptr);
  }

  free(qchars);

  /* all phases finished, body has been normalized */
  return str;
}
/* }}} */

/* {{{ flt_replace_destroy */
void flt_replace_destroy(void *a) {
  flt_replace_entry_t *x = (flt_replace_entry_t *)a;
  free(x->replacement);
  pcre_free(x->pattern);
}
/* }}} */

void flt_replace_replacethem(string_t *content,array_t *replacers,configuration_t *cfg) {
  size_t i;
  int matches,ovector[42],brefnum,breflen,default_action;
  flt_replace_entry_t *ent;
  string_t *str,str1,str2;
  u_char *ptr,*bref = NULL,*safe;
  cf_http_response_t *rsp;

  for(i=0;i<replacers->elements;++i) {
    ent = array_element_at(replacers,i);

    while((matches = pcre_exec(ent->pattern,NULL,content->content,(int)content->len,0,0,ovector,42)) >= 0) {
      str_init(&str1);
      default_action = 0;

      /* {{{ create replacement string */
      for(ptr=ent->replacement;*ptr;++ptr) {
        if(*ptr == '$') {
          if(ptr == ent->replacement || *(ptr-1) != '\\') {
            if((brefnum = strtol((const char *)ptr+1,(char **)&safe,10)) != 0) {
              ptr = safe - 1;
              breflen = pcre_get_substring(content->content,ovector,matches!=0?matches:14,brefnum,(const char **)&bref);
              if(breflen > 0) {
                str_chars_append(&str1,bref,breflen);
                free(bref);
              }
            }
            else str_char_append(&str1,'$');
          }
          else str_char_append(&str1,'$');
        }
        else str_char_append(&str1,*ptr);
      }
      /* }}} */

      /* {{{ get replacement by http request */
      if(ent->http) {
        if(is_valid_http_link(str1.content,1) == 0) {
          if((rsp = cf_http_simple_get_uri(str1.content,0)) != NULL && rsp->content.content != NULL) {
            str = flt_replace_body_plain2coded(rsp->content.content,cfg);
            cf_http_destroy_response(rsp);
            free(rsp);

            if(str) {
              if(is_valid_utf8_string(str->content,str->len) == 0) {
                str_init(&str2);
                str_chars_append(&str2,content->content,ovector[0]);
                str_str_append(&str2,str);
                str_chars_append(&str2,content->content+ovector[1],content->len - ovector[1]);

                str_cleanup(content);
                memcpy(content,&str2,sizeof(str2));
              }
              else default_action = 1;

              str_cleanup(str);
              free(str);
            }
            else default_action = 1;
          }
          else default_action = 1;
        }
        else default_action = 1;
      }
      else default_action = 1;
      /* }}} */

      if(default_action) {
        str_init(&str2);
        str_chars_append(&str2,content->content,ovector[0]);
        str_str_append(&str2,&str1);
        str_chars_append(&str2,content->content+ovector[1],content->len - ovector[1]);

        str_cleanup(content);
        memcpy(content,&str2,sizeof(str2));
      }

      str_cleanup(&str1);
    }
  }
}

/* {{{ flt_replace_showpost */
int flt_replace_showpost(cf_hash_t *head,configuration_t *dc,configuration_t *vc,cl_thread_t *thread,cf_template_t *tpl) {
  name_value_t *rm;
  message_t *msg;
  int ShowInvisible;

  if(flt_replace_fn == NULL) flt_replace_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);

  rm = cfg_get_first_value(vc,flt_replace_fn,"ReadMode");

  if(flt_replace_mustinit) {
    flt_replace_mustinit = 0;
    array_init(&flt_replace_prepost,sizeof(flt_replace_entry_t),flt_replace_destroy);
    array_init(&flt_replace_postpost,sizeof(flt_replace_entry_t),flt_replace_destroy);
  }

  if(cf_strcmp(rm->values[0],"thread") != 0) {
    ShowInvisible = cf_hash_get(GlobalValues,"ShowInvisible",13) == NULL ? 0 : 1;

    for(msg=thread->messages;msg;msg=msg->next) {
      if((msg->may_show && msg->invisible == 0) || ShowInvisible == 1) flt_replace_replacethem(&msg->content,&flt_replace_postpost,vc);
    }
  }
  else flt_replace_replacethem(&thread->threadmsg->content,&flt_replace_postpost,vc);
  return FLT_OK;
}
/* }}} */

/* {{{ flt_replace_newpost */
#ifdef CF_SHARED_MEM
int flt_replace_newpost(cf_hash_t *head,configuration_t *dc,configuration_t *pc,message_t *p,cl_thread_t *thr,void *mptr,int sock,int mode)
#else
int flt_replace_newpost(cf_hash_t *head,configuration_t *dc,configuration_t *pc,message_t *p,cl_thread_t *thr,int sock,int mode)
#endif
{
  if(flt_replace_fn == NULL) flt_replace_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);

  if(flt_replace_mustinit) {
    flt_replace_mustinit = 0;
    array_init(&flt_replace_prepost,sizeof(flt_replace_entry_t),flt_replace_destroy);
    array_init(&flt_replace_postpost,sizeof(flt_replace_entry_t),flt_replace_destroy);
  }

  flt_replace_replacethem(&p->content,&flt_replace_prepost,pc);
  return FLT_OK;
}
/* }}} */

/* {{{ flt_replace_handle */
int flt_replace_handle(configfile_t *cf,conf_opt_t *opt,const u_char *context,u_char **args,size_t argnum) {
  flt_replace_entry_t entry;
  const char *error = NULL;
  int erroffset = 0;

  if(flt_replace_fn == NULL) flt_replace_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(flt_replace_fn,context) != 0) return 0;

  if(flt_replace_mustinit) {
    flt_replace_mustinit = 0;
    array_init(&flt_replace_prepost,sizeof(flt_replace_entry_t),flt_replace_destroy);
    array_init(&flt_replace_postpost,sizeof(flt_replace_entry_t),flt_replace_destroy);
  }

  // Replace "subje(ct)" "replacement_$1" "pre-post"|"post-post"[ "http"]
  if((entry.pattern = pcre_compile(args[0],PCRE_UTF8|PCRE_NO_UTF8_CHECK,&error,&erroffset,NULL)) == NULL) {
    fprintf(stderr,"Error compiling regex '%s' at character %d: %s\n",args[0],erroffset,error);
    return 1;
  }

  entry.replacement = strdup(args[1]);

  if(argnum == 4) entry.http = 1;

  if(cf_strcmp(args[2],"pre-post") == 0) array_push(&flt_replace_prepost,&entry);
  else array_push(&flt_replace_postpost,&entry);

  return 0;
}
/* }}} */

/* {{{ flt_replace_finish */
void flt_replace_finish() {
  array_destroy(&flt_replace_prepost);
  array_destroy(&flt_replace_postpost);
}
/* }}} */

conf_opt_t flt_replace_config[] = {
  { "Replace", flt_replace_handle, CFG_OPT_CONFIG|CFG_OPT_LOCAL, NULL },
  { NULL, NULL, 0, NULL }
};

handler_config_t flt_replace_handlers[] = {
  { POSTING_HANDLER,  flt_replace_showpost },
  { NEW_POST_HANDLER, flt_replace_newpost },
  { 0, NULL }
};

module_config_t flt_replace = {
  MODULE_MAGIC_COOKIE,
  flt_replace_config,
  flt_replace_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  flt_replace_finish
};


/* eof */
