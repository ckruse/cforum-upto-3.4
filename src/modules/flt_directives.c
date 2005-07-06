/**
 * \file flt_directives.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 *
 * This plugin handles lot of standard directives
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
#include "htmllib.h"
/* }}} */

static u_char *flt_directives_extlink   = NULL;
static u_char *flt_directives_link      = NULL;
static u_char *flt_directives_icons     = NULL;
static u_char **flt_directives_badlinks = NULL;
static size_t flt_directives_bdl_len    = 0;
static int flt_directives_lit           = 0;
static int flt_directives_imagesaslink  = 0;
static int flt_directives_iframesaslink = 0;
static int flt_directives_rel_no_follow = 0;
static int flt_directives_wbl           = 0;
static int flt_directives_suial         = 0;
static int flt_directives_rpl           = 0;

typedef struct {
  u_char *id;
  u_char *uri;
} t_flt_directives_ref_uri;


#define FLT_DIRECTIVES_TOK_TITLE  0
#define FLT_DIRECTIVES_TOK_URI   -1
#define FLT_DIRECTIVES_TOK_ID    -2

typedef struct {
  int type;
  u_char *tok;
} t_flt_directives_lt_tok;

typedef struct {
  pcre *re;
  pcre_extra *extra;
} flt_directives_re;

static t_array flt_directives_ref_uris = { 0, 0, 0, NULL, NULL };
static t_array flt_directives_lt_toks  = { 0, 0, 0, NULL, NULL };
static t_array flt_directives_puris = { 0, 0, 0, NULL, NULL };

static u_char *flt_directives_fname = NULL;

/* {{{ flt_directives_is_valid_pref */
int flt_directives_is_valid_pref(const u_char *parameter,u_char **tmp,u_char **tmp1) {
  u_char *ptr;

  /* strict syntax checking for [pref:], necessary because of some idiots */
  if(*parameter == 't') {
    if(*(parameter+1) == '=') {
      for(ptr=(u_char *)parameter+2;*ptr && isdigit(*ptr);++ptr);

      if(*ptr++ == ';') {
        if(*ptr++ == 'm') {
          if(*ptr++ == '=') {
            *tmp = ptr;

            for(;*ptr && isdigit(*ptr);++ptr);

            if(cf_strncmp(ptr,"@title=",7) == 0) {
              if(*(ptr+7) && flt_directives_is_valid_title(ptr+7,strlen(ptr+7))) {
                *tmp1 = ptr;
                return 1;
              }
              else return 0;
            }

            *tmp1 = NULL;
            if(*ptr == '\0') return 1;
          }
        }
      }
    }
  }

  return 0;
}
/* }}} */

/* {{{ flt_directives_is_relative_uri */
int flt_directives_is_relative_uri(const u_char *tmp,size_t len) {
  t_string str;
  int ret = 0;

  str_init(&str);
  str_char_set(&str,"http://www.example.org",21);
  if(*tmp == '.' || *tmp == '?') str_char_append(&str,'/');
  str_chars_append(&str,tmp,len);

  ret = is_valid_http_link(str.content,0);
  str_cleanup(&str);

  return ret == 0;
}
/* }}} */

/* {{{ flt_directives_is_valid_title */
int flt_directives_is_valid_title(const u_char *title,size_t len) {
  int bytes = 0;
  register u_char *ptr;
  u_int32_t num;

  for(ptr=(u_char *)title;*ptr;) {
    if((bytes = utf8_to_unicode(ptr,len,&num)) < 0) {
      ++ptr;
      --len;
    }

    if(cf_isspace(num) == 0) return 1;

    ptr += bytes;
    len -= bytes;
  }

  return 0;
}
/* }}} */

/* {{{ flt_directives_replace */
void flt_directives_replace(t_string *content,const u_char *str,const u_char *uri,size_t ulen,const u_char *escaped,size_t eulen,const u_char *title,size_t tlen) {
  register u_char *ptr;

  for(ptr=(u_char *)str;*ptr;++ptr) {
    switch(*ptr) {
      case '%':
        switch(*(ptr+1)) {
          case 't':
            if(title) str_chars_append(content,title,tlen);
            else str_chars_append(content,uri,ulen);
            break;
          case 'u':
            str_chars_append(content,uri,ulen);
            break;
          case 'e':
            str_chars_append(content,escaped,eulen);
            break;
          default:
            str_char_append(content,'%');
            continue;
        }
        ++ptr;
        break;

      default:
        str_char_append(content,*ptr);
    }
  }
}
/* }}} */

