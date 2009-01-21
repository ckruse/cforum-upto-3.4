/**
 * \file fo_post.c
 * \author Christian Kruse, <cjk@wwwtech.de>
 * \brief Implementation of fo_post
 *
 * This file implements fo_post in C
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
#include <ctype.h>
#include <dlfcn.h>
#include <errno.h>

#include <pcre.h>
#include <sys/types.h>
#include <inttypes.h>

/* socket includes */
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/un.h>

#ifdef CF_SHARED_MEM
#include <sys/ipc.h>
#include <sys/shm.h>
#endif

#include <errno.h>
#include <signal.h>

#include "readline.h"
#include "hashlib.h"
#include "utils.h"
#include "cfgcomp.h"
#include "cfcgi.h"
#include "template.h"
#include "charconvert.h"
#include "clientlib.h"
#include "validate.h"
#include "htmllib.h"
#include "fo_post.h"
/* }}} */

/* {{{ display_finishing_screen */
/**
 * Function for displaying the user the 'Ok, posting has been processed' site
 * \param p The message struct
 */
void display_finishing_screen(cf_cfg_config_t *cfg,cf_message_t *p) {
  cf_template_t tpl;
  u_char tplname[256];
  int uname = cf_hash_get(GlobalValues,"UserName",8) != NULL;

  cf_cfg_config_value_t *tt = cf_cfg_get_value(cfg,"FP:OkTemplate"),
    *cs = cf_cfg_get_value(cfg,"DF:ExternCharset"),
    *qc = cf_cfg_get_value(cfg,"DF:QuotingChars"),
    *ps = cf_cfg_get_value(cfg,"DF:PostScript"),
    *fb = cf_cfg_get_value(cfg,"DF:BaseURL"),
    *df = cf_cfg_get_value(cfg,"FP:DateFormat"),
    *lc = cf_cfg_get_value(cfg,"DF:DateLocale"),
    *mode = cf_cfg_get_value(cfg,"DF:TemplateMode"),
    *lang = cf_cfg_get_value(cfg,"DF:Language");

  size_t len;
  u_char *val;
  cf_cl_thread_t thr;
  cf_string_t content;

  cf_gen_tpl_name(tplname,256,mode->sval,lang->sval,tt->sval);

  if(cf_tpl_init(&tpl,tplname) != 0) {
    printf("Status: 500 Internal Server Error\015\012\015\012");
    cf_error_message(cfg,"E_TPL_NOT_FOUND",NULL);
    return;
  }

  printf("\015\012");

  memset(&thr,0,sizeof(thr));
  thr.messages = thr.threadmsg = p;
  cf_str_init(&content);

  cf_set_variable(&tpl,cs->sval,"charset",cs->sval,strlen(cs->sval),1);
  cf_set_variable(&tpl,cs->sval,"new-posting-uri",ps->avals[uname].sval,strlen(ps->avals[uname].sval),1);
  cf_set_variable(&tpl,cs->sval,"forum-base-uri",fb->avals[uname].sval,strlen(fb->avals[uname].sval),1);

  cf_set_variable(&tpl,cs->sval,"Name",p->author.content,p->author.len,1);
  cf_set_variable(&tpl,cs->sval,"subject",p->subject.content,p->subject.len,1);

  if((val = cf_general_get_time(df->sval,lc->sval,&len,&p->date)) != NULL) {
    cf_set_variable(&tpl,cs->sval,"date",val,len,1);
    free(val);
  }

  /* {{{ transform body to html and set it in the template */
  cf_msg_to_html(cfg,&thr,p->content.content,&content,NULL,qc->sval,-1,1);
  cf_set_variable(&tpl,cs->sval,"body",content.content,content.len,0);
  cf_str_cleanup(&content);
  /* }}} */

  if(p->email.len)    cf_set_variable(&tpl,cs->sval,"EMail",p->email.content,p->email.len,1);
  if(p->category.len) cf_set_variable(&tpl,cs->sval,"cat",p->category.content,p->category.len,1);
  if(p->hp.len)       cf_set_variable(&tpl,cs->sval,"HomepageUrl",p->hp.content,p->hp.len,1);
  if(p->img.len)      cf_set_variable(&tpl,cs->sval,"ImageUrl",p->img.content,p->img.len,1);

  cf_tpl_parse(&tpl);
  cf_tpl_finish(&tpl);
}
/* }}} */

