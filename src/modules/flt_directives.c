/**
 * \file flt_directives.c
 * \author Christian Kruse, <cjk@wwwtech.de>
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
#include "cfconfig.h"
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
#include "cfgcomp.h"
#include "cfcgi.h"
#include "template.h"
#include "clientlib.h"
#include "charconvert.h"
#include "validate.h"
#include "htmllib.h"
/* }}} */

#define FLT_DIRECTIVES_TOK_TITLE  0
#define FLT_DIRECTIVES_TOK_URI   -1
#define FLT_DIRECTIVES_TOK_ID    -2

typedef struct {
  int type;
  u_char *tok;
} flt_directives_lt_tok_t;

static cf_array_t flt_directives_lt_toks  = { 0, 0, 0, NULL, NULL };

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
  cf_string_t str;
  int ret = 0;

  cf_str_init(&str);
  cf_str_char_set(&str,"http://www.example.org",21);
  if(*tmp == '.' || *tmp == '?') cf_str_char_append(&str,'/');
  cf_str_chars_append(&str,tmp,len);

  ret = is_valid_http_link(str.content,0);
  cf_str_cleanup(&str);

  return ret == 0;
}
/* }}} */

/* {{{ flt_directives_replace */
void flt_directives_replace(cf_string_t *content,const u_char *str,const u_char *uri,size_t ulen,const u_char *escaped,size_t eulen,const u_char *title,size_t tlen) {
  register u_char *ptr;

  for(ptr=(u_char *)str;*ptr;++ptr) {
    switch(*ptr) {
      case '%':
        switch(*(ptr+1)) {
          case 't':
            if(title) cf_str_chars_append(content,title,tlen);
            else cf_str_chars_append(content,uri,ulen);
            break;
          case 'u':
            cf_str_chars_append(content,uri,ulen);
            break;
          case 'e':
            cf_str_chars_append(content,escaped,eulen);
            break;
          default:
            cf_str_char_append(content,'%');
            continue;
        }
        ++ptr;
        break;

      default:
        cf_str_char_append(content,*ptr);
    }
  }
}
/* }}} */

/* {{{ flt_directives_parse_linktemplate */
void flt_directives_parse_linktemplate(const u_char *tpl) {
  u_char *ptr;
  cf_string_t str;
  flt_directives_lt_tok_t tok;

  cf_str_init(&str);
  cf_array_init(&flt_directives_lt_toks,sizeof(tok),NULL);

  for(ptr=tpl;*ptr;++ptr) {
    switch(*ptr) {
      case '%':
        if(*(ptr+1)) {
          switch(*(ptr+1)) {
            case '%':
              cf_str_char_append(&str,*(ptr+1));
              break;
            case 't':
              if(str.len) {
                tok.tok  = htmlentities(str.content,1);
                tok.type = strlen(tok.tok);
                cf_array_push(&flt_directives_lt_toks,&tok);
              }

              tok.type = FLT_DIRECTIVES_TOK_TITLE;
              tok.tok  = NULL;
              cf_array_push(&flt_directives_lt_toks,&tok);

              str.len = 0;

              break;

            case 'u':
              if(str.len) {
                tok.tok  = htmlentities(str.content,1);
                tok.type = strlen(tok.tok);
                cf_array_push(&flt_directives_lt_toks,&tok);
              }

              tok.type = FLT_DIRECTIVES_TOK_URI;
              tok.tok  = NULL;
              cf_array_push(&flt_directives_lt_toks,&tok);

              str.len = 0;

              break;

            default:
              cf_str_char_append(&str,*(ptr+1));
          }
          ++ptr;
          break;
        }
        break;

      default:
        cf_str_char_append(&str,*ptr);
    }
  }

  if(str.len) {
    tok.tok  = htmlentities(str.content,1);
    tok.type = strlen(tok.tok);
    cf_array_push(&flt_directives_lt_toks,&tok);

    cf_str_cleanup(&str);
  }

  return 0;
}
/* }}} */