/* {{{ flt_directives_generate_uri */
void flt_directives_generate_uri(const u_char *uri,const u_char *title,t_string *content,t_string *cite,int sig,t_configuration *dc,t_configuration *vc,int icons) {
  register u_char *ptr;
  u_char *tmp1,*tmp2 = NULL,*tmp3,*hostname;
  size_t len = 0,len1 = 0,len2,i;
  t_flt_directives_lt_tok *tok;
  u_char *new_uri = NULL;
  t_handler_config *handler;
  t_filter_urlrewrite fkt;
  int ret = FLT_DECLINE;

  uri = strdup(uri);
  if(Modules[URL_REWRITE_HANDLER].elements) {
    for(i=0;i<Modules[URL_REWRITE_HANDLER].elements && ret == FLT_DECLINE;++i) {
      handler = array_element_at(&Modules[URL_REWRITE_HANDLER],i);
      fkt     = (t_filter_urlrewrite)handler->func;
      ret     = fkt(dc,vc,uri,&new_uri);
    }

    if(new_uri) uri = new_uri;
  }

  tmp2 = htmlentities(uri,1);
  len = strlen(tmp2);

  tmp1 = cf_cgi_url_encode(tmp2,len);
  len2 = strlen(tmp1);

  if(flt_directives_rpl && title == NULL) {
    title = strdup(tmp2);
    len1  = len;
  }

  /* {{{ generate <a href */
  str_chars_append(content,"<a href=\"",9);
  str_chars_append(content,tmp2,len);

  if(flt_directives_rel_no_follow) str_chars_append(content,"\" rel=\"nofollow",15);

  if(flt_directives_link || flt_directives_extlink) {
    if((hostname = getenv("SERVER_NAME")) != NULL) {
      if(cf_strncmp(uri+7,hostname,strlen(hostname)) == 0) {
        if(flt_directives_link) {
          str_chars_append(content,"\" target=\"",10);
          str_chars_append(content,flt_directives_link,strlen(flt_directives_link));
        }
      }
      else {
        if(flt_directives_extlink) {
          str_chars_append(content,"\" target=\"",10);
          str_chars_append(content,flt_directives_extlink,strlen(flt_directives_extlink));
        }
      }
    }
    /* fallback: SERVER_NAME is not set. Just append default target, if exists */
    else {
      if(flt_directives_link) {
        str_chars_append(content,"\" target=\"",10);
        str_chars_append(content,flt_directives_link,strlen(flt_directives_link));
      }
    }
  }

  str_chars_append(content,"\">",2);
  /* }}} */

  if(title) {
    len1 = strlen(title);

    /*
     * OK, we got the border around the link, now lets generate the link text
     */

    if(flt_directives_lt_toks.elements) {
      for(i=0;i<flt_directives_lt_toks.elements;i++) {
        if((tok = array_element_at(&flt_directives_lt_toks,i)) != NULL) {
          switch(tok->type) {
            case FLT_DIRECTIVES_TOK_TITLE:
              str_chars_append(content,"<span class=\"lnk-title\">",24);
              str_chars_append(content,title,len1);
              str_chars_append(content,"</span>",7);
              break;

            case FLT_DIRECTIVES_TOK_URI:
              str_chars_append(content,"<span class=\"lnk-uri\">",22);
              str_chars_append(content,tmp2,len);
              str_chars_append(content,"</span>",7);
              break;
            default:
              str_chars_append(content,tok->tok,tok->type);
              break;
          }
        }
      }
    }
    else str_chars_append(content,title,len1);
  }
  else str_chars_append(content,tmp2,len);

  /*
   * generate rest of the link and the cite
   */
  if(cite && sig == 0) {
    str_chars_append(cite,"[link:",6);
    str_chars_append(cite,tmp2,len);
    if(title) {
      str_chars_append(cite,"@title=",7);
      str_chars_append(cite,title,len1);
    }
    str_char_append(cite,']');
  }

  str_chars_append(content,"</a>",4);


  if(flt_directives_icons && flt_directives_lit && icons) flt_directives_replace(content,flt_directives_icons,tmp2,len,tmp1,len2,title,len1);

  free(tmp1);
  free(tmp2);
  free(uri);
}
/* }}} */

