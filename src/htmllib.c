/**
 * \file htmllib.c
 * \author Christian Kruse
 *
 * This library contains some functions to display messages in HTML
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "template.h"
#include "readline.h"
#include "charconvert.h"
#include "clientlib.h"
#include "htmllib.h"
/* }}} */


/* {{{ run_directive_filters */
int run_directive_filters(const u_char *directive,const u_char *parameter,t_string *content,t_string *cite,const u_char *qchars,int sig) {
  int ret = FLT_DECLINE;
  t_handler_config *handler;
  size_t i;
  t_directive_filter fkt;

  if(Modules[DIRECTIVE_FILTER].elements) {
    for(i=0;i<Modules[DIRECTIVE_FILTER].elements && ret == FLT_DECLINE;i++) {
      handler = array_element_at(&Modules[DIRECTIVE_FILTER],i);
      fkt     = (t_directive_filter)handler->func;
      ret     = fkt(&fo_default_conf,&fo_view_conf,directive,parameter,content,cite,qchars,sig);
    }
  }

  return ret;
}
/* }}} */
/* {{{ run_content_filters */
void run_content_filters(int mode,t_cl_thread *thr,t_string *content,t_string *cite,const u_char *qchars) {
  int ret = FLT_OK;
  t_handler_config *handler;
  size_t i;
  t_content_filter fkt;

  if(Modules[mode].elements) {
    for(i=0;i<Modules[mode].elements;i++) {
      handler = array_element_at(&Modules[mode],i);
      fkt     = (t_content_filter)handler->func;
      ret     = fkt(&fo_default_conf,&fo_view_conf,thr,content,cite,qchars);
    }
  }
}
/* }}} */

/* {{{ next_line_is_no_quote_line */
int next_line_is_no_quote_line(const u_char *ptr) {
  int eq;

  for(;*ptr && ((eq = cf_strncmp(ptr,"<br />",6)) == 0 || *ptr == ' ');ptr++) {
    if(eq == 0) ptr += 5;
  }

  if(*ptr == (u_char)127) return 0;
  return 1;
}
/* }}} */