/* {{{ flt_directives_generate_uri */
void flt_directives_generate_uri(const u_char *uri,const u_char *title,cf_string_t *content,cf_string_t *cite,int sig,cf_configuration_t *cfg,int icons) {
  u_char *tmp1,*tmp2 = NULL,*hostname;
  size_t len = 0,len1 = 0,len2,i;
  flt_directives_lt_tok_t *tok;
  u_char *new_uri = NULL;
  cf_handler_config_t *handler;
  filter_urlrewrite_t fkt;
  int ret = FLT_DECLINE;
  cf_cfg_config_value_t *target,*target_ext,*link_tpl,*cfg_icos,*cfg_showicos;

  uri = strdup(uri);
  if(Modules[URL_REWRITE_HANDLER].elements) {
    for(i=0;i<Modules[URL_REWRITE_HANDLER].elements && ret == FLT_DECLINE;++i) {
      handler = cf_array_element_at(&Modules[URL_REWRITE_HANDLER],i);
      fkt     = (filter_urlrewrite_t)handler->func;
      ret     = fkt(cfg,uri,&new_uri);
    }

    if(new_uri) uri = new_uri;
  }

  tmp2 = htmlentities(uri,1);
  len = strlen(tmp2);

  len2 = len;
  tmp1 = cf_cgi_url_encode(tmp2,&len2);

  if(cf_cfg_get_value_bool(cfg,"Directives:ReplaceNormal") && title == NULL) {
    title = strdup(tmp2);
    len1  = len;
  }

  /* {{{ generate <a href */
  cf_str_chars_append(content,"<a href=\"",9);
  cf_str_chars_append(content,tmp2,len);

  if(cf_cfg_get_value_bool(cfg,"Directives:SetRelNoFollow")) cf_str_chars_append(content,"\" rel=\"nofollow",15);

  if((target = cf_cfg_get_value(cfg,"Directives:Link:Target")) != NULL || (target_ext = cf_cfg_get_value(cfg,"Directives:Link:ExternTarget")) != NULL) {
    if((hostname = getenv("SERVER_NAME")) != NULL) {
      if(cf_strncmp(uri+7,hostname,strlen(hostname)) == 0 && target) {
        cf_str_chars_append(content,"\" target=\"",10);
        cf_str_chars_append(content,target->sval,strlen(target->sval));
      }
      else if(target_ext) {
        cf_str_chars_append(content,"\" target=\"",10);
        cf_str_chars_append(content,target_ext->sval,strlen(target_ext->sval));
      }
    }
    /* fallback: SERVER_NAME is not set. Just append default target, if exists */
    else if(target) {
      cf_str_chars_append(content,"\" target=\"",10);
      cf_str_chars_append(content,target->sval,strlen(target->sval));
    }
  }

  cf_str_chars_append(content,"\">",2);
  /* }}} */

  if(title) {
    len1 = strlen(title);

    /*
     * OK, we got the border around the link, now lets generate the link text
     */

    if(flt_directives_lt_toks.elements == 0 && (link_tpl = cf_cfg_get_value(cfg,"Directives:Link:Template")) != NULL) flt_directives_parse_link_tpl(link_tpl->sval);

    if(flt_directives_lt_toks.elements) {
      for(i=0;i<flt_directives_lt_toks.elements;i++) {
        if((tok = cf_array_element_at(&flt_directives_lt_toks,i)) != NULL) {
          switch(tok->type) {
            case FLT_DIRECTIVES_TOK_TITLE:
              cf_str_chars_append(content,"<span class=\"lnk-title\">",24);
              cf_str_chars_append(content,title,len1);
              cf_str_chars_append(content,"</span>",7);
              break;

            case FLT_DIRECTIVES_TOK_URI:
              cf_str_chars_append(content,"<span class=\"lnk-uri\">",22);
              cf_str_chars_append(content,tmp2,len);
              cf_str_chars_append(content,"</span>",7);
              break;
            default:
              cf_str_chars_append(content,tok->tok,tok->type);
              break;
          }
        }
      }
    }
    else cf_str_chars_append(content,title,len1);
  }
  else cf_str_chars_append(content,tmp2,len);

  /*
   * generate rest of the link and the cite
   */
  if(cite && sig == 0) {
    cf_str_chars_append(cite,"[link:",6);
    cf_str_chars_append(cite,tmp2,len);
    if(title) {
      cf_str_chars_append(cite,"@title=",7);
      cf_str_chars_append(cite,title,len1);
    }
    cf_str_char_append(cite,']');
  }

  cf_str_chars_append(content,"</a>",4);


  if(icons && (cfg_icos = cf_cfg_get_value(cfg,"Directives:Link:Icons")) != NULL && cf_cfg_get_value_bool(cfg,"Directives:Link:ShowIcons")) flt_directives_replace(content,cfg_icos->sval,tmp2,len,tmp1,len2,title,len1);

  free(tmp1);
  free(tmp2);
  free((void *)uri);
}
/* }}} */