/* {{{ flt_directives_execute */
int flt_directives_execute(t_configuration *fdc,t_configuration *fvc,t_cl_thread *thread,const u_char *directive,const u_char **parameters,size_t plen,t_string *bco,t_string *bci,t_string *content,t_string *cite,const u_char *qchars,int sig) {
  u_char *forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  size_t len = 0,i,len1 = 0;
  t_name_value *xhtml = cfg_get_first_value(fdc,forum_name,"XHTMLMode");
  u_int64_t tid,mid;
  u_char *ptr,*tmp,*tmp1 = NULL,**list = NULL,*title_alt = NULL,*tmp2 = NULL,*uname = cf_hash_get(GlobalValues,"UserName",8);
  cf_readmode_t *rm = cf_hash_get(GlobalValues,"RM",2);
  t_flt_directives_ref_uri *uri;
  int go = 1;
  t_string tmpstr;
  u_char *parameter = (u_char *)parameters[0];

  //if(!parameter) return FLT_DECLINE;
  while(isspace(*parameter)) ++parameter;

  if(*directive == 'l') {
    /* {{{ [link:] */
    if(cf_strcmp(directive,"link") == 0) {
      if((ptr = strstr(parameter,"@title=")) != NULL) {
        if(*(ptr + 7) && flt_directives_is_valid_title(ptr+7,strlen(ptr+7))) {
          tmp1      = strndup(parameter,ptr-parameter);
          title_alt = htmlentities(ptr + 7,1);
        }
        else return FLT_DECLINE;
      }
      else tmp1 = (u_char *)parameter;

      if(is_valid_link(tmp1) != 0) {
        if(cf_strncmp(tmp1,"..",2) == 0 || *tmp1 == '/' || *tmp1 == '?') {
          if(!flt_directives_is_relative_uri(tmp1,strlen(tmp1))) go = 0;
        }
        else go = 0;
      }

      if(go) {
        flt_directives_generate_uri(tmp1,title_alt,content,cite,sig,fdc,fvc,1);
        if(title_alt) free(title_alt);

        return FLT_OK;
      }
    }
    /* }}} */
  }
  else if(*directive == 'i') {
    /* {{{ [image:] */
    if(cf_strcmp(directive,"image") == 0) {
      if((ptr = strstr(parameter,"@alt=")) != NULL) {
        if(*(ptr+5) && flt_directives_is_valid_title(ptr+5,strlen(ptr+5))) {
          tmp1      = strndup(parameter,ptr-parameter);
          len       = ptr - parameter;
          title_alt = htmlentities(ptr + 5,1);
          len1      = strlen(title_alt);
        }
        else return FLT_DECLINE;
      }
      else {
        tmp1 = (u_char *)parameter;
        len  = strlen(parameter);
      }

      if(is_valid_link(tmp1) != 0) {
        if(cf_strncmp(tmp1,"..",2) == 0 || *tmp1 == '/' || *tmp1 == '?') {
          if(!flt_directives_is_relative_uri(tmp1,len)) {
            go = 0;
          }
        }
        else {
          go = 0;
        }
      }

      if(go) {
        tmp2 = htmlentities(tmp1,1);
        len = strlen(tmp2);

        if(flt_directives_imagesaslink) {
          flt_directives_generate_uri(tmp1,title_alt,content,NULL,sig,fdc,fvc,0);
        }
        else {
          str_chars_append(content,"<img src=\"",10);
          str_chars_append(content,tmp2,len);
          str_char_append(content,'"');

          if(title_alt) {
            str_chars_append(content," alt=\"",6);
            str_chars_append(content,title_alt,len1);
            str_chars_append(content,"\" title=\"",9);
            str_chars_append(content,title_alt,len1);
          }

          if(*xhtml->values[0] == 'y')  str_chars_append(content,"\"/>",3);
          else str_chars_append(content,"\">",2);
        }

        if(cite && sig == 0) {
          str_chars_append(cite,"[image:",7);
          str_chars_append(cite,tmp2,len);
          if(title_alt) {
            str_chars_append(cite,"@alt=",5);
            str_chars_append(cite,title_alt,len1);
          }
          str_char_append(cite,']');
        }

        free(tmp2);
        if(title_alt) free(title_alt);

        return FLT_OK;
      }
    }
    /* }}} */
    /* {{{ [iframe:] */
    else if(cf_strcmp(directive,"iframe") == 0) {
      if(is_valid_link(parameter) != 0) {
        if(cf_strncmp(parameter,"..",2) == 0 || *parameter == '/' || *parameter == '?') {
          if(!flt_directives_is_relative_uri(parameter,len)) {
            go = 0;
          }
        }
        else {
          go = 0;
        }
      }

      if(go) {
        tmp2 = htmlentities(parameter,1);
        len = strlen(tmp2);

        if(flt_directives_iframesaslink) {
          flt_directives_generate_uri(parameter,NULL,content,NULL,sig,fdc,fvc,1);
        }
        else {
          str_chars_append(content,"<iframe src=\"",13);
          str_chars_append(content,tmp2,len);
          str_chars_append(content,"\" width=\"90%\" height=\"90%\"><a href=\"",36);
          str_chars_append(content,tmp2,len);

          if(flt_directives_link) {
            str_chars_append(content,"\" target=\"",10);
            str_chars_append(content,flt_directives_link,strlen(flt_directives_link));
          }

          str_chars_append(content,"\">",2);
          str_chars_append(content,tmp2,len);
          str_chars_append(content,"</a></iframe>",13);
        }

        if(cite && sig == 0) {
          str_chars_append(cite,"[iframe:",8);
          str_chars_append(cite,tmp2,len);
          str_char_append(cite,']');
        }

        free(tmp2);

        return FLT_OK;
      }
    }
    /* }}} */
  }
  else {
    /* {{{ [pref:] */
    if(cf_strcmp(directive,"pref") == 0) {
      tid = mid = 0;

      if(flt_directives_is_valid_pref(parameter,&tmp1,&tmp2)) {
        tid = str_to_u_int64(parameter+2);
        mid = str_to_u_int64(tmp1);
        tmp1 = cf_get_link(rm->posting_uri[uname?1:0],tid,mid);
        if(tmp2) tmp2 += 7;

        flt_directives_generate_uri(tmp1,tmp2,content,cite,sig,fdc,fvc,0);

        free(tmp1);

        return FLT_OK;
      }
    }
    /* }}} */
    /* {{{ [ref:] */
    else if(cf_strcmp(directive,"ref") == 0) {
      len   = nsplit(parameter,";",&list,2);

      if(len == 2) {
        for(i=0;i<flt_directives_ref_uris.elements;i++) {
          uri = array_element_at(&flt_directives_ref_uris,i);

          if(cf_strcmp(uri->id,list[0]) == 0) {
            /* check for title */
            if((title_alt = strstr(list[1],"@title=")) != NULL) {
              if(*(title_alt+7) && flt_directives_is_valid_title(title_alt+7,strlen(title_alt+7))) {
                tmp2 = strndup(list[1],title_alt-list[1]);
                title_alt = htmlentities(title_alt+7,0);
              }
              else return FLT_DECLINE;
            }
            else tmp2 = strdup(list[1]);

            tmp = htmlentities(list[0],0);
            tmp1 = htmlentities(uri->uri,0);
            ptr = htmlentities(tmp2,0);

            str_init(&tmpstr);
            str_chars_append(&tmpstr,uri->uri,strlen(uri->uri));
            str_chars_append(&tmpstr,tmp2,strlen(tmp2));

            flt_directives_generate_uri(tmpstr.content,title_alt,content,NULL,0,fdc,fvc,1);

            if(sig == 0 && cite) {
              str_chars_append(cite,"[ref:",5);
              str_chars_append(cite,tmp,strlen(tmp));
              str_char_append(cite,';');
              str_chars_append(cite,ptr,strlen(ptr));
              if(title_alt) {
                str_chars_append(cite,"@title=",7);
                str_chars_append(cite,title_alt,strlen(title_alt));
              }
              str_char_append(cite,']');
            }

            str_cleanup(&tmpstr);
            free(list[0]);
            free(list[1]);
            free(list);
            free(tmp);
            free(tmp1);
            free(tmp2);
            free(ptr);

            return FLT_OK;
          }
        }

        if(list) {
          for(i=0;i<len;++i) free(list[i]);
          free(list);
        }
      }
    }
    /* }}} */
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_directives_is_unwanted */
int flt_directives_is_unwanted(const u_char *link,size_t len) {
  size_t i;
  int erroffset;
  pcre *regexp;
  char *error;

  for(i=0;i<flt_directives_bdl_len;++i) {
    if((regexp = pcre_compile(flt_directives_badlinks[i], 0, (const char **)&error, &erroffset, NULL)) == NULL) {
      fprintf(stderr,"flt_directives: error in pattern '%s' (offset %d): %s\n",flt_directives_badlinks[i],erroffset,error);
      continue;
    }

    if(pcre_exec(regexp,NULL,link,len,0,0,NULL,0) >= 0) {
      pcre_free(regexp);
      return -1;
    }

    pcre_free(regexp);
  }

  return 0;
}
/* }}} */

/* {{{ flt_directives_validate */
int flt_directives_validate(t_configuration *fdc,t_configuration *fvc,const u_char *directive,const u_char **parameters,size_t plen,t_cf_tpl_variable *var) {
  u_char *forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  u_char *parameter = (u_char *)parameters[0];

  u_char *tmp1,*tmp2,*title_alt,*ptr,**list,*err;

  t_flt_directives_ref_uri *uri;

  size_t len,i;
  u_int64_t tid,mid;

  while(isspace(*parameter)) ++parameter;

  if(*directive == 'l') {
    /* {{{ [link:] */
    if(cf_strcmp(directive,"link") == 0) {
      if((ptr = strstr(parameter,"@title=")) != NULL) tmp1 = strndup(parameter,ptr-parameter);
      else tmp1 = (u_char *)parameter;

      len = strlen(tmp1);

      if(is_valid_link(tmp1) != 0) {
        if(cf_strncmp(tmp1,"..",2) == 0 || *tmp1 == '/' || *tmp1 == '?') {
          if(!flt_directives_is_relative_uri(tmp1,len)) {
            if(flt_directives_wbl == 0) return FLT_DECLINE;

            if((err = cf_get_error_message("E_posting_links",&len)) != NULL) {
              cf_tpl_var_addvalue(var,TPL_VARIABLE_STRING,err,len);
              free(err);
            }

            return FLT_ERROR;
          }
          else {
            if(flt_directives_is_unwanted(tmp1,len) == -1) {
              if((err = cf_get_error_message("E_unwanted_link",&len)) != NULL) {
                cf_tpl_var_addvalue(var,TPL_VARIABLE_STRING,err,len);
                free(err);
              }
              else fprintf(stderr,"flt_directives: Unwanted link but could not find error message!\n");

              return FLT_ERROR;
            }
          }
        }
        else {
          if(flt_directives_wbl == 0) return FLT_DECLINE;

          if((err = cf_get_error_message("E_posting_links",&len)) != NULL) {
            cf_tpl_var_addvalue(var,TPL_VARIABLE_STRING,err,len);
            free(err);
          }
          return FLT_ERROR;
        }
      }
      else {
        if(flt_directives_is_unwanted(tmp1,len) == -1) {
          if((err = cf_get_error_message("E_unwanted_link",&len)) != NULL) {
            cf_tpl_var_addvalue(var,TPL_VARIABLE_STRING,err,len);
            free(err);
          }
          else fprintf(stderr,"flt_directives: Unwanted link but could not find error message!\n");

          return FLT_ERROR;
        }
      }

      return FLT_OK;
    }
    /* }}} */
  }
  else if(*directive == 'i') {
    if(flt_directives_wbl == 0) return FLT_DECLINE;

    /* {{{ [image:] */
    if(cf_strcmp(directive,"image") == 0) {
      if((ptr = strstr(parameter,"@alt=")) != NULL) tmp1      = strndup(parameter,ptr-parameter);
      else tmp1 = (u_char *)parameter;

      len = strlen(tmp1);

      if(is_valid_link(tmp1) != 0) {
        if(cf_strncmp(tmp1,"..",2) == 0 || *tmp1 == '/' || *tmp1 == '?') {
          if(!flt_directives_is_relative_uri(tmp1,len)) {
            if((err = cf_get_error_message("E_posting_links",&len)) != NULL) {
              cf_tpl_var_addvalue(var,TPL_VARIABLE_STRING,err,len);
              free(err);
            }
            return FLT_ERROR;
          }
        }
        else {
          if((err = cf_get_error_message("E_posting_links",&len)) != NULL) {
            cf_tpl_var_addvalue(var,TPL_VARIABLE_STRING,err,len);
            free(err);
          }
          return FLT_ERROR;
        }
      }

      return FLT_OK;
    }
    /* }}} */
    /* {{{ [iframe:] */
    else if(cf_strcmp(directive,"iframe") == 0) {
      if(flt_directives_wbl == 0) return FLT_DECLINE;

      if(is_valid_link(parameter) != 0) {
        if(cf_strncmp(parameter,"..",2) == 0 || *parameter == '/' || *parameter == '?') {
          len = strlen(parameter);

          if(!flt_directives_is_relative_uri(parameter,len)) {
            if((err = cf_get_error_message("E_posting_links",&len)) != NULL) {
              cf_tpl_var_addvalue(var,TPL_VARIABLE_STRING,err,len);
              free(err);
            }
            return FLT_ERROR;
          }
        }
        else {
          if((err = cf_get_error_message("E_posting_links",&len)) != NULL) {
            cf_tpl_var_addvalue(var,TPL_VARIABLE_STRING,err,len);
            free(err);
          }
          return FLT_ERROR;
        }
      }
    }
    /* }}} */
  }
  else {
    if(flt_directives_wbl == 0) return FLT_DECLINE;

    /* {{{ [pref:] */
    if(cf_strcmp(directive,"pref") == 0) {
      tid = mid = 0;

      if(!flt_directives_is_valid_pref(parameter,&tmp1,&tmp2)) {
        if((err = cf_get_error_message("E_posting_links",&len)) != NULL) {
          cf_tpl_var_addvalue(var,TPL_VARIABLE_STRING,err,len);
          free(err);
        }
        return FLT_ERROR;
      }
    }
    /* }}} */
    /* {{{ [ref:] */
    else if(cf_strcmp(directive,"ref") == 0) {
      len   = nsplit(parameter,";",&list,2);

      if(len == 2) {
        for(i=0;i<flt_directives_ref_uris.elements;i++) {
          uri = array_element_at(&flt_directives_ref_uris,i);

          if(cf_strcmp(uri->id,list[0]) == 0) return FLT_OK;
        }

        if(list) {
          for(i=0;i<len;++i) free(list[i]);
          free(list);
        }

        if((err = cf_get_error_message("E_posting_links",&len)) != NULL) {
          cf_tpl_var_addvalue(var,TPL_VARIABLE_STRING,err,len);
          free(err);
        }
        return FLT_ERROR;
      }
    }
    /* }}} */
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_directives_check_for_pref */
int flt_directives_check_for_pref(const u_char *link,size_t len) {
  size_t i;
  flt_directives_re *re;

  for(i=0;i<flt_directives_puris.elements;++i) {
    re = array_element_at(&flt_directives_puris,i);

    if(pcre_exec(re->re,re->extra,link,len,0,0,NULL,0) >= 0) return 1;
  }

  return 0;
}
/* }}} */

/* {{{ flt_directives_parse_link_for_pref */
int flt_directives_parse_link_for_pref(const u_char *link,u_int64_t *tid,u_int64_t *mid) {
  register u_char *ptr;
  int found = 0;

  for(ptr=(u_char *)link;*ptr;++ptr) {
    switch(*ptr) {
      case 't':
        if(*(ptr+1) == '=' || *(ptr+1) == '/') *tid = str_to_u_int64(ptr+2);
        found |= 1;
        break;
      case 'm':
        if(*(ptr+1) == '=' || *(ptr+1) == '/') *mid = str_to_u_int64(ptr+2);
        found |= 2;
        break;
    }
  }

  return (found & 1) && (found & 2);
}
/* }}} */

/* {{{ flt_directives_rewrite */
int flt_directives_rewrite(t_cf_hash *head,t_configuration *dc,t_configuration *pc,t_message *p,t_cl_thread *thr,int sock,int mode) {
  t_string new_content;
  register u_char *ptr;
  u_char *safe,*link,*title;
  u_int64_t tid,mid;
  size_t len;

  str_init(&new_content);

  for(ptr=p->content.content;*ptr;++ptr) {
    switch(*ptr) {
      case '[':
        if(cf_strncmp(ptr,"[link:",6)  == 0) {
          safe = ptr;

          for(ptr+=6,title=NULL;*ptr && *ptr != ']' && *ptr != '<';++ptr) {
            if(*ptr == '@' && cf_strncmp(ptr,"@title=",7) == 0 && title == NULL) title = ptr;
          }

          if(title) len = title - safe - 6;
          else len = ptr - safe - 6;

          link = strndup(safe+6,len);

          if(flt_directives_check_for_pref(link,len)) {
            if(flt_directives_parse_link_for_pref(link,&tid,&mid)) {
              str_chars_append(&new_content,"[pref:t=",8);
              u_int64_to_str(&new_content,tid);
              str_chars_append(&new_content,";m=",3);
              u_int64_to_str(&new_content,mid);
              if(title) str_chars_append(&new_content,title,ptr-title);
              str_char_append(&new_content,']');
              continue;
            }
          }

          ptr = safe;
        }
      default:
        str_char_append(&new_content,*ptr);
    }
  }

  str_cleanup(&p->content);
  p->content.content = new_content.content;
  p->content.len = new_content.len;
  p->content.reserved = new_content.reserved;

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_directives_suial_set */
int flt_directives_suial_set(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_cl_thread *thread,t_message *msg,t_cf_tpl_variable *hash) {
  if(flt_directives_suial == 0) cf_tpl_hashvar_setvalue(hash,"showimage",TPL_VARIABLE_INT,1);
  if(flt_directives_link) cf_tpl_hashvar_setvalue(hash,"target",TPL_VARIABLE_STRING,flt_directives_link,strlen(flt_directives_link));
  return FLT_OK;
}
/* }}} */

/* {{{ flt_directives_init */
int flt_directives_init(t_cf_hash *cgi,t_configuration *dc,t_configuration *vc) {
  cf_html_register_directive("link",flt_directives_execute,CF_HTML_DIR_TYPE_ARG|CF_HTML_DIR_TYPE_INLINE);
  cf_html_register_directive("pref",flt_directives_execute,CF_HTML_DIR_TYPE_ARG|CF_HTML_DIR_TYPE_INLINE);
  cf_html_register_directive("ref",flt_directives_execute,CF_HTML_DIR_TYPE_ARG|CF_HTML_DIR_TYPE_INLINE);
  cf_html_register_directive("image",flt_directives_execute,CF_HTML_DIR_TYPE_ARG|CF_HTML_DIR_TYPE_INLINE);
  cf_html_register_directive("iframe",flt_directives_execute,CF_HTML_DIR_TYPE_ARG|CF_HTML_DIR_TYPE_INLINE);

  cf_html_register_validator("link",flt_directives_validate,CF_HTML_DIR_TYPE_ARG|CF_HTML_DIR_TYPE_INLINE);
  cf_html_register_validator("pref",flt_directives_validate,CF_HTML_DIR_TYPE_ARG|CF_HTML_DIR_TYPE_INLINE);
  cf_html_register_validator("ref",flt_directives_validate,CF_HTML_DIR_TYPE_ARG|CF_HTML_DIR_TYPE_INLINE);
  cf_html_register_validator("image",flt_directives_validate,CF_HTML_DIR_TYPE_ARG|CF_HTML_DIR_TYPE_INLINE);
  cf_html_register_validator("iframe",flt_directives_validate,CF_HTML_DIR_TYPE_ARG|CF_HTML_DIR_TYPE_INLINE);

  return FLT_DECLINE;
}
/* }}} */

/* {{{ directive handlers */
int flt_directives_handle_rpl(t_configfile *cfile,t_conf_opt *opt,const u_char *context,u_char **args,size_t argnum) {
  if(flt_directives_fname == NULL) flt_directives_fname = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(context,flt_directives_fname) != 0) return 0;

  flt_directives_rpl = cf_strcmp(args[0],"yes") == 0;

  return 0;
}

int flt_directives_handle_uwl(t_configfile *cfile,t_conf_opt *opt,const u_char *context,u_char **args,size_t argnum) {
  size_t i;

  if(flt_directives_fname == NULL) flt_directives_fname = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(context,flt_directives_fname) != 0) return 0;

  if(flt_directives_badlinks) {
    for(i=0;i<flt_directives_bdl_len;++i) free(flt_directives_badlinks[i]);
    free(flt_directives_badlinks);
  }

  flt_directives_bdl_len  = argnum;
  flt_directives_badlinks = args;

  return -1;
}

int flt_directives_handle_lit(t_configfile *cfile,t_conf_opt *opt,const u_char *context,u_char **args,size_t argnum) {
  if(flt_directives_fname == NULL) flt_directives_fname = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(context,flt_directives_fname) != 0) return 0;

  flt_directives_lit = cf_strcmp(args[0],"yes") == 0;

  return 0;
}

int flt_directives_handle_icons(t_configfile *cfile,t_conf_opt *opt,const u_char *context,u_char **args,size_t argnum) {
  if(flt_directives_fname == NULL) flt_directives_fname = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(context,flt_directives_fname) != 0) return 0;

  if(flt_directives_icons) free(flt_directives_icons);
  flt_directives_icons = strdup(args[0]);

  return 0;
}

int flt_directives_handle_suial(t_configfile *cfile,t_conf_opt *opt,const u_char *context,u_char **args,size_t argnum) {
  if(flt_directives_fname == NULL) flt_directives_fname = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(context,flt_directives_fname) != 0) return 0;

  flt_directives_suial = cf_strcmp(args[0],"yes") == 0;

  return 0;
}

