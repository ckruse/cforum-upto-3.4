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

typedef struct s_directive_callback {
  t_directive_filter callback;
  int type;
} t_directive_callback;

typedef struct s_html_stack {
  u_char *begin;
  u_char *name;
  u_char **args;
  size_t argnum;
} t_html_stack_elem;

static t_cf_hash *registered_directives = NULL;


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

/* {{{ run_inline_directive_filters */
int run_inline_directive_filters(const u_char *directive,const u_char **parameters,t_string *content,t_string *cite,const u_char *qchars,int sig) {
  t_directive_callback *cb;

  if((cb = cf_hash_get(registered_directives,(u_char *)directive,strlen(directive))) == NULL) return FLT_DECLINE;

  if(cb->type & CF_HTML_DIR_TYPE_INLINE) return cb->callback(&fo_default_conf,&fo_view_conf,directive,parameters,1,NULL,NULL,content,cite,qchars,sig);
  return FLT_DECLINE;
}
/* }}} */



/* {{{ internal functions */

int next_line_is_no_quote_line(const u_char *ptr) {
  int eq;

  for(;*ptr && ((eq = cf_strncmp(ptr,"<br />",6)) == 0 || *ptr == ' ');ptr++) {
    if(eq == 0) ptr += 5;
  }

  if(*ptr == (u_char)127) return 0;
  return 1;
}

int is_open(const u_char *name,t_array *stack) {
  int i;
  t_html_stack_elem *s_el;

  for(i=stack->elements-1;i>=0;--i) {
    s_el = array_element_at(stack,i);
    if(cf_strcmp(s_el->name,name) == 0) return 1;
  }

  return 0;
}

int run_block_directive_filters(const u_char *directive,const u_char **parameters,size_t len,t_string *bcontent,t_string *bcite,t_string *content,t_string *cite,const u_char *qchars,int sig) {
  t_directive_callback *cb;

  if((cb = cf_hash_get(registered_directives,(u_char *)directive,strlen(directive))) == NULL) return FLT_DECLINE;

  if(cb->type & CF_HTML_DIR_TYPE_BLOCK) return cb->callback(&fo_default_conf,&fo_view_conf,directive,parameters,len,bcontent,bcite,content,cite,qchars,sig);
  return FLT_DECLINE;  
}