/* {{{ flt_directives_execute */
int flt_directives_execute(cf_configuration_t *cfg,cf_cl_thread_t *thread,const u_char *directive,const u_char **parameters,size_t plen,cf_string_t *bco,cf_string_t *bci,cf_string_t *content,cf_string_t *cite,const u_char *qchars,int sig) {
  size_t len = 0,i,len1 = 0;
  int xhtml = cf_cfg_get_value_bool(cfg,"DF:XHTMLMode");
  cf_cfg_config_value_t *target,*refs;
  u_int64_t tid,mid;
  u_char *ptr,*tmp,*tmp1 = NULL,**list = NULL,*title_alt = NULL,*tmp2 = NULL,*uname = cf_hash_get(GlobalValues,"UserName",8);
  cf_readmode_t *rm = cf_hash_get(GlobalValues,"RM",2);
  int go = 1;
  cf_string_t tmpstr;
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
          if(!flt_directives_is_relative_uri(tmp1,len)) go = 0;
        }
        else go = 0;
      }

      if(go) {
        tmp2 = htmlentities(tmp1,1);
        len = strlen(tmp2);

        if(cf_cfg_get_value_bool(cfg,"Directives:ShowAsLink:Image")) flt_directives_generate_uri(tmp1,title_alt,content,NULL,sig,fdc,fvc,0);
        else {
          cf_str_chars_append(content,"<img src=\"",10);
          cf_str_chars_append(content,tmp2,len);

          if(title_alt) {
            cf_str_chars_append(content,"\" alt=\"",7);
            cf_str_chars_append(content,title_alt,len1);
            cf_str_chars_append(content,"\" title=\"",9);
            cf_str_chars_append(content,title_alt,len1);
          }
          else cf_str_chars_append(content,"\" alt=\"",7);

          if(xhtml)  cf_str_chars_append(content,"\"/>",3);
          else cf_str_chars_append(content,"\">",2);
        }

        if(cite && sig == 0) {
          cf_str_chars_append(cite,"[image:",7);
          cf_str_chars_append(cite,tmp2,len);
          if(title_alt) {
            cf_str_chars_append(cite,"@alt=",5);
            cf_str_chars_append(cite,title_alt,len1);
          }
          cf_str_char_append(cite,']');
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
          if(!flt_directives_is_relative_uri(parameter,len)) go = 0;
        }
        else go = 0;
      }

      if(go) {
        tmp2 = htmlentities(parameter,1);
        len = strlen(tmp2);

        if(cf_cfg_get_value_bool(cfg,"Directives:ShowAsLink:Iframe")) flt_directives_generate_uri(parameter,NULL,content,NULL,sig,fdc,fvc,1);
        else {
          cf_str_chars_append(content,"<iframe src=\"",13);
          cf_str_chars_append(content,tmp2,len);
          cf_str_chars_append(content,"\" width=\"90%\" height=\"90%\"><a href=\"",36);
          cf_str_chars_append(content,tmp2,len);

          if((target = cf_cfg_get_value(cfg,"Directives:Link:Target")) != NULL) {
            cf_str_chars_append(content,"\" target=\"",10);
            cf_str_chars_append(content,target->sval,target->ival);
          }

          cf_str_chars_append(content,"\">",2);
          cf_str_chars_append(content,tmp2,len);
          cf_str_chars_append(content,"</a></iframe>",13);
        }

        if(cite && sig == 0) {
          cf_str_chars_append(cite,"[iframe:",8);
          cf_str_chars_append(cite,tmp2,len);
          cf_str_char_append(cite,']');
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
        tid = cf_str_to_uint64(parameter+2);
        mid = cf_str_to_uint64(tmp1);
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
      len   = cf_nsplit(parameter,";",&list,2);

      if(len == 2 && (refs = cf_cfg_get_value(cfg,"Directives:ReferenceURI")) != NULL && refs->type == CF_ASM_ARG_ARY) {
        for(i=0;i<refs->alen;++i) {
          if(cf_strcmp(refs->avals[i].avals[0].sval,list[0]) == 0) {
            /* check for title */
            if((title_alt = strstr(list[1],"@title=")) != NULL) {
              if(*(title_alt+7) && flt_directives_is_valid_title(title_alt+7,strlen(title_alt+7))) {
                if(title_alt != list[1]) tmp2 = strndup(list[1],title_alt-list[1]);
                else tmp2 = NULL;
                title_alt = htmlentities(title_alt+7,0);
              }
              else return FLT_DECLINE;
            }
            else tmp2 = strdup(list[1]);

            tmp = htmlentities(list[0],0);
            tmp1 = htmlentities(uri->uri,0);
            ptr = htmlentities(tmp2,0);

            cf_str_init(&tmpstr);
            cf_str_chars_append(&tmpstr,refs->avals[i].avals[1].sval,refs->avals[i].avals[1].ival);
            if(tmp2) cf_str_chars_append(&tmpstr,tmp2,strlen(tmp2));

            flt_directives_generate_uri(tmpstr.content,title_alt,content,NULL,0,fdc,fvc,1);

            if(sig == 0 && cite) {
              cf_str_chars_append(cite,"[ref:",5);
              cf_str_chars_append(cite,tmp,strlen(tmp));
              cf_str_char_append(cite,';');
              if(tmp2) cf_str_chars_append(cite,tmp2,strlen(tmp2));
              if(title_alt) {
                cf_str_chars_append(cite,"@title=",7);
                cf_str_chars_append(cite,title_alt,strlen(title_alt));
              }
              cf_str_char_append(cite,']');
            }

            cf_str_cleanup(&tmpstr);
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
    /* {{{ [char:] */
    else if(cf_strcmp(directive,"char") == 0) {
      u_char *start = parameter,utf8_chr[10];
      u_int32_t chr;
      int cls;

      if(*start == 'U' || *start == 'u') {
        start = parameter + 1;
        if(*start == '-' || *start == '+') start += 1;
      }

      if(strlen(start) > 6 || strlen(start) < 1) return FLT_DECLINE;

      chr = strtoull(start,NULL,16);
      cls = cf_classify_char(chr);

      if(cls == CF_UNI_CLS_CC || cls == CF_UNI_CLS_CF || cls == CF_UNI_CLS_CS || cls == CF_UNI_CLS_CN || cls == -1) return FLT_DECLINE;

      if((cls = unicode_to_utf8(chr,utf8_chr,10)) == EINVAL) return FLT_DECLINE;
      cf_str_chars_append(content,utf8_chr,cls);

      if(sig == 0 && cite) {
        cf_str_chars_append(cite,"[char:",6);
        cf_str_cstr_append(cite,parameter);
        cf_str_char_append(cite,']');
      }

      return FLT_OK;
    }
    /* }}} */

  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_directives_execute_irony */
int flt_directives_execute_irony(cf_configuration_t *fdc,cf_configuration_t *fvc,cl_thread_t *thread,const u_char *directive,const u_char **parameters,size_t plen,cf_string_t *bco,cf_string_t *bci,cf_string_t *content,cf_string_t *cite,const u_char *qchars,int sig) {
  cf_str_chars_append(bco,"<span class=\"IRONY\">",20);
  cf_str_str_append(bco,content);
  cf_str_chars_append(bco,"</span>",7);

  if(sig && bci && cite) {
    cf_str_chars_append(bci,"[irony]",7);
    cf_str_str_append(bci,cite);
    cf_str_chars_append(bci,"[/irony]",8);
  }

  return FLT_OK;
}
/* }}} */

/* {{{ flt_directives_is_unwanted */
int flt_directives_is_unwanted(cf_configuration_t *cfg,const u_char *link,size_t len) {
  size_t i;
  int erroffset;
  pcre *regexp;
  char *error;
  cf_cfg_config_value_t *uwls = cf_cfg_get_value(cfg,"Directives:UnwantedLinks");

  if(uwls == NULL || uwls->type != CF_ASM_ARG_ARY) return 0;

  for(i=0;i<uwls->alen;++i) {
    if((regexp = pcre_compile(uwls->avals[i].sval, 0, (const char **)&error, &erroffset, NULL)) == NULL) {
      fprintf(stderr,"flt_directives: error in pattern '%s' (offset %d): %s\n",uwls->avals[i].sval,erroffset,error);
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
int flt_directives_validate(cf_configuration_t *cfg,const u_char *directive,const u_char **parameters,size_t plen,cf_tpl_variable_t *var) {
  u_char *parameter = (u_char *)parameters[0];

  u_char *tmp1,*tmp2,*ptr,**list,*err;

  flt_directives_ref_uri_t *uri;

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
            if(cf_cfg_get_value_bool(cfg,"Directives:WarnBadLinks") == 0) return FLT_DECLINE;

            if((err = cf_get_error_message(cfg,"E_posting_links",&len)) != NULL) {
              cf_tpl_var_addvalue(var,TPL_VARIABLE_STRING,err,len);
              free(err);
            }

            return FLT_ERROR;
          }
          else {
            if(flt_directives_is_unwanted(cfg,tmp1,len) == -1) {
              if((err = cf_get_error_message(cfg,"E_unwanted_link",&len)) != NULL) {
                cf_tpl_var_addvalue(var,TPL_VARIABLE_STRING,err,len);
                free(err);
              }
              else fprintf(stderr,"flt_directives: Unwanted link but could not find error message!\n");

              return FLT_ERROR;
            }
          }
        }
        else {
          if(cf_cfg_get_value_bool(cfg,"Directives:WarnBadLinks") == 0) return FLT_DECLINE;

          if((err = cf_get_error_message(cfg,"E_posting_links",&len)) != NULL) {
            cf_tpl_var_addvalue(var,TPL_VARIABLE_STRING,err,len);
            free(err);
          }
          return FLT_ERROR;
        }
      }
      else {
        if(flt_directives_is_unwanted(cfg,tmp1,len) == -1) {
          if((err = cf_get_error_message(cfg,"E_unwanted_link",&len)) != NULL) {
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
    if(cf_cfg_get_value_bool(cfg,"Directives:WarnBadLinks") == 0) return FLT_DECLINE;

    /* {{{ [image:] */
    if(cf_strcmp(directive,"image") == 0) {
      if((ptr = strstr(parameter,"@alt=")) != NULL) tmp1      = strndup(parameter,ptr-parameter);
      else tmp1 = (u_char *)parameter;

      len = strlen(tmp1);

      if(is_valid_link(tmp1) != 0) {
        if(cf_strncmp(tmp1,"..",2) == 0 || *tmp1 == '/' || *tmp1 == '?') {
          if(!flt_directives_is_relative_uri(tmp1,len)) {
            if((err = cf_get_error_message(cfg,"E_posting_links",&len)) != NULL) {
              cf_tpl_var_addvalue(var,TPL_VARIABLE_STRING,err,len);
              free(err);
            }
            return FLT_ERROR;
          }
        }
        else {
          if((err = cf_get_error_message(cfg,"E_posting_links",&len)) != NULL) {
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
      if(is_valid_link(parameter) != 0) {
        if(cf_strncmp(parameter,"..",2) == 0 || *parameter == '/' || *parameter == '?') {
          len = strlen(parameter);

          if(!flt_directives_is_relative_uri(parameter,len)) {
            if((err = cf_get_error_message(cfg,"E_posting_links",&len)) != NULL) {
              cf_tpl_var_addvalue(var,TPL_VARIABLE_STRING,err,len);
              free(err);
            }
            return FLT_ERROR;
          }
        }
        else {
          if((err = cf_get_error_message(cfg,"E_posting_links",&len)) != NULL) {
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
    if(cf_cfg_get_value_bool(cfg,"Directives:WarnBadLinks") == 0) return FLT_DECLINE;

    /* {{{ [pref:] */
    if(cf_strcmp(directive,"pref") == 0) {
      tid = mid = 0;

      if(!flt_directives_is_valid_pref(parameter,&tmp1,&tmp2)) {
        if((err = cf_get_error_message(cfg,"E_posting_links",&len)) != NULL) {
          cf_tpl_var_addvalue(var,TPL_VARIABLE_STRING,err,len);
          free(err);
        }
        return FLT_ERROR;
      }
    }
    /* }}} */
    /* {{{ [ref:] */
    else if(cf_strcmp(directive,"ref") == 0) {
      len   = cf_nsplit(parameter,";",&list,2);

      if(len == 2) {
        for(i=0;i<flt_directives_ref_uris.elements;i++) {
          uri = cf_array_element_at(&flt_directives_ref_uris,i);

          if(cf_strcmp(uri->id,list[0]) == 0) return FLT_OK;
        }

        if(list) {
          for(i=0;i<len;++i) free(list[i]);
          free(list);
        }

        if((err = cf_get_error_message(cfg,"E_posting_links",&len)) != NULL) {
          cf_tpl_var_addvalue(var,TPL_VARIABLE_STRING,err,len);
          free(err);
        }
        return FLT_ERROR;
      }
    }
    /* }}} */
    /* {{{ [char:] */
    else if(cf_strcmp(directive,"char") == 0) {
      u_char *start = parameter;
      u_int64_t chr;
      int cls;

      if(*start == 'U' || *start == 'u') {
        start = parameter + 1;
        if(*start == '-' || *start == '+') start += 1;
      }

      if(strlen(start) > 6 || strlen(start) < 1) {
        if((err = cf_get_error_message(cfg,"E_invalid_char",&len)) != NULL) {
          cf_tpl_var_addvalue(var,TPL_VARIABLE_STRING,err,len);
          free(err);
        }
        return FLT_ERROR;
      }

      chr = strtoull(start,NULL,16);
      cls = cf_classify_char(chr);

      if(cls == CF_UNI_CLS_CC || cls == CF_UNI_CLS_CF || cls == CF_UNI_CLS_CS || cls == CF_UNI_CLS_CN || cls == -1) {
        if((err = cf_get_error_message(cfg,"E_invalid_char",&len)) != NULL) {
          cf_tpl_var_addvalue(var,TPL_VARIABLE_STRING,err,len);
          free(err);
        }
        return FLT_ERROR;
      }

      return FLT_OK;
    }
    /* }}} */
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_directives_check_for_pref */
int flt_directives_check_for_pref(cf_configuration_t *cfg,const u_char *link,size_t len) {
  size_t i;
  int erroffset;
  pcre *regexp;
  char *error;
  cf_cfg_config_value_t *purls = cf_cfg_get_value(cfg,"Directives:PostingUrl");

  if(purls == NULL || purls->type != CF_ASM_ARG_ARY) return 0;

  for(i=0;i<purls->alen;++i) {
    if((regexp = pcre_compile(purls->avals[i].sval, 0, (const char **)&error, &erroffset, NULL)) == NULL) {
      fprintf(stderr,"flt_directives: error in pattern '%s' (offset %d): %s\n",purls->avals[i].sval,erroffset,error);
      continue;
    }

    if(pcre_exec(regexp,NULL,link,len,0,0,NULL,0) >= 0) 
      pcre_free(regexp);
      return 1;
    }

    pcre_free(regexp);
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
        if(*(ptr+1) == '=' || *(ptr+1) == '/') *tid = cf_str_to_uint64(ptr+2);
        found |= 1;
        break;
      case 'm':
        if(*(ptr+1) == '=' || *(ptr+1) == '/') *mid = cf_str_to_uint64(ptr+2);
        found |= 2;
        break;
    }
  }

  return (found & 1) && (found & 2);
}
/* }}} */

/* {{{ flt_directives_rewrite */
int flt_directives_rewrite(cf_hash_t *head,cf_configuration_t *cfg,cf_message_t *p,cf_cl_thread_t *thr,int sock,int mode) {
  cf_string_t new_content;
  register u_char *ptr;
  u_char *safe,*link,*title;
  u_int64_t tid,mid;
  size_t len;

  cf_str_init(&new_content);

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

          if(flt_directives_check_for_pref(cfg,link,len)) {
            if(flt_directives_parse_link_for_pref(link,&tid,&mid)) {
              cf_str_chars_append(&new_content,"[pref:t=",8);
              cf_uint64_to_str(&new_content,tid);
              cf_str_chars_append(&new_content,";m=",3);
              cf_uint64_to_str(&new_content,mid);
              if(title) cf_str_chars_append(&new_content,title,ptr-title);
              cf_str_char_append(&new_content,']');
              continue;
            }
          }

          ptr = safe;
        }
      default:
        cf_str_char_append(&new_content,*ptr);
    }
  }

  cf_str_cleanup(&p->content);
  p->content.content = new_content.content;
  p->content.len = new_content.len;
  p->content.reserved = new_content.reserved;

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_directives_suial_set */
int flt_directives_suial_set(cf_hash_t *head,cf_configuration_t *cfg,cf_cl_thread_t *thread,cf_message_t *msg,cf_tpl_variable_t *hash) {
  cf_cfg_config_value_t *target = cf_cfg_get_value(cfg,"Directives:Link:Target");

  if(cf_cfg_get_value_bool(cfg,"Directives:ShowAsLink:UserImage") == 0) cf_tpl_hashvar_setvalue(hash,"showimage",TPL_VARIABLE_INT,1); //TODO: sinnvoller name
  if(target) cf_tpl_hashvar_setvalue(hash,"target",TPL_VARIABLE_STRING,target->sval,target->ival); //TODO: sinnvoller name

  return FLT_OK;
}
/* }}} */

/* {{{ flt_directives_init */
int flt_directives_init(cf_hash_t *cgi,cf_configuration_t *dc,cf_configuration_t *vc) {
  cf_html_register_directive("link",flt_directives_execute,CF_HTML_DIR_TYPE_ARG|CF_HTML_DIR_TYPE_INLINE);
  cf_html_register_directive("pref",flt_directives_execute,CF_HTML_DIR_TYPE_ARG|CF_HTML_DIR_TYPE_INLINE);
  cf_html_register_directive("ref",flt_directives_execute,CF_HTML_DIR_TYPE_ARG|CF_HTML_DIR_TYPE_INLINE);
  cf_html_register_directive("image",flt_directives_execute,CF_HTML_DIR_TYPE_ARG|CF_HTML_DIR_TYPE_INLINE);
  cf_html_register_directive("iframe",flt_directives_execute,CF_HTML_DIR_TYPE_ARG|CF_HTML_DIR_TYPE_INLINE);
  cf_html_register_directive("irony",flt_directives_execute_irony,CF_HTML_DIR_TYPE_NOARG|CF_HTML_DIR_TYPE_BLOCK);

  cf_html_register_directive("char",flt_directives_execute,CF_HTML_DIR_TYPE_ARG|CF_HTML_DIR_TYPE_INLINE);

  cf_html_register_validator("link",flt_directives_validate,CF_HTML_DIR_TYPE_ARG|CF_HTML_DIR_TYPE_INLINE);
  cf_html_register_validator("pref",flt_directives_validate,CF_HTML_DIR_TYPE_ARG|CF_HTML_DIR_TYPE_INLINE);
  cf_html_register_validator("ref",flt_directives_validate,CF_HTML_DIR_TYPE_ARG|CF_HTML_DIR_TYPE_INLINE);
  cf_html_register_validator("image",flt_directives_validate,CF_HTML_DIR_TYPE_ARG|CF_HTML_DIR_TYPE_INLINE);
  cf_html_register_validator("iframe",flt_directives_validate,CF_HTML_DIR_TYPE_ARG|CF_HTML_DIR_TYPE_INLINE);

  cf_html_register_validator("char",flt_directives_validate,CF_HTML_DIR_TYPE_ARG|CF_HTML_DIR_TYPE_INLINE);

  return FLT_DECLINE;
}
/* }}} */

/**
 * Config directives:
 * Directives:PostingUrl = ("URL1","URL2");
 * Directives:ShowAsLink:Iframe = Yes|No;
 * Directives:ShowAsLink:Image = Yes|No;
 * Directives:ShowAsLink:UserImage = Yes|No;
 * Directives:Link:Target = "target";
 * Directives:Link:ExternTarget = "target";
 * Directives:SetRelNoFollow = Yes|No;
 * Directives:ReferenceURI = (
 *   ("id","uri"),
 *   ("id1","uri")
 * );
 * Directives:Link:Template = "template";
 * Directives:WarnBadLinks = Yes|No;
 * Directives:Link:Icons = "<img src=\"ico1.gif\">";
 * Directives:Link:ShowIcons = Yes|No;
 * Directives:UnwantedLinks = ("^linkpattern","^linkpattern2");
 * Directives:ReplaceNormal = Yes|No;
 */

cf_handler_config_t flt_directives_handlers[] = {
  { PERPOST_VAR_HANDLER, flt_directives_suial_set },
  { INIT_HANDLER,        flt_directives_init },
  { NEW_POST_HANDLER,    flt_directives_rewrite },
  { 0, NULL }
};

cf_module_config_t flt_directives = {
  MODULE_MAGIC_COOKIE,
  flt_directives_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};

/* eof */