int flt_directives_handle_wbl(t_configfile *cfile,t_conf_opt *opt,const u_char *context,u_char **args,size_t argnum) {
  if(flt_directives_fname == NULL) flt_directives_fname = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(context,flt_directives_fname) != 0) return 0;

  flt_directives_wbl = cf_strcmp(args[0],"yes") == 0;

  return 0;
}

int flt_directives_handle_purl(t_configfile *cfile,t_conf_opt *opt,const u_char *context,u_char **args,size_t argnum) {
  pcre *re;
  pcre_extra *extra;
  u_char *error = NULL;
  int offset;
  flt_directives_re ar_re;

  if(flt_directives_fname == NULL) flt_directives_fname = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(context,flt_directives_fname) != 0) return 0;

  if(flt_directives_puris.element_size == 0) array_init(&flt_directives_puris,sizeof(ar_re),NULL);

  if((re = pcre_compile((const char *)args[0],PCRE_CASELESS,(const char **)&error,&offset,NULL)) == NULL) {
    fprintf(stderr,"flt_directives: error in pattern '%s': %s (Offset %d)\n",args[0],error,offset);
    return 1;
  }

  if((extra = pcre_study(re,0,(const char **)&error)) == NULL) {
    if(error) {
      fprintf(stderr,"flt_directives: error in pattern '%s': %s\n",args[0],error);
      return 1;
    }
  }

  ar_re.re = re;
  ar_re.extra = extra;

  array_push(&flt_directives_puris,&ar_re);

  return 0;
}