/* {{{ display_posting_form */
void display_posting_form(cf_cfg_config_t *cfg,cf_hash_t *head,cf_message_t *p,cf_tpl_variable_t *var) {
  /* display him the fucking formular */
  cf_template_t tpl;
  u_char tplname[256];
  int uname = cf_hash_get(GlobalValues,"UserName",8) != NULL;
  cf_cfg_config_value_t *tt  = cf_cfg_get_value(cfg,"FP:ThreadTemplate"),
    *cs  = cf_cfg_get_value(cfg,"DF:ExternCharset"),
    *cats = cf_cfg_get_value(cfg,"DF:Categories"),
    *qc = cf_cfg_get_value(cfg,"DF:QuotingChars"),
    *ps = cf_cfg_get_value(cfg,"DF:PostScript"),
    *fb = cf_cfg_get_value(cfg,"DF:BaseURL"),
    *mode = cf_cfg_get_value(cfg,"DF:TemplateMode"),
    *lang = cf_cfg_get_value(cfg,"DF:Language");
  size_t len,i;
  u_char *val;
  u_char *tmp;
  cf_tpl_variable_t array;
  u_char buff[256];

  u_char *qchars;
  size_t qclen;
  int utf8;

  cf_hash_keylist_t *key;
  cf_cgi_param_t *param;

  cf_string_t *cat = NULL;

  utf8 = cf_strcmp(cs->sval,"UTF-8") == 0;

  if(utf8 || (qchars = htmlentities_charset_convert(qc->sval,"UTF-8",cs->sval,&qclen,0)) == NULL) {
    qchars = htmlentities(qc->sval,0);
    qclen  = strlen(qchars);
  }

  cf_gen_tpl_name(tplname,256,mode->sval,lang->sval,tt->sval);

  if(cf_tpl_init(&tpl,tplname) != 0) {
    printf("Status: 500 Internal Server Error\015\012\015\012");
    cf_error_message(cfg,"E_TPL_NOT_FOUND",NULL);
    return;
  }

  /* {{{ set categories */
  cf_tpl_var_init(&array,TPL_VARIABLE_ARRAY);

  if(p) {
    if(p->category.len) cf_set_variable(&tpl,cs->sval,"cat",p->category.content,p->category.len,1);
    cf_set_variable(&tpl,cs->sval,"subject",p->subject.content,p->subject.len,1);
  }
  else if(head) {
    if((cat = cf_cgi_get(head,"cat")) != NULL) cf_set_variable(&tpl,cs->sval,"cat",cat->content,cat->len,1);
  }

  for(i=0;i<cats->alen;++i) {
    tmp = charset_convert_entities(cats->avals[i].sval,strlen(cats->avals[i].sval),"UTF-8",cs->sval,&len);
    cf_tpl_var_addvalue(&array,TPL_VARIABLE_STRING,tmp,len);
    free(tmp);
  }
  cf_tpl_setvar(&tpl,"cats",&array);
  /* }}} */

  /* {{{ set error string */
  if(*ErrorString) {
    val = cf_get_error_message(cfg,ErrorString,&len);

    if(!val) {
      val = strdup(ErrorString);
      len = strlen(val);
    }

    cf_set_variable(&tpl,cs->sval,"err",val,len,1);
    free(val);

    printf("Status: 500 Internal Server Error\015\012\015\012");
  }
  else printf("\015\012");
  /* }}} */

  /* {{{ set cgi variables */
  if(head) {
    for(key=head->keys.elems;key;key=key->next) {
      for(param = cf_hash_get(head,key->key,strlen(key->key));param;param=param->next) {
        if(param->value.content) {
          /* we don't want to have empty URLs */
          len = strlen(param->name);
          if(cf_strcasecmp(param->name+len-3,"Url") == 0) {
            if(cf_strcmp(param->value.content,"http://") == 0) continue;
          }

          if(cf_strncmp(param->name,"ne_",3) == 0) cf_set_variable(&tpl,cs->sval,param->name+3,param->value.content,param->value.len,0);
          else cf_set_variable(&tpl,cs->sval,param->name,param->value.content,param->value.len,1);
        }
      }
    }
  }
  /* }}} */

  len = cf_gen_unid(buff,50);

  cf_set_variable(&tpl,cs->sval,"charset",cs->sval,strlen(cs->sval),1);
  cf_set_variable(&tpl,cs->sval,"script",ps->avals[uname].sval,strlen(ps->avals[uname].sval),1); //TODO: new-posting-uri
  cf_set_variable(&tpl,cs->sval,"forumbase",fb->avals[uname].sval,strlen(fb->avals[uname].sval),1); //TODO: forum-base-uri
  cf_set_variable(&tpl,cs->sval,"unid",buff,len,1);
  cf_tpl_setvalue(&tpl,"qchar",TPL_VARIABLE_STRING,"&#255;",6);
  cf_tpl_appendvalue(&tpl,"qchar",qchars,qclen);

  if(var) {
    cf_tpl_setvalue(&tpl,"err",TPL_VARIABLE_INT,1);
    cf_tpl_setvar(&tpl,"errs",var);
  }

  cf_run_post_display_handlers(cfg,head,&tpl,p);

  cf_tpl_parse(&tpl);

  cf_tpl_finish(&tpl);
}
/* }}} */

/* {{{ normalize_cgi_variables */
/**
 * "Normalizes" the cgi input variables (means: everything to UTF-8)
 * and checks if strings are valid UTF-8 (in case of UTF-8 cgi params)
 * \param head The cgi hash
 * \param field_name The name of the field containing &#255;
 * \return 0 on success, -1 on failure
 */
