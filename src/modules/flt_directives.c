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

static u_char *flt_directives_link      = NULL;
static int flt_directives_imagesaslink  = 0;
static int flt_directives_iframesaslink = 0;

typedef struct {
  u_char *id;
  u_char *uri;
} t_flt_directives_ref_uri;


#define FLT_DIRECTIVES_TOK_TITLE  0
#define FLT_DIRECTIVES_TOK_URI   -1

typedef struct {
  int type;
  u_char *tok;
} t_flt_directives_lt_tok;

static t_array flt_directives_ref_uris = { 0, 0, 0, NULL, NULL };
static t_array flt_directives_lt_toks  = { 0, 0, 0, NULL, NULL };

/* {{{ flt_directives_is_valid_pref */
int flt_directives_is_valid_pref(const u_char *parameter,u_char **tmp) {
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

/* {{{ flt_directives_generate_uri */
void flt_directives_generate_uri(const u_char *uri,const u_char *title,t_string *content,t_string *cite,int sig) {
  u_char *tmp2;
  size_t len,len1,i;
  t_flt_directives_lt_tok *tok;

  if(title) {
    len1 = strlen(title);
    tmp2 = htmlentities(uri,1);
    len = strlen(tmp2);

    str_chars_append(content,"<a href=\"",9);
    str_chars_append(content,tmp2,len);

    if(flt_directives_link) {
      str_chars_append(content,"\" target=\"",10);
      str_chars_append(content,flt_directives_link,strlen(flt_directives_link));
    }

    str_chars_append(content,"\">",2);

    /*
     * OK, we got the border around the link, now lets generate the link text
     */

    if(flt_directives_lt_toks.elements) {
      for(i=0;i<flt_directives_lt_toks.elements;i++) {
        if((tok = array_element_at(&flt_directives_lt_toks,i)) != NULL) {
          switch(tok->type) {
            case FLT_DIRECTIVES_TOK_TITLE:
              str_chars_append(content,title,len1);
              break;
            case FLT_DIRECTIVES_TOK_URI:
              str_chars_append(content,tmp2,len);
              break;
            default:
              str_chars_append(content,tok->tok,tok->type);
              break;
          }
        }
      }
    }
    else {
      str_chars_append(content,title,len1);
    }

    /*
     * generate rest of the link
     */

    if(cite && sig == 0) {
      str_chars_append(cite,"[link:",6);
      str_chars_append(cite,tmp2,len);
      str_chars_append(cite,"@title=",7);
      str_chars_append(cite,title,len1);
      str_char_append(cite,']');
    }

    str_chars_append(content,"</a>",4);
    free(tmp2);
  }
  else {
    tmp2 = htmlentities(uri,1);
    len = strlen(tmp2);

    str_chars_append(content,"<a href=\"",9);
    str_chars_append(content,tmp2,len);

    if(flt_directives_link) {
      str_chars_append(content,"\" target=\"",10);
      str_chars_append(content,flt_directives_link,strlen(flt_directives_link));
    }

    str_chars_append(content,"\">",2);
    str_chars_append(content,tmp2,len);
    str_chars_append(content,"</a>",4);

    if(cite && sig == 0) {
      str_chars_append(cite,"[link:",6);
      str_chars_append(cite,tmp2,len);
      str_char_append(cite,']');
    }

    free(tmp2);
  }
}
/* }}} */

/* {{{ flt_directives_execute */
int flt_directives_execute(t_configuration *fdc,t_configuration *fvc,const u_char *directive,const u_char *parameter,t_string *content,t_string *cite,const u_char *qchars,int sig) {
  size_t len,i,len1 = 0;
  t_name_value *xhtml = cfg_get_first_value(fdc,"XHTMLMode");
  u_int64_t tid,mid;
  u_char *ptr,*tmp,*tmp1 = NULL,**list = NULL,*title_alt = NULL,*tmp2;
  t_name_value *vs = cfg_get_first_value(fdc,cf_hash_get(GlobalValues,"UserName",8) ? "UPostingURL" : "PostingURL");
  t_flt_directives_ref_uri *uri;
  int go = 1;

  while(isspace(*parameter)) ++parameter;

  if(*directive == 'l') {
    /* {{{ [link:] */
    if(cf_strcmp(directive,"link") == 0) {
      if((ptr = strstr(parameter,"@title=")) != NULL) {
        tmp1      = strndup(parameter,ptr-parameter);
        title_alt = htmlentities(ptr + 7,1);
      }
      else {
        tmp1 = (u_char *)parameter;
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
        flt_directives_generate_uri(tmp1,title_alt,content,cite,sig);
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
        tmp1      = strndup(parameter,ptr-parameter);
        len       = ptr - parameter;
        title_alt = htmlentities(ptr + 5,1);
        len1      = strlen(title_alt);
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
        if(flt_directives_imagesaslink) {
          flt_directives_generate_uri(tmp1,title_alt,content,NULL,sig);
        }
        else {
          tmp2 = htmlentities(tmp1,1);
          len = strlen(tmp2);

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

          free(tmp2);
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
        if(flt_directives_iframesaslink) {
          flt_directives_generate_uri(parameter,NULL,content,NULL,sig);
        }
        else {
          tmp2 = htmlentities(parameter,1);
          len = strlen(tmp2);

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

          free(tmp2);
        }

        if(cite && sig == 0) {
          str_chars_append(cite,"[iframe:",8);
          str_chars_append(cite,tmp2,len);
          str_char_append(cite,']');
        }

        return FLT_OK;
      }
    }
    /* }}} */
  }
  else {
    /* {{{ [pref:] */
    if(cf_strcmp(directive,"pref") == 0) {
      tid = mid = 0;

      if(flt_directives_is_valid_pref(parameter,&tmp1)) {
        tid = str_to_u_int64(parameter+2);
        mid = str_to_u_int64(tmp1);
        tmp1 = get_link(vs->values[0],tid,mid);

        if(sig == 0 && cite) {
          str_chars_append(cite,"[link:",6);
          str_chars_append(cite,tmp1,strlen(tmp1));
          str_char_append(cite,']');
        }

        str_chars_append(content,"<a href=\"",9);
        str_chars_append(content,tmp1,strlen(tmp1));

        if(flt_directives_link) {
          str_chars_append(content,"\" target=\"",10);
          str_chars_append(content,flt_directives_link,strlen(flt_directives_link));
        }

        str_chars_append(content,"\">",2);
        str_chars_append(content,tmp1,strlen(tmp1));
        str_chars_append(content,"</a>",4);

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
            tmp = htmlentities(list[0],1);
            tmp1 = htmlentities(uri->uri,1);
            tmp2 = htmlentities(list[1],1);

            str_chars_append(content,"<a href=\"",9);
            str_chars_append(content,tmp1,strlen(tmp1));
            str_chars_append(content,tmp2,strlen(tmp2));

            if(flt_directives_link) {
              str_chars_append(content,"\" target=\"",10);
              str_chars_append(content,flt_directives_link,strlen(flt_directives_link));
            }

            str_chars_append(content,"\">",2);
            str_chars_append(content,tmp1,strlen(tmp1));
            str_chars_append(content,tmp2,strlen(tmp2));
            str_chars_append(content,"</a>",4);

            if(sig == 0 && cite) {
              str_chars_append(cite,"[ref:",5);
              str_chars_append(cite,tmp,strlen(tmp));
              str_char_append(cite,';');
              str_chars_append(cite,tmp2,strlen(tmp2));
              str_char_append(cite,']');
            }

            free(list[0]);
            free(list[1]);
            free(list);
            free(tmp);
            free(tmp1);
            free(tmp2);

            return FLT_OK;
          }
        }

        if(list) {
          for(i=0;i<len;i++) free(list[i]);
          free(list);
        }
      }
    }
    /* }}} */
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ module configuration */

/* {{{ directive handlers */
int flt_directives_handle_iframe(t_configfile *cfile,t_conf_opt *opt,u_char **args,int argnum) {
  flt_directives_iframesaslink = cf_strcmp(args[0],"yes") == 0;
  return 0;
}

int flt_directives_handle_image(t_configfile *cfile,t_conf_opt *opt,u_char **args,int argnum) {
  flt_directives_imagesaslink = cf_strcmp(args[0],"yes") == 0;
  return 0;
}

int flt_directives_handle_link(t_configfile *cfile,t_conf_opt *opt,u_char **args,int argnum) {
  if(flt_directives_link) free(flt_directives_link);
  flt_directives_link = strdup(args[0]);

  return 0;
}

void flt_directives_cleanup_entry(void *e) {
  t_flt_directives_ref_uri *uri = (t_flt_directives_ref_uri *)e;
  free(uri->uri);
  free(uri->id);
}

int flt_directives_handle_ref(t_configfile *cfile,t_conf_opt *opt,u_char **args,int argnum) {
  t_flt_directives_ref_uri uri;

  uri.id  = strdup(args[0]);
  uri.uri = strdup(args[1]);

  if(flt_directives_ref_uris.element_size == 0) array_init(&flt_directives_ref_uris,sizeof(uri),flt_directives_cleanup_entry);

  array_push(&flt_directives_ref_uris,&uri);

  return 0;
}

int flt_directives_handle_lt(t_configfile *cfile,t_conf_opt *opt,u_char **args,int argnum) {
  u_char *ptr;
  t_string str;
  t_flt_directives_lt_tok tok;

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
              tok.tok  = htmlentities(str.content,1);
              tok.type = strlen(tok.tok);
              array_push(&flt_directives_lt_toks,&tok);

              tok.type = FLT_DIRECTIVES_TOK_TITLE;
              tok.tok  = NULL;
              array_push(&flt_directives_lt_toks,&tok);

              str.len = 0;

              break;

            case 'u':
              tok.tok  = htmlentities(str.content,1);
              tok.type = strlen(tok.tok);
              array_push(&flt_directives_lt_toks,&tok);

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

void flt_directives_cleanup(void) {
  if(flt_directives_link) free(flt_directives_link);
  if(flt_directives_ref_uris.element_size > 0) array_destroy(&flt_directives_ref_uris);
}

t_conf_opt flt_directives_config[] = {
  { "ShowIframeAsLink",     flt_directives_handle_iframe,   CFG_OPT_CONFIG|CFG_OPT_USER,  NULL },
  { "ShowImageAsLink",      flt_directives_handle_image,    CFG_OPT_CONFIG|CFG_OPT_USER,  NULL },
  { "PostingLinkTarget",    flt_directives_handle_link,     CFG_OPT_CONFIG|CFG_OPT_USER,  NULL },
  { "ReferenceURI",         flt_directives_handle_ref,      CFG_OPT_CONFIG|CFG_OPT_USER,  NULL },
  { "LinkTemplate",         flt_directives_handle_lt,       CFG_OPT_CONFIG|CFG_OPT_USER,  NULL },
  { NULL, 0, NULL, NULL }
};

t_handler_config flt_directives_handlers[] = {
  { DIRECTIVE_FILTER,    flt_directives_execute },
  { 0, NULL }
};

t_module_config flt_directives = {
  flt_directives_config,
  flt_directives_handlers,
  NULL,
  NULL,
  NULL,
  flt_directives_cleanup
};
/* }}} */

/* eof */