int flt_directives_handle_iframe(t_configfile *cfile,t_conf_opt *opt,const u_char *context,u_char **args,size_t argnum) {
  if(flt_directives_fname == NULL) flt_directives_fname = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(context,flt_directives_fname) != 0) return 0;

  flt_directives_iframesaslink = cf_strcmp(args[0],"yes") == 0;
  return 0;
}

int flt_directives_handle_image(t_configfile *cfile,t_conf_opt *opt,const u_char *context,u_char **args,size_t argnum) {
  if(flt_directives_fname == NULL) flt_directives_fname = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(context,flt_directives_fname) != 0) return 0;

  flt_directives_imagesaslink = cf_strcmp(args[0],"yes") == 0;
  return 0;
}

int flt_directives_handle_link(t_configfile *cfile,t_conf_opt *opt,const u_char *context,u_char **args,size_t argnum) {
  if(flt_directives_fname == NULL) flt_directives_fname = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(context,flt_directives_fname) != 0) return 0;

  if(cf_strcmp(opt->name,"PostingLinkExtTarget") == 0) {
    if(flt_directives_extlink) free(flt_directives_extlink);
    flt_directives_extlink = strdup(args[0]);
  }
  else {
    if(flt_directives_link) free(flt_directives_link);
    flt_directives_link = strdup(args[0]);
  }

  return 0;
}