/* {{{ msg_to_html */
void msg_to_html(t_cl_thread *thread,const u_char *msg,t_string *content,t_string *cite,u_char *quote_chars,int max_sig_lines,int show_sig) {
  t_name_value *cs   = cfg_get_first_value(&fo_default_conf,"ExternCharset");
  t_name_value *xmlm = cfg_get_first_value(&fo_default_conf,"XHTMLMode");
  const u_char *ptr,*tmp,*ptr1;
  u_char *qchars;
  size_t qclen;
  int linebrk = 0,quotemode = 0,sig = 0,utf8 = cf_strcmp(cs->values[0],"UTF-8") == 0,line = 0,rc,xml,run = 1;
  u_char *directive,*parameter,*safe,*buff;

  xml = cf_strcmp(xmlm->values[0],"yes");

  if(utf8 || (qchars = htmlentities_charset_convert(quote_chars,"UTF-8",cs->values[0],&qclen,0)) == NULL) {
    qchars = strdup(quote_chars);
    qclen  = strlen(qchars);
  }

  run_content_filters(PRE_CONTENT_FILTER,thread,content,cite,qchars);

  /* first line has no linebreak, so append quoting chars to cite */
  if(cite) str_chars_append(cite,qchars,qclen);

  for(ptr=msg;*ptr && run;ptr++) {
    switch(*ptr) {
      case '[':
        safe = (u_char *)ptr;

        /* ok, parse this directive */
        for(ptr1=ptr;*ptr1 && *ptr1 != ':' && *ptr1 != ']' && !isspace(*ptr1);++ptr1);
        if(*ptr1 == ':') {
          tmp = ptr1;

          for(ptr1++;*ptr1 && *ptr1 != ']';++ptr1);
          if(*ptr1 == ']') {
            directive = strndup(ptr+1,tmp-ptr-1);
            buff      = strndup(tmp+1,ptr1-tmp-1);
            parameter = htmlentities_decode(buff);
            free(buff);

            rc = run_directive_filters(directive,parameter,content,cite,qchars,sig);

            free(directive);
            free(parameter);

            if(rc == FLT_DECLINE) {
              ptr = safe;
              goto default_action;
            }
            else ptr = ptr1;
          }
          else {
            ptr = safe;
            goto default_action;
          }
        }
        else {
          ptr = safe;
          goto default_action;
        }
        break;
      case '<':
        if(cf_strncmp(ptr,"<br />",6) == 0) {
          linebrk = 1;
          line++;

          if(xml) str_chars_append(content,"<br />",6);
          else    str_chars_append(content,"<br>",4);

          if(sig && max_sig_lines > 0 && line >= max_sig_lines) {
            run = 0;
            break;
          }
          if(sig == 0 && cite) {
            str_chars_append(cite,"\n",1);
            str_chars_append(cite,qchars,qclen);
          }

          if(quotemode && next_line_is_no_quote_line(ptr+6)) {
            str_chars_append(content,"</span>",7);
            quotemode = 0;
          }

          ptr += 5;
        }
        else if(cf_strncmp(ptr,"<a href=\"",9) == 0) {
          safe    = (u_char *)ptr;
          ptr    += 9;
          linebrk = 0;
          tmp     = strstr(ptr,"\"");

          if(tmp) {
            directive = strdup("link");
            buff      = strndup(ptr,tmp-ptr);
            parameter = htmlentities_decode(buff);
            free(buff);

            rc = run_directive_filters(directive,parameter,content,cite,qchars,sig);

            free(directive);
            free(parameter);

            if(rc == FLT_DECLINE) {
              ptr = safe;
              goto default_action;
            }
            else ptr = strstr(tmp,"</a>") + 3;
          }
          else {
            ptr = safe;
            goto default_action;
          }
        }
        else if(cf_strncmp(ptr,"<img src=\"",10) == 0) {
          safe    = (u_char *)ptr;
          ptr    += 10;
          linebrk = 0;

          tmp     = strstr(ptr,"\"");

          if(tmp) {
            directive = strdup("image");
            buff      = strndup(ptr,tmp-ptr);
            parameter = htmlentities_decode(buff);
            free(buff);

            run_directive_filters(directive,parameter,content,cite,qchars,sig);

            free(directive);
            free(parameter);

            if(rc == FLT_DECLINE) {
              ptr = safe;
              goto default_action;
            }
            else ptr = strstr(tmp,">");
          }
          else {
            ptr = safe;
            goto default_action;
          }
        }
        else if(cf_strncmp(ptr,"<iframe",7) == 0) {
          safe = (u_char *)ptr;
          ptr += 13;
          tmp = ptr;
          ptr = strstr(ptr,"\"");

          directive = strdup("iframe");
          buff      = strndup(tmp,ptr-tmp);
          parameter = htmlentities_decode(buff);
          free(buff);

          rc = run_directive_filters(directive,parameter,content,cite,qchars,sig);

          free(directive);
          free(parameter);

          if(rc == FLT_DECLINE) {
            ptr = safe;
            goto default_action;
          }
          else ptr = strstr(ptr,"</iframe>") + 8;
        }
        else goto default_action;
        break;

      case 127:
        linebrk = 0;

        if(!quotemode) str_chars_append(content,"<span class=\"q\">",16);
        str_chars_append(content,qchars,qclen);
        quotemode = 1;
        if(sig == 0 && cite) str_chars_append(cite,qchars,qclen);

        break;

      case '_':
        if(cf_strncmp(ptr,"_/_SIG_/_",9) == 0) {
          if(quotemode) {
            str_chars_append(content,"</span>",7);
            quotemode = 0;
          }
        
          /* some users don't like sigs */
          if(!show_sig) {
            run = 0;
            break;
          }

          sig  = 1;
          line = 0;

          if(xml) {
            str_chars_append(content,"<br /><span class=\"sig\">",24);
            str_chars_append(content,"-- <br />",9);
          }
          else {
            str_chars_append(content,"<br><span class=\"sig\">",22);
            str_chars_append(content,"-- <br>",7);
          }

          ptr += 8;
        }
        else goto default_action;
        break;

      default:
        default_action:
        str_chars_append(content,ptr,1);
        if(sig == 0 && cite) str_chars_append(cite,ptr,1);
    }
  }

  if(quotemode || sig) str_chars_append(content,"</span>",7);

  run_content_filters(POST_CONTENT_FILTER,thread,content,cite,qchars);

  free(qchars);
}
/* }}} */

/* eof */