int normalize_cgi_variables(cf_cfg_config_t *cfg,cf_hash_t *head,const u_char *field_name) {
  size_t flen,len,i;
  cf_string_t *field = cf_cgi_get(head,(u_char *)field_name);
  register u_char *ptr;
  u_char c;
  u_char *converted;
  cf_cfg_config_value_t *cs = cf_cfg_get_value(cfg,"DF:ExternCharset");
  cf_cgi_param_t *param;
  char *buff;
  cf_string_t str,*str1;
  cf_hash_keylist_t *key;
  u_int32_t num;

  if(!field) return -1;

  /* In UTF-8 &#255; is xC3xBF, so if first char is not \194 it is not UTF-8 */
  if(*field->content != 0xC3 || *(field->content+1) != 0xBF) {
    /* {{{ transform everything to utf-8... */
    for(key=head->keys.elems;key;key=key->next) {
      for(param = cf_cgi_get_multiple(head,key->key);param;param=param->next) {
        if((converted = charset_convert(param->value.content,param->value.len,cs->sval,"UTF-8",NULL)) == NULL) return -1;

        for(ptr=converted,len=strlen(converted);*ptr;) {
          if((i = utf8_to_unicode(ptr,len,&num)) <= 0) return -2;

          if(num == 0x9 || num == 0xA || num == 0xD || (num >= 0x20 && num <= 0xD7FF) || (num >= 0xE000 && num <= 0xFFFD) || (num >= 0x10000 && num <= 0x10FFFF)) {
            /* valid character, but may be a discouraged character as defined in http://www.w3.org/TR/REC-xml/#charsets */
            if(
               (num >= 0x7F && num <= 0x84) ||
               (num >= 0x86 && num <= 0x9F) ||
               (num >= 0xFDD0 && num <= 0xFDDF) ||
               (num >= 0x1FFFE && num <= 0x1FFFF) ||
               (num >= 0x2FFFE && num <= 0x2FFFF) ||
               (num >= 0x3FFFE && num <= 0x3FFFF) ||
               (num >= 0x4FFFE && num <= 0x4FFFF) ||
               (num >= 0x5FFFE && num <= 0x5FFFF) ||
               (num >= 0x6FFFE && num <= 0x6FFFF) ||
               (num >= 0x7FFFE && num <= 0x7FFFF) ||
               (num >= 0x8FFFE && num <= 0x8FFFF) ||
               (num >= 0x9FFFE && num <= 0x9FFFF) ||
               (num >= 0xAFFFE && num <= 0xAFFFF) ||
               (num >= 0xBFFFE && num <= 0xBFFFF) ||
               (num >= 0xCFFFE && num <= 0xCFFFF) ||
               (num >= 0xDFFFE && num <= 0xDFFFF) ||
               (num >= 0xEFFFE && num <= 0xEFFFF) ||
               (num >= 0xFFFFE && num <= 0xFFFFF) ||
               (num >= 0x10FFFE && num <= 0x10FFFF)
               ) {
              strcpy(ErrorString,"E_BADCHAR");
              return -2;
            }
          }
          /* invalid character as defined in http://www.w3.org/TR/REC-xml/#charsets */
          else {
            strcpy(ErrorString,"E_INVALID_CHAR");
            return -2;
          }

          len -= i;
          ptr += i;
        }

        /* {{{ removed unicode whitespaces */
        cf_str_init(&str);
        for(ptr=converted;*ptr;++ptr) {
          // \xC2\xA0 is nbsp
          if(cf_strncmp(ptr,"\xC2\xA0",2) == 0) {
            cf_str_char_append(&str,' ');
            ++ptr;
          }
          // \xE2\x80[\x80-\x8B\xA8-\xAF] are unicode whitespaces
          else if(cf_strncmp(ptr,"\xE2\x80",2) == 0) {
            c = *(ptr+3);
            if((c >= 0x80 && c <= 0x8B) || (c >= 0xA8 && c <= 0xAF)) {
              cf_str_char_append(&str,' ');
              ptr += 2;
            }
            else cf_str_char_append(&str,*ptr);
          }
          else cf_str_char_append(&str,*ptr);
        }
        /* }}} */

        cf_str_cleanup(&param->value);
        param->value.content = str.content;
        param->value.len     = str.len;
        param->value.reserved= str.reserved;

        free(converted);
      }
    }
    /* }}} */
  }
  else {
    /* {{{ input seems to be UTF-8, check if strings are valid UTF-8 */
    for(key=head->keys.elems;key;key=key->next) {
      for(param = cf_cgi_get_multiple(head,key->key);param;param=param->next) {
        for(ptr=param->value.content,len=param->value.len;*ptr;) {
          if((i = utf8_to_unicode(ptr,len,&num)) <= 0) return -2;

          if(num == 0x9 || num == 0xA || num == 0xD || (num >= 0x20 && num <= 0xD7FF) || (num >= 0xE000 && num <= 0xFFFD) || (num >= 0x10000 && num <= 0x10FFFF)) {
            /* valid character, but may be a discouraged character as defined in http://www.w3.org/TR/REC-xml/#charsets */
            if(
               (num >= 0x7F && num <= 0x84) ||
               (num >= 0x86 && num <= 0x9F) ||
               (num >= 0xFDD0 && num <= 0xFDDF) ||
               (num >= 0x1FFFE && num <= 0x1FFFF) ||
               (num >= 0x2FFFE && num <= 0x2FFFF) ||
               (num >= 0x3FFFE && num <= 0x3FFFF) ||
               (num >= 0x4FFFE && num <= 0x4FFFF) ||
               (num >= 0x5FFFE && num <= 0x5FFFF) ||
               (num >= 0x6FFFE && num <= 0x6FFFF) ||
               (num >= 0x7FFFE && num <= 0x7FFFF) ||
               (num >= 0x8FFFE && num <= 0x8FFFF) ||
               (num >= 0x9FFFE && num <= 0x9FFFF) ||
               (num >= 0xAFFFE && num <= 0xAFFFF) ||
               (num >= 0xBFFFE && num <= 0xBFFFF) ||
               (num >= 0xCFFFE && num <= 0xCFFFF) ||
               (num >= 0xDFFFE && num <= 0xDFFFF) ||
               (num >= 0xEFFFE && num <= 0xEFFFF) ||
               (num >= 0xFFFFE && num <= 0xFFFFF) ||
               (num >= 0x10FFFE && num <= 0x10FFFF)
               ) {
              strcpy(ErrorString,"E_BADCHAR");
              return -2;
            }
          }
          /* invalid character as defined in http://www.w3.org/TR/REC-xml/#charsets */
          else {
            strcpy(ErrorString,"E_INVALID_CHAR");
            return -2;
          }

          len -= i;
          ptr += i;
        }

        /* {{{ removed unicode whitespaces */
        cf_str_init(&str);
        for(ptr=param->value.content;*ptr;++ptr) {
          // \xC2\xA0 is nbsp
          if(cf_strncmp(ptr,"\xC2\xA0",2) == 0) {
            cf_str_char_append(&str,' ');
            ++ptr;
          }
          // \xE2\x80[\x80-\x8B\xA8-\xAF] are unicode whitespaces
          else if(cf_strncmp(ptr,"\xE2\x80",2) == 0) {
            c = *(ptr+3);
            if((c >= 0x80 && c <= 0x8B) || (c >= 0xA8 && c <= 0xAF)) {
              cf_str_char_append(&str,' ');
              ptr += 2;
            }
            else cf_str_char_append(&str,*ptr);
          }
          else cf_str_char_append(&str,*ptr);
        }
        /* }}} */

        cf_str_cleanup(&param->value);
        param->value.content = str.content;
        param->value.len     = str.len;
        param->value.reserved= str.reserved;
      }
    }
    /* }}} */
  }

  if((str1 = cf_cgi_get(head,(u_char *)field_name)) != NULL) {
    flen = str1->len;
    buff = cf_alloc(NULL,1,flen-2,CF_ALLOC_MALLOC);

    /* strip character from field */
    memcpy(buff,str1->content+2,flen-2);

    free(str1->content);

    str1->content[flen-2] = '\0';
    str1->content = buff;
    str1->len     = flen-2;
    str1->reserved= flen-2;
  }

  return 0;
}