void flt_directives_cleanup_entry(void *e) {
  t_flt_directives_ref_uri *uri = (t_flt_directives_ref_uri *)e;
  free(uri->uri);
  free(uri->id);
}

int flt_directives_handle_ref(t_configfile *cfile,t_conf_opt *opt,const u_char *context,u_char **args,size_t argnum) {
  t_flt_directives_ref_uri uri;

  if(flt_directives_fname == NULL) flt_directives_fname = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(context,flt_directives_fname) != 0) return 0;

  uri.id  = strdup(args[0]);
  uri.uri = strdup(args[1]);

  if(flt_directives_ref_uris.element_size == 0) array_init(&flt_directives_ref_uris,sizeof(uri),flt_directives_cleanup_entry);

  array_push(&flt_directives_ref_uris,&uri);

  return 0;
}

int flt_directives_handle_rel(t_configfile *cfile,t_conf_opt *opt,const u_char *context,u_char **args,size_t argnum) {
  if(flt_directives_fname == NULL) flt_directives_fname = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(context,flt_directives_fname) != 0) return 0;

  flt_directives_rel_no_follow = cf_strcmp(args[0],"yes") == 0;

  return 0;
}

int flt_directives_handle_lt(t_configfile *cfile,t_conf_opt *opt,const u_char *context,u_char **args,size_t argnum) {
  u_char *ptr;
  t_string str;
  t_flt_directives_lt_tok tok;

  if(flt_directives_fname == NULL) flt_directives_fname = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(context,flt_directives_fname) != 0) return 0;

  str_init(&str);
  array_init(&flt_directives_lt_toks,sizeof(tok),NULL);

  for(ptr=args[0];*ptr;++ptr) {
    switch(*ptr) {
      case '%':
        if(*(ptr+1)) {
          switch(*(ptr+1)) {
            case '%':
              str_char_append(&str,*(ptr+1));
              break;
            case 't':
              if(str.len) {
                tok.tok  = htmlentities(str.content,1);
                tok.type = strlen(tok.tok);
                array_push(&flt_directives_lt_toks,&tok);
              }

              tok.type = FLT_DIRECTIVES_TOK_TITLE;
              tok.tok  = NULL;
              array_push(&flt_directives_lt_toks,&tok);

              str.len = 0;

              break;

            case 'u':
              if(str.len) {
                tok.tok  = htmlentities(str.content,1);
                tok.type = strlen(tok.tok);
                array_push(&flt_directives_lt_toks,&tok);
              }

              tok.type = FLT_DIRECTIVES_TOK_URI;
              tok.tok  = NULL;
              array_push(&flt_directives_lt_toks,&tok);

              str.len = 0;

              break;

            default:
              str_char_append(&str,*(ptr+1));
          }
          ++ptr;
          break;
        }
        break;

      default:
        str_char_append(&str,*ptr);
    }
  }

  if(str.len) {
    tok.tok  = htmlentities(str.content,1);
    tok.type = strlen(tok.tok);
    array_push(&flt_directives_lt_toks,&tok);

    str_cleanup(&str);
  }

  return 0;
}
/* }}} */

