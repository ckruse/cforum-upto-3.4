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

typedef struct s_validator_callback {
  t_directive_validator callback;
  int type;
} t_validator_callback;

typedef struct s_html_stack {
  u_char *begin;
  u_char *name;
  u_char **args;
  size_t argnum;
} t_html_stack_elem;

static t_cf_hash *registered_directives = NULL;
static t_cf_hash *registered_validators = NULL;


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
int run_inline_directive_filters(t_cl_thread *thread,const u_char *directive,const u_char **parameters,t_string *content,t_string *cite,const u_char *qchars,int sig) {
  t_directive_callback *cb;

  if((cb = cf_hash_get(registered_directives,(u_char *)directive,strlen(directive))) == NULL) return FLT_DECLINE;

  if(cb->type & CF_HTML_DIR_TYPE_INLINE) return cb->callback(&fo_default_conf,&fo_view_conf,thread,directive,parameters,1,NULL,NULL,content,cite,qchars,sig);
  return FLT_DECLINE;
}
/* }}} */

/* {{{ run_validate_inline */
int run_validate_inline(const u_char *directive,const u_char **parameters,t_cf_tpl_variable *var) {
  t_validator_callback *cb;

  if((cb = cf_hash_get(registered_validators,(u_char *)directive,strlen(directive))) == NULL) return FLT_DECLINE;

  if(cb->type & CF_HTML_DIR_TYPE_INLINE) return cb->callback(&fo_default_conf,&fo_view_conf,directive,parameters,1,var);
  return FLT_DECLINE;
}
/* }}} */

/* {{{ next_line_is_no_quote_line */
static int next_line_is_no_quote_line(const u_char *ptr) {
  int eq;

  for(;*ptr && ((eq = cf_strncmp(ptr,"<br />",6)) == 0 || *ptr == ' ');ptr++) {
    if(eq == 0) ptr += 5;
  }

  if(*ptr == (u_char)127) return 0;
  return 1;
}
/* }}} */

/* {{{ is_open */
static int is_open(const u_char *name,t_array *stack) {
  int i;
  t_html_stack_elem *s_el;

  for(i=stack->elements-1;i>=0;--i) {
    s_el = array_element_at(stack,i);
    if(cf_strcmp(s_el->name,name) == 0) return 1;
  }

  return 0;
}
/* }}} */

/* {{{ run_block_directive_filters */
static int run_block_directive_filters(t_cl_thread *thread,const u_char *directive,const u_char **parameters,size_t len,t_string *bcontent,t_string *bcite,t_string *content,t_string *cite,const u_char *qchars,int sig) {
  t_directive_callback *cb;

  if((cb = cf_hash_get(registered_directives,(u_char *)directive,strlen(directive))) == NULL) return FLT_DECLINE;

  if(cb->type & CF_HTML_DIR_TYPE_BLOCK) return cb->callback(&fo_default_conf,&fo_view_conf,thread,directive,parameters,len,bcontent,bcite,content,cite,qchars,sig);
  return FLT_DECLINE;
}
/* }}} */

/* {{{ run_validate_block_directive */
static int run_validate_block_directive(const u_char *directive,const u_char **parameters,size_t len,t_cf_tpl_variable *var) {
  t_validator_callback *cb;

  if((cb = cf_hash_get(registered_validators,(u_char *)directive,strlen(directive))) == NULL) return FLT_DECLINE;

  if(cb->type & CF_HTML_DIR_TYPE_BLOCK) return cb->callback(&fo_default_conf,&fo_view_conf,directive,parameters,len,var);
  return FLT_DECLINE;
}
/* }}} */