/* }}} */

/* {{{ validate_cgi_variables */
/**
 * Validates the input cgi variables as defined in configuration
 * \param head The cgi hash
 * \return -1 on failure, 0 on success
 */
int validate_cgi_variables(cf_cfg_config_t *conf,cf_hash_t *head) {
  cf_cfg_config_value_t *cfg;
  pcre *regexp;
  u_char *error;
  int erroffset,negate=0;
  size_t i;

  size_t maxlen,minlen,len;
  int fupto = cf_cgi_get(head,"fupto") != NULL,ret = -1;

  cf_string_t *value;

  /* {{{ check if every needed field exists */
  if((cfg = cf_cfg_get_value(conf,"FP:FieldNeeded")) != NULL) {
    for(i=0;i<cfg->alen;++i) {
      if((value = cf_cgi_get(head,cfg->avals[i].avals[0].sval)) == NULL || *value->content == '\0') {
        /*
         * ok, value doesn't exist. But it may be that it does not need
         * to exist in a particular case (FieldNeeded has two parameters)
         */
        if((cf_strcmp(cfg->avals[i].avals[2].sval,"yes") == 0 && fupto) || (cf_strcmp(cfg->avals[i].avals[1].sval,"yes") == 0 && !fupto)) {
          snprintf(ErrorString,50,"E_%s_missing",cfg->avals[i].avals[0].sval);
          return -1;
        }
      }
    }
  }
  /* }}} */

  /* {{{ check if every field is valid in length */
  if((cfg = cf_cfg_get_value(conf,"FP:FieldConfig")) != NULL) {
    for(i=0;i<cfg->alen;++i) {
      if((value = cf_cgi_get(head,cfg->avals[i].avals[0].sval)) != NULL) {
        maxlen = cfg->avals[i].avals[1].ival;
        minlen = cfg->avals[i].avals[2].ival;

        len = cf_strlen_utf8_wo_space(value->content,value->len);
        //len = cf_strlen_utf8(value,strlen(value));
        if(maxlen && len > maxlen) {
          snprintf(ErrorString,50,"E_%s_long",cfg->avals[i].avals[0].sval);
          return -1;
        }

        /* bad boy, string is to short */
        if(minlen && len < minlen) {
          snprintf(ErrorString,50,"E_%s_short",cfg->avals[i].avals[0].sval);
          return -1;
        }
      }
    }
  }
  /* }}} */

  /* {{{ Check if every field is valid by validation function */
  if((cfg = cf_cfg_get_value(conf,"FP:FieldValidate")) != NULL) {
    for(i=0;i<cfg->alen;++i) {
      if((value = cf_cgi_get(head,cfg->avals[i].avals[0].sval)) != NULL) {
        /* {{{ ignore default values for URLs */
        if(cf_strcmp(value->content,"http://") == 0) {
          len = strlen(cfg->avals[i].avals[0].sval);
          if(cf_strcasecmp(cfg->avals[i].avals[0].sval+len-3,"url") == 0) continue;
        }
        /* }}} */

        if(*cfg->avals[i].avals[1].sval == '!') negate = 1;

        switch(*(cfg->avals[i].avals[1].sval+negate)) {
          case 'e':
            ret = is_valid_mailaddress(value->content);
            break;

          case 'h':
            if(cf_strcmp(cfg->avals[i].avals[1].sval,"http-strict") == 0) ret = is_valid_http_link(value->content,1);
            else ret = is_valid_http_link(value->content,0);
            break;

          case 'u':
            ret = is_valid_link(value->content);
            break;

          default:
            if((regexp = pcre_compile(cfg->avals[i].avals[1].sval,0,(const char **)&error,&erroffset,NULL)) == NULL) {
              fprintf(stderr,"Error in pattern '%s': %s\n",cfg->avals[i].avals[1].sval,error);
              ret = -1;
              break;
            }

            if(pcre_exec(regexp,NULL,value->content,value->len,0,0,NULL,0) < 0) ret = -1;
            else ret = 0;
            pcre_free(regexp);

            break;
        }

        /*
         * negative result and negate = 0 => error.
         * positive result and negate = 1 => error.
         */
        if((ret == -1 && negate == 0) || (ret != -1 && negate == 1)) {
          snprintf(ErrorString,50,"E_%s_invalid",cfg->avals[i].avals[0].sval);
          return -1;
        }
      }
    }
  }
  /* }}} */

  /* everything is fine */
  return 0;
}
/* }}} */