/* {{{ flt_directives_cleanup */
void flt_directives_cleanup(void) {
  if(flt_directives_link) free(flt_directives_link);
  if(flt_directives_ref_uris.element_size > 0) array_destroy(&flt_directives_ref_uris);
}
/* }}} */

t_conf_opt flt_directives_config[] = {
  { "PostingUrl",           flt_directives_handle_purl,     CFG_OPT_CONFIG|CFG_OPT_LOCAL,               NULL },
  { "ShowIframeAsLink",     flt_directives_handle_iframe,   CFG_OPT_CONFIG|CFG_OPT_USER|CFG_OPT_LOCAL,  NULL },
  { "ShowImageAsLink",      flt_directives_handle_image,    CFG_OPT_CONFIG|CFG_OPT_USER|CFG_OPT_LOCAL,  NULL },
  { "PostingLinkTarget",    flt_directives_handle_link,     CFG_OPT_CONFIG|CFG_OPT_USER|CFG_OPT_LOCAL,  NULL },
  { "PostingLinkExtTarget", flt_directives_handle_link,     CFG_OPT_CONFIG|CFG_OPT_USER|CFG_OPT_LOCAL,  NULL },
  { "SetRelNoFollow",       flt_directives_handle_rel,      CFG_OPT_CONFIG|CFG_OPT_LOCAL, NULL },
  { "ReferenceURI",         flt_directives_handle_ref,      CFG_OPT_CONFIG|CFG_OPT_LOCAL, NULL },
  { "LinkTemplate",         flt_directives_handle_lt,       CFG_OPT_CONFIG|CFG_OPT_USER|CFG_OPT_LOCAL,  NULL },
  { "WarnBadLinks",         flt_directives_handle_wbl,      CFG_OPT_CONFIG|CFG_OPT_USER|CFG_OPT_LOCAL,  NULL },
  { "ShowUserImageAsLink",  flt_directives_handle_suial,    CFG_OPT_CONFIG|CFG_OPT_USER|CFG_OPT_LOCAL,  NULL },
  { "LinkIcons",            flt_directives_handle_icons,    CFG_OPT_CONFIG|CFG_OPT_LOCAL, NULL },
  { "LinkShowIcons",        flt_directives_handle_lit,      CFG_OPT_CONFIG|CFG_OPT_USER|CFG_OPT_LOCAL,  NULL },
  { "UnwantedLinks",        flt_directives_handle_uwl,      CFG_OPT_CONFIG|CFG_OPT_LOCAL,               NULL },
  { "ReplaceNormal",        flt_directives_handle_rpl,      CFG_OPT_CONFIG|CFG_OPT_USER|CFG_OPT_LOCAL,  NULL },
  { NULL, NULL, 0, NULL }
};

t_handler_config flt_directives_handlers[] = {
  { PERPOST_VAR_HANDLER, flt_directives_suial_set },
  { INIT_HANDLER,        flt_directives_init },
  { NEW_POST_HANDLER,    flt_directives_rewrite },
  { 0, NULL }
};

t_module_config flt_directives = {
  MODULE_MAGIC_COOKIE,
  flt_directives_config,
  flt_directives_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  flt_directives_cleanup
};

/* eof */