/* {{{ parse_message */
static u_char *parse_message(t_cl_thread *thread,u_char *start,t_array *stack,t_string *content,t_string *cite,const u_char *qchars,size_t qclen,int utf8,int xml,int max_sig_lines,int show_sig,int linebrk,int sig,int quotemode,int line) {
  const u_char *ptr,*tmp,*ptr1;
  int rc,run = 1,sb = 0,fail,ending;
  u_char *directive,*parameter,*safe,*buff,*retval;
  t_string d_content,d_cite,strtmp;
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
        for(ptr1=ptr+1,sb=0;*ptr1 && (isalpha(*ptr1) || (ptr1 != ptr + 1 && isalnum(*ptr1))) && sb == 0 && *ptr1 != '<';++ptr1) {
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
          str_init(&strtmp);

          /* get directive end, but accept \] as not-end */
          for(++ptr1;*ptr1 && *ptr1 != ']' && *ptr1 != '<';++ptr1) {
            if(*ptr1 == '\\' && *(ptr1+1) == ']') {
              str_char_append(&strtmp,']');
              ++ptr1;
            }
            else str_char_append(&strtmp,*ptr1);
          }

          if(*ptr1 == ']' && strtmp.len) {
            directive = strndup(ptr+1,tmp-ptr-1);
            buff      = strtmp.content;
            parameter = htmlentities_decode(buff,NULL);
            free(buff);

            rc = run_inline_directive_filters(thread,directive,(const u_char **)&parameter,content,cite,qchars,sig);

            free(directive);
            free(parameter);

            if(rc == FLT_DECLINE) {
              ptr = safe;
              goto default_action;
            }
            else ptr = ptr1;
          }
          else {
            str_cleanup(&strtmp);
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

            if(cf_hash_get(registered_directives,directive,tmp-ptr-1) == NULL) {
              free(directive);
              goto default_action;
            }

            parameter = strndup(tmp+1,ptr-tmp-1);

            stack_elem.begin   = (u_char *)ptr1;
            stack_elem.name    = directive;
            stack_elem.args    = fo_alloc(NULL,1,sizeof(u_char **),FO_ALLOC_MALLOC);
            stack_elem.args[0] = parameter;
            stack_elem.argnum  = 1;

            array_push(stack,&stack_elem);

            str_init(&d_content);
            str_init(&d_cite);

            retval = parse_message(thread,(u_char *)ptr1+1,stack,&d_content,cite ? &d_cite : NULL,qchars,qclen,utf8,xml,max_sig_lines,show_sig,linebrk,sig,quotemode,line);

            array_pop(stack);

            if(retval == NULL || d_content.len == 0) {
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
            rc = run_block_directive_filters(thread,directive,(const u_char **)&parameter,1,content,cite,&d_content,cite ? &d_cite : NULL,qchars,sig);

            str_cleanup(&d_content);
            str_cleanup(&d_cite);

            if(rc == FLT_DECLINE) {
              ptr = safe;
              goto default_action;
            }

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

          if(cf_hash_get(registered_directives,directive,ptr1-ptr-1) == NULL) {
            free(directive);
            goto default_action;
          }

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

            retval = parse_message(thread,(u_char *)ptr1+1,stack,&d_content,cite ? &d_cite : NULL,qchars,qclen,utf8,xml,max_sig_lines,show_sig,linebrk,sig,quotemode,line);
            array_pop(stack);

            /* directive is invalid (e.g. no content, wrong nesting), get defined state */
            if(retval == NULL || d_content.len == 0) {
              free(directive);
              str_cleanup(&d_content);
              str_cleanup(&d_cite);
              free(stack_elem.args);
              ptr = safe;
              goto default_action;
            }

            /* ok, go and run directive filters */
            rc = run_block_directive_filters(thread,directive,(const u_char **)stack_elem.args,stack_elem.argnum,content,cite,&d_content,cite ? &d_cite : NULL,qchars,sig);

            str_cleanup(&d_content);
            str_cleanup(&d_cite);

            if(rc == FLT_DECLINE) {
              ptr = safe;
              goto default_action;
            }

            ptr = retval;
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

/* {{{ validate_message */
int validate_message(t_array *stack,t_cl_thread *thread,const u_char *msg,u_char **pos,t_cf_tpl_variable *var) {
  const u_char *ptr,*tmp,*ptr1;
  int rc,run = 1,sb = 0,fail,ending,retval,ret = 1;
  u_char *directive,*parameter,*safe,*buff;
  t_string strtmp;
  t_html_stack_elem stack_elem,*stack_tmp;

  for(ptr=(u_char *)msg;*ptr && run;++ptr) {
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
        for(ptr1=ptr+1,sb=0;*ptr1 && isalpha(*ptr1) && sb == 0 && *ptr1 != '<';++ptr1) {
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
              if(pos) *pos = (u_char *)ptr1;
              return FLT_OK;
            }
            /* nesting error */
            else {
              free(directive);
              return FLT_DECLINE;
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
          str_init(&strtmp);

          /* get directive end, but accept \] as not-end */
          for(++ptr1;*ptr1 && *ptr1 != ']' && *ptr1 != '<';++ptr1) {
            if(*ptr1 == '\\' && *(ptr1+1) == ']') {
              str_char_append(&strtmp,']');
              ++ptr1;
            }
            else str_char_append(&strtmp,*ptr1);
          }

          if(*ptr1 == ']') {
            directive = strndup(ptr+1,tmp-ptr-1);
            buff      = strtmp.content;
            parameter = htmlentities_decode(buff,NULL);
            free(buff);

            rc = run_validate_inline(directive,(const u_char **)&parameter,var);

            free(directive);
            free(parameter);

            if(rc == FLT_DECLINE) {
              ptr = safe;
              goto default_action;
            }
            else ptr = ptr1;

            if(rc == FLT_ERROR) ret = FLT_ERROR;
          }
          else {
            str_cleanup(&strtmp);
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

            if(cf_hash_get(registered_validators,directive,tmp-ptr-1) == NULL) {
              free(directive);
              goto default_action;
            }

            parameter = strndup(tmp+1,ptr-tmp-1);

            stack_elem.begin   = (u_char *)ptr1;
            stack_elem.name    = directive;
            stack_elem.args    = fo_alloc(NULL,1,sizeof(u_char **),FO_ALLOC_MALLOC);
            stack_elem.args[0] = parameter;
            stack_elem.argnum  = 1;

            array_push(stack,&stack_elem);

            retval = validate_message(stack,thread,ptr1+1,(u_char **)&ptr,var);

            array_pop(stack);

            if(retval == FLT_ERROR) {
              /* directive is invalid, get defined state */
              free(directive);
              free(parameter);
              free(stack_elem.args);

              ret = FLT_ERROR;
              goto default_action;
            }
            else if(retval == FLT_DECLINE || ret == 1) {
              /* directive is invalid, get defined state */
              free(directive);
              free(parameter);
              free(stack_elem.args);

              goto default_action;
            }

            /* ok, go and run directive filters */
            rc = run_validate_block_directive(directive,(const u_char **)&parameter,1,var);

            if(rc == FLT_ERROR) ret = FLT_ERROR;
            else if(rc == FLT_DECLINE) {
              ptr = safe;
              goto default_action;
            }
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

          if(cf_hash_get(registered_validators,directive,ptr1-ptr-1) == NULL) {
            free(directive);
            goto default_action;
          }

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

            retval = validate_message(stack,thread,ptr1+1,(u_char **)&ptr,var);

            array_pop(stack);

            if(retval == FLT_ERROR) {
              /* directive is invalid, get defined state */
              free(directive);
              free(stack_elem.args);
              ptr = safe;
              ret = FLT_ERROR;

              goto default_action;
            }
            else if(retval == FLT_DECLINE || retval == 1) {
              free(directive);
              free(stack_elem.args);
              ptr = safe;

              goto default_action;
            }

            /* ok, go and run directive filters */
            rc = run_validate_block_directive(directive,(const u_char **)stack_elem.args,stack_elem.argnum,var);

            if(rc == FLT_ERROR) ret = FLT_ERROR;
            else if(rc == FLT_DECLINE) {
              ptr = safe;
              goto default_action;
            }
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

      default:
        default_action:
        break;
    }
  }

  return ret;
}

/* }}} */


/* {{{ cf_html_register_validator */
int cf_html_register_validator(u_char *name,t_directive_validator filter,int type) {
  t_validator_callback clbck;
  size_t len = strlen(name);

  if(!registered_validators) registered_validators = cf_hash_new(NULL);
  if(cf_hash_get(registered_validators,name,len) != NULL) return -1;

  clbck.type = type;
  clbck.callback = filter;
  cf_hash_set(registered_validators,name,len,&clbck,sizeof(clbck));

  return 0;
}
/* }}} */

/* {{{ cf_validate_msg */
int cf_validate_msg(t_cl_thread *thread,const u_char *msg,t_cf_tpl_variable *var) {
  t_array my_stack;
  int rc;

  if(registered_validators == NULL) registered_validators = cf_hash_new(NULL);

  array_init(&my_stack,sizeof(t_html_stack_elem),NULL);
  rc = validate_message(&my_stack,thread,msg,NULL,var);

  array_destroy(&my_stack);

  return rc;
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
  parse_message(thread,(u_char *)msg,&my_stack,content,cite,qchars,qclen,utf8,xml,max_sig_lines,show_sig,0,0,0,0);

  run_content_filters(POST_CONTENT_FILTER,thread,content,cite,qchars);

  free(qchars);
}
/* }}} */

/* {{{ cf_gen_threadlist */
int cf_gen_threadlist(t_cl_thread *thread,t_cf_hash *head,t_string *threadlist,const u_char *type,const u_char *linktpl,int mode) {
  t_message *msg;
  int ShowInvisible = cf_hash_get(GlobalValues,"ShowInvisible",13) == NULL ? 0 : 1;

  u_char *forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10), *date, *link;

  int slvl = -1,level = 0,ret,printed = 0,first = 1;

  t_name_value *dft = cfg_get_first_value(&fo_view_conf,forum_name,"DateFormatThreadList"),
    *open_thread   = cfg_get_first_value(&fo_view_conf,forum_name,"OpenThread"),
    *open_post     = cfg_get_first_value(&fo_view_conf,forum_name,"OpenPosting"),
    *open_subtree  = cfg_get_first_value(&fo_view_conf,forum_name,"OpenSubtree"),
    *close_subtree = cfg_get_first_value(&fo_view_conf,forum_name,"CloseSubtree"),
    *close_post    = cfg_get_first_value(&fo_view_conf,forum_name,"ClosePosting"),
    *close_thread  = cfg_get_first_value(&fo_view_conf,forum_name,"CloseThread"),
    *locale = cfg_get_first_value(&fo_default_conf,forum_name,"DateLocale"),
    *cs = cfg_get_first_value(&fo_default_conf,forum_name,"ExternCharset");

  size_t len,
    open_thread_len   = strlen(open_thread->values[0]),
    open_post_len     = strlen(open_post->values[0]),
    open_subtree_len  = strlen(open_subtree->values[0]),
    close_subtree_len = strlen(close_subtree->values[0]),
    close_post_len    = strlen(close_post->values[0]),
    close_thread_len  = strlen(close_thread->values[0]);


  str_init(threadlist);

  if(cf_strcmp(type,"none") != 0) {
    /* {{{ hide thread path if in partitial mode */
    if(cf_strcmp(type,"partitial") == 0 && mode == CF_MODE_THREADVIEW) {
      for(msg=thread->messages;msg && msg->mid != thread->threadmsg->mid;msg=msg->next) msg->may_show = 0;

      level = msg->level;
      msg->may_show = 0;

      for(msg=msg->next;msg && msg->level > level;msg=msg->next);
      for(;msg;msg=msg->next) msg->may_show = 0;
    }
    /* }}} */
    /* {{{ set some standard variables */
    else if(mode == CF_MODE_THREADVIEW) cf_tpl_setvalue(&thread->threadmsg->tpl,"active",TPL_VARIABLE_INT,1);
    else if(mode == CF_MODE_THREADLIST) {
      cf_tpl_setvalue(&thread->messages->tpl,"start",TPL_VARIABLE_INT,1);
      cf_tpl_setvalue(&thread->messages->tpl,"msgnum",TPL_VARIABLE_INT,thread->msg_len);
      cf_tpl_setvalue(&thread->messages->tpl,"answers",TPL_VARIABLE_INT,thread->msg_len-1);
    }
    /* }}} */

    /* {{{ run handlers in pre and post mode */
    if(mode == CF_MODE_THREADLIST) {
      ret = cf_run_view_handlers(thread,head,CF_MODE_THREADLIST|CF_MODE_PRE);
      if(ret == FLT_OK || ret == FLT_DECLINE || ShowInvisible) {
        for(msg=thread->messages;msg;msg=msg->next) cf_run_view_list_handlers(msg,head,thread->tid,mode);
        ret = cf_run_view_handlers(thread,head,mode|CF_MODE_POST);

        if(ret == FLT_EXIT && !ShowInvisible) return FLT_EXIT;
      }

    }
    else {
      cf_run_view_handlers(thread,head,mode|CF_MODE_PRE);
      for(msg=thread->messages;msg;msg=msg->next) cf_run_view_list_handlers(msg,head,thread->tid,mode);
      cf_run_view_handlers(thread,head,mode|CF_MODE_POST);
    }
    /* }}} */

    level = 0;
    for(msg=thread->messages;msg;msg=msg->next) {
      if(ShowInvisible == 0 && (msg->may_show == 0 || msg->invisible == 1)) continue;
      if(slvl == -1) slvl = msg->level;
      printed = 1;

      date = cf_general_get_time(dft->values[0],locale->values[0],&len,&msg->date);
      link = cf_get_link(linktpl,thread->tid,msg->mid);

      cf_set_variable(&msg->tpl,cs,"author",msg->author.content,msg->author.len,1);
      cf_set_variable(&msg->tpl,cs,"title",msg->subject.content,msg->subject.len,1);

      if(msg->category.len) cf_set_variable(&msg->tpl,cs,"category",msg->category.content,msg->category.len,1);

      if(date) {
        cf_set_variable(&msg->tpl,cs,"time",date,len,1);
        free(date);
      }

      if(link) {
        cf_set_variable(&msg->tpl,cs,"link",link,strlen(link),1);
        free(link);
      }

      if(msg->level < level) {
        for(;level>msg->level;level--) {
          str_chars_append(threadlist,close_subtree->values[0],close_subtree_len);
          str_chars_append(threadlist,close_post->values[0],close_post_len);
        }
      }

      level = msg->level;

      if(msg->next && cf_msg_has_answers(msg)) { /* this message has at least one answer */
        if(msg == thread->messages || first) str_chars_append(threadlist,open_thread->values[0],open_thread_len);
        //else str_chars_append(threadlist,open_post->values[0],open_post_len);
        str_chars_append(threadlist,open_post->values[0],open_post_len);

        cf_tpl_parse_to_mem(&msg->tpl);
        str_str_append(threadlist,&msg->tpl.parsed);

        str_chars_append(threadlist,open_subtree->values[0],open_subtree_len);
        level++;
      }
      else {
        if(msg == thread->messages || first) str_chars_append(threadlist,open_thread->values[0],open_thread_len);
        //else str_chars_append(threadlist,open_post->values[0],open_post_len);
        str_chars_append(threadlist,open_post->values[0],open_post_len);

        cf_tpl_parse_to_mem(&msg->tpl);
        str_str_append(threadlist,&msg->tpl.parsed);

        str_chars_append(threadlist,close_post->values[0],close_post_len);
        //if(msg == thread->messages || first) str_chars_append(threadlist,ct->values[0],ct_l);
        //else str_chars_append(threadlist,cp->values[0],cp_l);
      }

      str_cleanup(&msg->tpl.parsed);
      first = 0;
    }

    if(printed) {
      for(;level>slvl;level--) {
        str_chars_append(threadlist,close_subtree->values[0],close_subtree_len);
        str_chars_append(threadlist,close_post->values[0],close_post_len);
      }

      //str_chars_append(threadlist,close_post->values[0],close_post_len);
      str_chars_append(threadlist,close_thread->values[0],close_thread_len);
    }
  }

  return FLT_OK;
}
/* }}} */

/* eof */