/* {{{ body_plain2coded */
cf_string_t *body_plain2coded(const u_char *qchars_p,const u_char *text) {
  u_char *body;
  cf_string_t *str = cf_alloc(NULL,1,sizeof(*str),CF_ALLOC_CALLOC);
  u_char *ptr,*end,*qchars;
  size_t len;
  int sig = 0;

  cf_str_init(str);

  qchars = htmlentities(qchars_p,0);
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
        cf_str_char_append(str,'\012');

        if(cf_strncmp(ptr+1,qchars,len) == 0) {
          cf_str_char_append(str,(u_char)127);
          ptr += len;
        }

        break;

      default:
        if(ptr == body && cf_strncmp(ptr,qchars,len) == 0) {
          cf_str_char_append(str,(u_char)127);
          ptr += len - 1;
        }
        else cf_str_char_append(str,*ptr);
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
  cf_str_init(str);

  for(ptr=body;*ptr;ptr++) {
    if(cf_strncmp(ptr,"\012-- \012",5) == 0 && sig == 0 && strstr(ptr+1,"\012-- \012") == NULL) { /* we prereserve \n-- \n for sig handling; special cases suck! */
      sig = 1;
      ptr += 4;
      cf_str_chars_append(str,"<br />-- <br />",15);
    }
    else if(*ptr == '\012') cf_str_chars_append(str,"<br />",6);
    else if(isspace(*ptr)) {
      for(end=ptr;*end && isspace(*end) && *end != '\012';++end);

      /* strip newlines at the end of the line */
      if(*end == '\012') {
        ptr = end - 1;
        continue;
      }

      if(end-ptr == 1 && (ptr - 1 >= body && *(ptr-1) != '\012')) {
        cf_str_char_append(str,' ');
        continue;
      }

      /* transform spaces after newlines to &nbsp; */
      for(len=0;len<(size_t)(end-ptr);++len) cf_str_chars_append(str,"\xC2\xA0",2);

      ptr = end - 1;
    }
    else cf_str_char_append(str,*ptr);
  }

  free(qchars);

  /* all phases finished, body has been normalized */
  return str;
}
/* }}} */

/* {{{ get_remote_addr */
/**
 * Tries to get the remote address of the user
 * \return The remote address or a fake address of 666.666.666.666
 */
u_char *get_remote_addr(void) {
  u_char *tmp;

  if((tmp = getenv("HTTP_X_FORWARDED_FOR")) == NULL) {
    if((tmp = getenv("REMOTE_ADDR")) == NULL) tmp = "666.666.666.666";
  }

  return tmp;
}
/* }}} */

/* {{{ get_thread */
int get_thread(cf_cfg_config_t *cfg,cf_cl_thread_t *thr,cf_hash_t *head,int overwritten) {
  u_char *tmp;
  u_int64_t tid,mid;
  #ifndef CF_SHARED_MEM
  int sock;
  rline_t rl;
  cf_cfg_config_value_t *cfg_sock = cf_cfg_get_value(cfg,"DF:SocketName");
  #else
  cf_cfg_config_value_t *shminf = cf_cfg_get_value(cfg,"DF:SharedMemIds");
  int shmids[3] = { shminf->avals[0].ival,shminf->avals[1].ival,shminf->avals[2].ival };
  void *shm;
  #endif

  cf_string_t *tidmid,*val;

  if((tidmid = cf_cgi_get(head,"fupto")) != NULL) {
    #ifndef CF_SHARED_MEM
    memset(&rl,0,sizeof(rl));
    #endif

    if(((val = cf_cgi_get(head,"a")) != NULL && cf_strcmp(val->content,"answer") == 0) || overwritten) {
      tmp = strstr(tidmid->content,",");
      tid = cf_str_to_uint64(tidmid->content);
      mid = cf_str_to_uint64(tmp+1);

      if(tid && mid) {
        #ifdef CF_SHARED_MEM
        if((shm = cf_get_shm_ptr(shmids)) == NULL) return -1;
        #else
        if((sock = cf_socket_setup(cfg_sock->sval)) == -1) return -1;
        #endif
        else {
          #ifdef CF_SHARED_MEM
          if(cf_get_message_through_shm(shmids,shm,thr,tid,mid,CF_KILL_DELETED) == -1) return -1;
          #else
          if(cf_get_message_through_sock(sock,&rl,thr,tid,mid,CF_KILL_DELETED) == -1) return -1;
          #endif
          else return 0;
        }
      }
    }
  }

  return -1;
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

    fprintf(fd,"fo_post: Got signal %s!\nUsername: %s\nQuery-String: %s\n----\n",buff,uname?uname:(u_char *)"(null)",qs?qs:(u_char *)"(null)");
    fclose(fd);
  }

  exit(0);
}
/* }}} */