u_char *parse_message(u_char *start,t_array *stack,t_string *content,t_string *cite,const u_char *qchars,size_t qclen,int utf8,int xml,int max_sig_lines,int show_sig,int linebrk,int sig,int quotemode,int line) {
  const u_char *ptr,*tmp,*ptr1;
  size_t i;
  int rc,run = 1,sb = 0,fail,ending;
  u_char *directive,*parameter,*safe,*buff,*retval;
  t_string d_content,d_cite;
  t_html_stack_elem stack_elem,*stack_tmp;

  for(ptr=start;*ptr && run;++ptr) {
    switch(*ptr) {
      case '[':
        safe = (u_char *)ptr;
        ending = 0;

        /* [/name] ends a directive */
        if(*(ptr+1) == '/') {
          ending = 1;
          ++ptr;
        }

        /* ok, parse this directive */
        for(ptr1=ptr+1,sb=0;*ptr1 && isalnum(*ptr1) && sb == 0 && *ptr1 != '<';++ptr1) {
          sb = *ptr1 == '[';
        }

        if(sb) {
          ptr = safe;
          goto default_action;
        }

        /* {{{ end of a directive */
        if(ending) {
          directive = strndup(ptr+1,ptr1-ptr-1);

          if(is_open(directive,stack)) {
            stack_tmp = array_element_at(stack,stack->elements-1);

            /* nesting is ok */
            if(cf_strcmp(stack_tmp->name,directive) == 0) {
              free(directive);
              return (u_char *)ptr1;
            }
            /* nesting error */
            else {
              free(directive);
              return NULL;
            }
          }
          /* not open, ignore it, user error */
          else {
            free(directive);
            ptr = safe;
            goto default_action;
          }
        }
        /* }}} */

        /* {{{ directive with argument, CForum syntax [name:argument], no ending tag */
        if(*ptr1 == ':') {
          tmp = ptr1;

          for(++ptr1;*ptr1 && *ptr1 != ']' && *ptr1 != '<';++ptr1);

          if(*ptr1 == ']') {
            directive = strndup(ptr+1,tmp-ptr-1);
            buff      = strndup(tmp+1,ptr1-tmp-1);
            parameter = htmlentities_decode(buff);
            free(buff);

            rc = run_inline_directive_filters(directive,(const u_char **)&parameter,content,cite,qchars,sig);

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
        /* }}} */

        /* {{{ we got [blub=blub] */
        else if(*ptr1 == '=') {
          tmp = ptr1;

          for(++ptr1;*ptr1 && *ptr1 != ']' && *ptr1 != '<';++ptr1);

          if(*ptr1 == ']') {
            directive = strndup(ptr+1,tmp-ptr-1);
            parameter = strndup(tmp+1,ptr-tmp-1);

            stack_elem.begin   = (u_char *)ptr1;
            stack_elem.name    = directive;
            stack_elem.args    = fo_alloc(NULL,1,sizeof(u_char **),FO_ALLOC_MALLOC);
            stack_elem.args[0] = parameter;
            stack_elem.argnum  = 1;

            array_push(stack,&stack_elem);

            str_init(&d_content);
            str_init(&d_cite);

            retval = parse_message((u_char *)ptr1+1,stack,&d_content,cite ? &d_cite : NULL,qchars,qclen,utf8,xml,max_sig_lines,show_sig,linebrk,sig,quotemode,line);

            array_pop(stack);

            if(retval == NULL) {
              /* directive is invalid, get defined state */
              free(directive);
              free(parameter);
              str_cleanup(&d_content);
              str_cleanup(&d_cite);
              free(stack_elem.args);
              ptr = safe;
              goto default_action;
            }

            /* ok, go and run directive filters */
            rc = run_block_directive_filters(directive,(const u_char **)&parameter,1,content,cite,&d_content,cite ? &d_cite : NULL,qchars,sig);

            if(rc == FLT_DECLINE) {
              str_char_append(content,'[');
              str_chars_append(content,directive,strlen(directive));
              str_char_append(content,'=');
              str_chars_append(content,parameter,strlen(parameter));
              str_char_append(content,']');

              str_str_append(content,&d_content);
              str_chars_append(content,"[/",2);
              str_chars_append(content,directive,strlen(directive));
              str_char_append(content,']');

              ptr = retval;
              continue;
            }
            else ptr = retval;

            str_str_append(content,&d_content);
            if(cite) str_str_append(cite,&d_cite);

            ptr = retval;
          }
          else {
            ptr = safe;
            goto default_action;
          }
        }
        /* }}} */

        /* {{{ we got [something something */
        else if(isspace(*ptr1) || *ptr1 == ']') {
          directive = strndup(ptr+1,ptr1-ptr-1);
          memset(&stack_elem,0,sizeof(stack_elem));

          sb = 0;
          fail = 0;

          /* ok, we can have multiple arguments in the form of arg=value */
          while(!sb && *ptr1 != ']') {
            /* eat up trailing whitespaces */
            for(++ptr1;isspace(*ptr1) || *ptr1 == '=';++ptr1);

            /* whitespaces are not allowed */
            for(tmp = ptr1;*ptr1 != '=' && *ptr1 != ']' && *ptr1 != '<' && !isspace(*ptr1);++ptr1);

            if(*ptr1 == '<') {
              free(directive);
              if(stack_elem.args) free(stack_elem.args);
              ptr = safe;
              goto default_action;
            }

            stack_elem.args = fo_alloc(stack_elem.args,++stack_elem.argnum,sizeof(*stack_elem.args),FO_ALLOC_REALLOC);
            stack_elem.args[stack_elem.argnum-1] = strndup(tmp,ptr1-tmp);

            sb = *ptr1 == ']';
          }

          if(stack_elem.argnum % 2 != 0) fail = 1;

          if(!fail) {
            stack_elem.name = directive;
            stack_elem.begin = (u_char *)ptr1;
            array_push(stack,&stack_elem);

            str_init(&d_content);
            str_init(&d_cite);

            retval = parse_message((u_char *)ptr1+1,stack,&d_content,cite ? &d_cite : NULL,qchars,qclen,utf8,xml,max_sig_lines,show_sig,linebrk,sig,quotemode,line);
            array_pop(stack);

            if(retval == NULL) {
              /* directive is invalid, get defined state */
              free(directive);
              str_cleanup(&d_content);
              str_cleanup(&d_cite);
              free(stack_elem.args);
              ptr = safe;
              goto default_action;
            }

            /* ok, go and run directive filters */
            rc = run_block_directive_filters(directive,(const u_char **)stack_elem.args,stack_elem.argnum,content,cite,&d_content,cite ? &d_cite : NULL,qchars,sig);

            if(rc == FLT_DECLINE) {
              str_char_append(content,'[');
              str_chars_append(content,directive,strlen(directive));

              for(i=0;i<stack_elem.argnum;i+=2) {
                str_char_append(content,' ');
                str_chars_append(content,stack_elem.args[i],strlen(stack_elem.args[i]));
                str_char_append(content,'=');
                str_chars_append(content,stack_elem.args[i+1],strlen(stack_elem.args[i+1]));

                free(stack_elem.args[i]);
                free(stack_elem.args[i+1]);
              }

              free(stack_elem.args);

              str_char_append(content,']');

              str_str_append(content,&d_content);
              str_chars_append(content,"[/",2);
              str_chars_append(content,directive,strlen(directive));
              str_char_append(content,']');

              ptr = retval;
              continue;
            }
            else ptr = retval;

            //str_str_append(content,&d_content);
            //if(cite) str_str_append(cite,&d_cite);
          }
          else {
            ptr = safe;
            goto default_action;
          }
        }
        /* }}} */

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

  return NULL;
}
/* }}} */



/* {{{ cf_html_register_directive */
int cf_html_register_directive(u_char *name,t_directive_filter filter,int type) {
  t_directive_callback clbck;
  size_t len = strlen(name);

  if(!registered_directives) registered_directives = cf_hash_new(NULL);
  if(cf_hash_get(registered_directives,name,len) != NULL) return -1;

  clbck.type = type;
  clbck.callback = filter;
  cf_hash_set(registered_directives,name,len,&clbck,sizeof(clbck));

  return 0;
}
/* }}} */

/* {{{ msg_to_html */
void msg_to_html(t_cl_thread *thread,const u_char *msg,t_string *content,t_string *cite,u_char *quote_chars,int max_sig_lines,int show_sig) {
  u_char *forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  t_name_value *cs   = cfg_get_first_value(&fo_default_conf,forum_name,"ExternCharset");
  t_name_value *xmlm = cfg_get_first_value(&fo_default_conf,forum_name,"XHTMLMode");
  u_char *qchars;
  size_t qclen;
  int utf8 = cf_strcmp(cs->values[0],"UTF-8") == 0,xml;
  t_array my_stack;

  if(registered_directives == NULL) registered_directives = cf_hash_new(NULL);

  xml = cf_strcmp(xmlm->values[0],"yes") == 0;

  if(utf8 || (qchars = htmlentities_charset_convert(quote_chars,"UTF-8",cs->values[0],&qclen,0)) == NULL) {
    qchars = htmlentities(quote_chars,0);
    qclen  = strlen(qchars);
  }

  run_content_filters(PRE_CONTENT_FILTER,thread,content,cite,qchars);

  /* first line has no linebreak, so append quoting chars to cite */
  if(cite) str_chars_append(cite,qchars,qclen);

  array_init(&my_stack,sizeof(t_html_stack_elem),NULL);
  parse_message((u_char *)msg,&my_stack,content,cite,qchars,qclen,utf8,xml,max_sig_lines,show_sig,0,0,0,0);

  run_content_filters(POST_CONTENT_FILTER,thread,content,cite,qchars);

  free(qchars);
}
/* }}} */



/* eof */