/**
 * The main function of the forum poster. No command line switches used.
 * \param argc The argument count
 * \param argv The argument vector
 * \param env The environment vector
 * \return EXIT_SUCCESS on success, EXIT_FAILURE on error
 */
int main(int argc,char *argv[],char *env[]) {
  static const u_char *wanted[] = {
    "fo_default", "fo_post"
  };

  int ret,work_on_post = 1;
  u_char  *ucfg,buff[256],*forum_name,*rqmethod;
  cf_hash_t *head;
  cf_cfg_config_value_t *cs = NULL,*cfg_val,*socknam,*qchars;
  u_char *UserName,*link,*tmp;
  cf_cl_thread_t thr;
  cf_message_t *p;

  cf_tpl_variable_t var;

  size_t len;

  cf_string_t *str,str1,*val;

  cf_string_t *tidmid;
  u_int64_t tid = 0,mid = 0;

  #ifdef CF_SHARED_MEM
  cf_cfg_config_value_t *shminf;
  int shmids[3];
  void *shm;
  #endif

  int sock,new_thread = 0,ShowInvisible = 0;
  rline_t rl;

  cf_readmode_t rm_infos;

  cf_cfg_config_t cfg;

  /* set signal handler for SIGSEGV (for error reporting) */
  signal(SIGSEGV,sighandler);
  signal(SIGILL,sighandler);
  signal(SIGFPE,sighandler);
  signal(SIGBUS,sighandler);

  /* call initialization routines */
  cf_init();
  cf_htmllib_init();

  if((forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10)) == NULL) {
    fprintf(stderr,"Could not get forum name!");
    return EXIT_FAILURE;
  }

  memset(&rl,0,sizeof(rl));
  memset(&thr,0,sizeof(thr));
  cf_str_init(&str1);

  sock = 0;

  #ifdef CF_SHARED_MEM
  shm = NULL;
  #endif

  ret  = FLT_OK;

  if(cf_cfg_get_conf(&cfg,wanted,2) != 0) {
    fprintf(stderr,"Config error!\n");
    return EXIT_FAILURE;
  }

  /* parse CGI */
  head = cf_cgi_new();

  /* first action: authorization modules */
  ret = cf_run_auth_handlers(&cfg,head);

  if((UserName = cf_hash_get(GlobalValues,"UserName",8)) != NULL) {
    /* get user config */
    if((ucfg = cf_get_uconf_name(socknam->sval,UserName)) != NULL) {
      if(cf_cfg_read_conffile(&cfg,ucfg) != 0) {
        fprintf(stderr,"config file error!\n");
        return EXIT_FAILURE;
      }
    }
  }

  /* first state: let the begin-filters run! :-) */
  if(ret != FLT_EXIT) ret = cf_run_init_handlers(&cfg,head);

  cs     = cf_cfg_get_value(&cfg,"DF:ExternCharset");
  qchars = cf_cfg_get_value(&cfg,"DF:QuotingChars");
  socknam = cf_cfg_get_value(&cfg,"DF:SocketName");

  #ifdef CF_SHARED_MEM
  shminf = cf_cfg_get_value(&cfg,"DF:SharedMemIds");
  shmids[0] = shminf->avals[0].ival;
  shmids[1] = shminf->avals[1].ival;
  shmids[2] = shminf->avals[2].ival;
  #endif

  /* {{{ get readmode information */
  if(ret != FLT_EXIT) {
    memset(&rm_infos,0,sizeof(rm_infos));
    if(cf_run_readmode_collectors(&cfg,head,&rm_infos) != FLT_OK) {
      printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->sval);
      cf_error_message(&cfg,"E_CONFIG_ERR",NULL);
      ret = FLT_EXIT;
    }
    else cf_hash_set(GlobalValues,"RM",2,&rm_infos,sizeof(rm_infos));
  }
  /* }}} */

  ShowInvisible = cf_hash_get(GlobalValues,"ShowInvisible",13) != NULL;

  if(ret != FLT_EXIT) {
    /* fine -- lets spit out http headers */
    printf("Content-Type: text/html; charset=%s\015\012",cs->sval);

    /* ok -- lets check if user want's to post or just want's to fill out the form */
    if(head) {
      if((rqmethod = getenv("REQUEST_METHOD")) == NULL || cf_strcmp(rqmethod,"GET") == 0) {
        if(cf_cgi_get(head,"unid") == NULL) work_on_post = 0;
      }

      if(work_on_post) {
        /* {{{ ok, user gave us variables -- lets normalize them */
        *ErrorString = '\0';
        if(normalize_cgi_variables(&cfg,head,"qchar") != 0) {
          if(get_thread(&cfg,&thr,head,0) == -1) {
            if(*ErrorString == '\0') strcpy(ErrorString,"E_manipulated");
            display_posting_form(&cfg,head,NULL,NULL);
          }
          else {
            *ErrorString = '\0';
            display_posting_form(&cfg,head,thr.threadmsg,NULL);
            cf_cleanup_thread(&thr);
          }

          return EXIT_SUCCESS;
        }
        /* }}} */

        /* {{{ everything seems to be fine, so lets validate user input */
        if(validate_cgi_variables(&cfg,head) != 0) {
          if(get_thread(&cfg,&thr,head,1) == -1) display_posting_form(&cfg,head,NULL,NULL);
          else {
            *ErrorString = '\0';
            display_posting_form(&cfg,head,thr.threadmsg,NULL);
            cf_cleanup_thread(&thr);
          }

          return EXIT_SUCCESS;
        }
        /* }}} */

        /* {{{ go and normalize the posting body */
        val = cf_cgi_get(head,"body");
        str = body_plain2coded(qchars->sval,val->content);
        /* }}} */

        /* lets validate the posting body */
        if(cf_cgi_get(head,"validate") == NULL) {
          cf_tpl_var_init(&var,TPL_VARIABLE_ARRAY);

          if((ret = cf_validate_msg(&cfg,NULL,str->content,&var)) == FLT_ERROR) {
            cf_cgi_set(head,"validate","no",3);

            if(get_thread(&cfg,&thr,head,1) == -1) display_posting_form(&cfg,head,NULL,&var);
            else display_posting_form(&cfg,head,thr.threadmsg,&var);

            return EXIT_SUCCESS;
          }
        }

        /* {{{ get thread id and message id (if given) */
        if((tidmid = cf_cgi_get(head,"fupto")) != NULL) {
          tmp = strstr(tidmid->content,",");
          tid = cf_str_to_uint64(tidmid->content);
          mid = cf_str_to_uint64(tmp+1);

          if(!tid || !mid) {
            strcpy(ErrorString,"E_manipulated");
            display_posting_form(&cfg,head,NULL,NULL);
            return EXIT_SUCCESS;
          }
        }
        else new_thread = 1;
        /* }}} */

        /* {{{ ok, everythings fine: all fields, all fields are long enough. Go and get parent posting */
        #ifdef CF_SHARED_MEM
        if((shm = cf_get_shm_ptr(shmids)) == NULL) {
          printf("Status: 500 Internal Server Error\015\012\015\012");
          cf_error_message(&cfg,"E_NO_CONN",NULL,strerror(errno));
          return EXIT_SUCCESS;
        }
        #else
        /* fatal error: could not connect */
        if((sock = cf_socket_setup(socknam->sval)) == -1) {
          printf("Status: 500 Internal Server Error\015\012\015\012");
          cf_error_message(&cfg,ErrorString,NULL);
          return EXIT_SUCCESS;
        }
        #endif

        #ifdef CF_SHARED_MEM
        if(new_thread == 0 && cf_get_message_through_shm(shmids,shm,&thr,tid,mid,ShowInvisible ? CF_KEEP_DELETED : CF_KILL_DELETED) == -1) {
          printf("Status: 500 Internal Server Error\015\012\015\012");
          cf_error_message(&cfg,ErrorString,NULL);
          return EXIT_SUCCESS;
        }
        #else
        if(new_thread == 0 && cf_get_message_through_sock(sock,&rl,&thr,tid,mid,ShowInvisible ? CF_KEEP_DELETED : CF_KILL_DELETED) == -1) {
          printf("Status: 500 Internal Server Error\015\012\015\012");
          cf_error_message(&cfg,ErrorString,NULL);
          return EXIT_SUCCESS;
        }
        #endif
        /* }}} */

        p = cf_alloc(NULL,1,sizeof(*p),CF_ALLOC_CALLOC);

        /* {{{ set userdata to posting */
        val = cf_cgi_get(head,"Name");
        cf_str_str_set(&p->author,val);

        p->content.content = str->content;
        p->content.len     = str->len;

        free(str);

        /* we inherit subject if none given */
        if((val = cf_cgi_get(head,"subject")) == NULL) cf_str_str_set(&p->subject,&thr.threadmsg->subject);
        else cf_str_str_set(&p->subject,val);

        /* we inherit category */
        if((val = cf_cgi_get(head,"cat")) == NULL) cf_str_str_set(&p->category,&thr.threadmsg->category);
        else cf_str_str_set(&p->category,val);

        if((val = cf_cgi_get(head,"EMail")) != NULL) cf_str_str_set(&p->email,val);
        if((val = cf_cgi_get(head,"HomepageUrl")) != NULL) {
          if(cf_strcmp(val->content,"http://") != 0) cf_str_str_set(&p->hp,val);
        }
        if((val = cf_cgi_get(head,"ImageUrl")) != NULL) {
          if(cf_strcmp(val->content,"http://") != 0) cf_str_str_set(&p->img,val);
        }

        p->date        = time(NULL);
        if(new_thread == 0) p->level = thr.threadmsg->level + 1;
        p->may_show    = 1;

        if(new_thread == 0 && thr.threadmsg->invisible) p->invisible = 1;
        else p->invisible = 0;
        /* }}} */

        #ifdef CF_SHARED_MEM
        /* filters finished... send posting */
        if((sock = cf_socket_setup(socknam->sval)) == -1) {
          printf("Status: 500 Internal Server Error\015\012\015\012");
          cf_error_message(&cfg,"E_NO_SOCK",NULL,strerror(errno));
          return EXIT_SUCCESS;
        }
        #endif

        /* ok, we did everything we had to do, let filters run */
        #ifndef CF_SHARED_MEM
        if(cf_run_post_filters(&cfg,head,p,new_thread?NULL:&thr,sock) != FLT_EXIT)
        #else
        if(cf_run_post_filters(&cfg,head,p,new_thread?NULL:&thr,shm,sock) != FLT_EXIT)
        #endif
        {
          /* {{{ submit posting */
          len = snprintf(buff,256,"SELECT %s\n",forum_name);
          writen(sock,buff,len);
          if((tmp = readline(sock,&rl)) == NULL) {
            printf("Status: 500 Internal Server Error\015\012\015\012");
            cf_error_message(&cfg,"E_IO_ERR",NULL);
            return EXIT_SUCCESS;
          }

          if(cf_strncmp(tmp,"200",3) != 0) {
            ret = atoi(tmp);
            free(tmp);
            if(ret) len = snprintf(buff,256,"E_FO_%d",ret);
            else len = snprintf(buff,256,"E_IO_ERR");

            printf("Status: 500 Internal Server Error\015\012\015\012");
            cf_error_message(&cfg,buff,NULL);
            return EXIT_SUCCESS;
          }

          free(tmp);

          if(tid && mid) len = snprintf(buff,256,"POST ANSWER t=%" PRIu64 " m=%" PRIu64 "\n",tid,mid);
          else           len = snprintf(buff,256,"POST THREAD\n");

          cf_str_chars_append(&str1,buff,len);

          val = cf_cgi_get(head,"unid");
          cf_str_chars_append(&str1,"Unid: ",6);
          cf_str_str_append(&str1,val);

          if(UserName) {
            cf_str_chars_append(&str1,"\nFlag: UserName=",16);
            cf_str_chars_append(&str1,UserName,strlen(UserName));
          }

          cf_str_chars_append(&str1,"\nAuthor: ",9);
          cf_str_str_append(&str1,&p->author);

          cf_str_chars_append(&str1,"\nSubject: ",10);
          cf_str_str_append(&str1,&p->subject);

          if(p->email.len) {
            cf_str_chars_append(&str1,"\nEMail: ",8);
            cf_str_str_append(&str1,&p->email);
          }

          if(p->category.len) {
            cf_str_chars_append(&str1,"\nCategory: ",11);
            cf_str_str_append(&str1,&p->category);
          }

          if(p->hp.len) {
            if(cf_strcmp(p->hp.content,"http://")) {
              cf_str_chars_append(&str1,"\nHomepageUrl: ",14);
              cf_str_str_append(&str1,&p->hp);
            }
          }

          if(p->img.len) {
            if(cf_strcmp(p->img.content,"http://")) {
              cf_str_chars_append(&str1,"\nImageUrl: ",11);
              cf_str_str_append(&str1,&p->img);
            }
          }

          cf_str_chars_append(&str1,"\nBody: ",7);
          cf_str_str_append(&str1,&p->content);

          tmp = get_remote_addr();
          cf_str_chars_append(&str1,"\nRemoteAddr: ",13);
          cf_str_chars_append(&str1,tmp,strlen(tmp));

          if(p->invisible) cf_str_chars_append(&str1,"\nInvisible: 1",13);

          cf_str_chars_append(&str1,"\n\n",2);

          /* now: transmit everything */
          writen(sock,str1.content,str1.len);
          cf_str_cleanup(&str1);
          /* }}} */

          /* ok, everything has been submitted to the forum. Now lets wait for an answer... */
          if((tmp = readline(sock,&rl)) == NULL) {
            printf("Status: 500 Internal Server Error\015\012\015\012");
            cf_error_message(&cfg,"E_IO_ERR",NULL);
            return EXIT_SUCCESS;
          }

          if(cf_strncmp(tmp,"200",3) != 0) {
            fprintf(stderr,"Forum returned: %s\n",tmp);
            ret = atoi(tmp);
            if(tmp) free(tmp);
            len = snprintf(buff,256,"E_FO_%d",ret);
            printf("Status: 500 Internal Server Error\015\012\015\012");
            cf_error_message(&cfg,buff,NULL);
            return EXIT_SUCCESS;
          }

          free(tmp);

          /* {{{ yeah, posting has ben processed! now, get the new message id and the (new?) thread id */
          if((tmp = readline(sock,&rl)) != NULL) {
            tid = cf_str_to_uint64(tmp+5);
            free(tmp);

            if((tmp = readline(sock,&rl)) != NULL) {
              mid = cf_str_to_uint64(tmp+5);
              free(tmp);
            }
            else mid = 0;

            if((tmp = readline(sock,&rl)) != NULL) free(tmp);

            cfg_val = cf_cfg_get_value(&cfg,"RedirectOnPost");

            p->mid = mid;
            #ifdef CF_SHARED_MEM
            cf_run_after_post_handlers(&cfg,head,p,tid,shm,sock);
            #else
            cf_run_after_post_handlers(&cfg,head,p,tid,sock);
            #endif

            writen(sock,"QUIT\n",5);

            if(cfg_val && cf_strcmp(cfg_val->sval,"yes") == 0) {
              link = cf_get_link(rm_infos.posting_uri[UserName?1:0],tid,mid);
              cf_http_redirect_with_nice_uri(link,0);
              free(link);
            }
            else display_finishing_screen(&cfg,p);

            cf_cleanup_message(p);
            free(p);

            close(sock);
          }
          /* }}} */
        }
      }
      else display_posting_form(&cfg,head,NULL,NULL);
    }
    else display_posting_form(&cfg,head,NULL,NULL);
  }

  /* cleanup source */
  cf_cfg_config_destroy(&cfg);
  cf_fini();

  if(head) cf_hash_destroy(head);

  #ifdef CF_SHARED_MEM
  if(shm) shmdt((void *)shm);
  #endif

  return EXIT_SUCCESS;
}

/* eof */
