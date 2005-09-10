/**
 * \file userconf.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 *
 * User config utilities
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
#include "config.h"
#include "defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>

#include <gdome.h>

#include <pcre.h>

#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "template.h"
#include "readline.h"
#include "clientlib.h"
#include "cfcgi.h"
#include "validate.h"

#include "userconf.h"
#include "xml_handling.h"
/* }}} */

/* {{{ cf_uconf_destroy_argument */
void cf_uconf_destroy_argument(uconf_argument_t *argument) {
  if(argument->param) free(argument->param);
  if(argument->ifnotcommitted) free(argument->ifnotcommitted);
  if(argument->deflt) free(argument->deflt);
  if(argument->validation) free(argument->validation);
  if(argument->error) free(argument->error);
  if(argument->val) free(argument->val);
}
/* }}} */

/* {{{ cf_uconf_destroy_directive */
void cf_uconf_destroy_directive(uconf_directive_t *dir) {
  if(dir->name) free(dir->name);
  if(dir->access) free(dir->access);
  if(dir->arguments.elements) array_destroy(&dir->arguments);
}
/* }}} */

/* {{{ cf_uconf_get_arg */
static int cf_uconf_get_arg(uconf_directive_t *dir,GdomeNode *arg) {
  uconf_argument_t argument;
  GdomeNodeList *nl;
  GdomeNode *n;
  GdomeException e;
  GdomeDOMString *name;
  gulong i,len;

  memset(&argument,0,sizeof(argument));

  if((argument.param      = xml_get_attribute(arg,"paramname")) == NULL) return -1;
  argument.ifnotcommitted = xml_get_attribute(arg,"ifNotCommitted");
  argument.deflt          = xml_get_attribute(arg,"default");
  argument.parse          = xml_get_attribute(arg,"parse");

  if((nl = gdome_n_childNodes(arg,&e)) == NULL) {
    cf_uconf_destroy_argument(&argument);
    return -1;
  }

  len = gdome_nl_length(nl,&e);
  for(i=0;i<len;++i) {
    n = gdome_nl_item(nl,i,&e);
    if((name = gdome_n_nodeName(n,&e)) == NULL) {
      gdome_n_unref(n,&e);
      cf_uconf_destroy_argument(&argument);
      return -1;
    }

    if(cf_strcmp(name->str,"validate") == 0) {
      if((argument.validation = xml_get_node_value(n)) == NULL) {
        argument.validation_type = 1;
        argument.validation      = xml_get_attribute(n,"type");
      }
      else argument.validation_type = 0;
    }
    else if(cf_strcmp(name->str,"error") == 0) argument.error = xml_get_node_value(n);

    gdome_str_unref(name);
    gdome_n_unref(n,&e);
  }

  gdome_nl_unref(nl,&e);

  array_push(&dir->arguments,&argument);

  return 0;
}
/* }}} */

/* {{{ cf_uconf_get_directive */
int cf_uconf_get_directive(uconf_userconfig_t *domxml,GdomeNode *directive_node) {
  uconf_directive_t directive;
  u_char *val;
  GdomeNodeList *nl;
  GdomeDOMString *str;
  GdomeException e;
  GdomeNode *n;

  gulong i,len;

  memset(&directive,0,sizeof(directive));
  array_init(&directive.arguments,sizeof(uconf_argument_t),(void (*)(void *))cf_uconf_destroy_argument);

  /* {{{ get directive attributes */
  if((directive.name   = xml_get_attribute(directive_node,"name")) == NULL) return -1;
  directive.access     = xml_get_attribute(directive_node,"for");

  if((val = xml_get_attribute(directive_node,"invisible")) != NULL) {
    if(cf_strcmp(val,"yes") == 0) directive.flags |= CF_UCONF_FLAG_INVISIBLE;
    free(val);
  }

  if((val = xml_get_attribute(directive_node,"arguments")) != NULL) {
    directive.argnum = atoi(val);
    free(val);
  }
  /* }}} */

  str = gdome_str_mkref("argument");
  if((nl = gdome_el_getElementsByTagName((GdomeElement *)directive_node,str,&e)) == NULL) {
    gdome_str_unref(str);
    cf_uconf_destroy_directive(&directive);
    return -1;
  }

  len = gdome_nl_length(nl,&e);
  for(i=0;i<len;++i) {
    n = gdome_nl_item(nl,i,&e);

    if(cf_uconf_get_arg(&directive,n) != 0) {
      cf_uconf_destroy_directive(&directive);
      gdome_n_unref(n,&e);
      return -1;
    }

    gdome_n_unref(n,&e);
  }

  gdome_nl_unref(nl,&e);

  array_push(&domxml->directives,&directive);
  return 0;
}
/* }}} */

/* {{{ cf_uconf_read_modxml */
uconf_userconfig_t *cf_uconf_read_modxml() {
  GdomeException e;
  GdomeDOMImplementation *di;
  GdomeDocument *doc;

  GdomeDOMString *directive_str;
  GdomeNodeList *nl;
  GdomeNode *n;

  uconf_userconfig_t *domxml;
  uconf_directive_t directive;

  gulong i,len;

  u_char *fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  name_value_t *path = cfg_get_first_value(&fo_userconf_conf,fn,"ModuleConfig");

  di = gdome_di_mkref();

  if((doc = gdome_di_createDocFromURI(di,path->values[0],GDOME_LOAD_VALIDATING,&e)) == NULL) {
    gdome_di_unref(di,&e);
    return NULL;
  }

  directive_str = gdome_str_mkref("directive");
  if((nl = gdome_doc_getElementsByTagName(doc,directive_str,&e)) == NULL) {
    gdome_str_unref(directive_str);
    gdome_doc_unref(doc,&e);
    gdome_di_unref(di,&e);
    return NULL;
  }

  domxml = fo_alloc(NULL,1,sizeof(*domxml),FO_ALLOC_CALLOC);
  array_init(&domxml->directives,sizeof(directive),(void (*)(void *))cf_uconf_destroy_directive);

  len = gdome_nl_length(nl,&e);
  for(i=0;i<len;++i) {
    n = gdome_nl_item(nl,i,&e);

    /* {{{ handle directive, on error return NULL */
    if(cf_uconf_get_directive(domxml,n) == -1) {
      cf_uconf_cleanup_modxml(domxml);
      gdome_n_unref(n,&e);
      gdome_nl_unref(nl,&e);
      gdome_str_unref(directive_str);
      gdome_doc_unref(doc,&e);
      gdome_di_unref(di,&e);
      return NULL;
    }
    /* }}} */

    gdome_n_unref(n,&e);
  }

  gdome_nl_unref(nl,&e);
  gdome_str_unref(directive_str);
  gdome_doc_unref(doc,&e);
  gdome_di_unref(di,&e);

  return domxml;
}
/* }}} */

/* {{{ cf_uconf_cleanup_modxml */
void cf_uconf_cleanup_modxml(uconf_userconfig_t *modxml) {
  array_destroy(&modxml->directives);
  free(modxml);
}
/* }}} */

/* {{{ cf_uconf_to_html */
void cf_uconf_to_html(string_t *str) {
  string_t local;
  register u_char *ptr;

  str_init(&local);
  for(ptr=str->content;*ptr;++ptr) {
    switch(*ptr) {
      case '\\':
        switch(*(ptr+1)) {
          case '"':
            str_char_append(&local,'"');
            ++ptr;
            break;
          case 'n':
            str_chars_append(&local,"<br />",6);
            ++ptr;
            break;
          default:
            str_char_append(&local,*ptr);
            break;
        }
        break;

      default:
        str_char_append(&local,*ptr);
    }
  }

  str_cleanup(str);
  str->content  = local.content;
  str->len      = local.len;
  str->reserved = local.reserved;
}
/* }}} */

void cf_uconf_copy_values(configuration_t *config,uconf_directive_t *directive,uconf_directive_t *my_directive,int do_if_empty) {
  uconf_argument_t *arg,my_arg;
  name_value_t *val;
  size_t i;

  memset(my_directive,0,sizeof(*my_directive));
  my_directive->name   = strdup(directive->name);
  if(my_directive->access) my_directive->access = strdup(directive->access);
  my_directive->flags  = directive->flags;
  my_directive->argnum = directive->argnum;

  array_init(&my_directive->arguments,sizeof(my_arg),(void (*)(void *))cf_uconf_destroy_argument);
  memset(&my_arg,0,sizeof(my_arg));

  if((val = cfg_get_first_value(config,NULL,directive->name)) == NULL) {
    if(do_if_empty) {
      for(i=0;i<directive->arguments.elements;++i) {
        arg = array_element_at(&directive->arguments,i);
        if(arg->val) my_arg.val = strdup(arg->val);

        array_push(&my_directive->arguments,&my_arg);
      }
    }
  }
  else {
    for(i=0;i<val->valnum;++i) {
      my_arg.val = strdup(val->values[i]);
      array_push(&my_directive->arguments,&my_arg);
    }
  }
}

uconf_userconfig_t *cf_uconf_merge_config(cf_hash_t *head,configuration_t *config,array_t *errormessages,int touch_committed) {
  uconf_userconfig_t *modxml = cf_uconf_read_modxml(),*merged;
  uconf_directive_t *directive,my_directive;
  uconf_argument_t *arg,my_arg;
  cf_cgi_param_t *mult;
  size_t i,j;
  string_t str,str1;
  name_value_t *val;

  pcre *regexp;
  u_char *error;
  int erroffset;

  int err_occured = 0;

  if(!modxml) return NULL;

  merged = fo_alloc(NULL,1,sizeof(*merged),FO_ALLOC_MALLOC);
  array_init(&merged->directives,sizeof(my_directive),(void (*)(void *))cf_uconf_destroy_directive);
  array_init(errormessages,sizeof(str1),(void (*)(void *))str_cleanup);

  for(i=0;i<modxml->directives.elements;++i) {
    directive = array_element_at(&modxml->directives,i);

    memset(&my_directive,0,sizeof(my_directive));
    my_directive.name   = strdup(directive->name);
    if(my_directive.access) my_directive.access = strdup(directive->access);
    my_directive.flags  = directive->flags;
    my_directive.argnum = directive->argnum;

    array_init(&my_directive.arguments,sizeof(my_arg),(void (*)(void *))cf_uconf_destroy_argument);

    if(directive->flags & CF_UCONF_FLAG_INVISIBLE) {
      cf_uconf_copy_values(config,directive,&my_directive,0);
      array_push(&merged->directives,&my_directive);
    }
    else {
      for(j=0;j<directive->arguments.elements;++j) {
        arg = array_element_at(&directive->arguments,j);

        if((mult = cf_cgi_get_multiple(head,arg->param)) != NULL) {
          str_init_growth(&str,128);
          str_cstr_set(&str,mult->value);

          for(mult=mult->next;mult;mult=mult->next) {
            str_char_append(&str,',');
            str_cstr_append(&str,mult->value);
          }

          if(arg->validation_type) {
            if(cf_strcmp(arg->validation,"email") == 0) {
              if(is_valid_mailaddress(str.content) == -1) {
                str_init_growth(&str1,128);
                str_cstr_set(&str1,arg->error);
                err_occured = 1;
                array_push(errormessages,&str1);
              }
            }
            else if(cf_strcmp(arg->validation,"http-url") == 0) {
              if(is_valid_http_link(str.content,0) == -1) {
                str_init_growth(&str1,128);
                str_cstr_set(&str1,arg->error);
                err_occured = 1;
                array_push(errormessages,&str1);
              }
            }
            else if(cf_strcmp(arg->validation,"http-url-strict") == 0) {
              if(is_valid_http_link(str.content,1) == -1) {
                str_init_growth(&str1,128);
                str_cstr_set(&str1,arg->error);
                err_occured = 1;
                array_push(errormessages,&str1);
              }
            }
            else if(cf_strcmp(arg->validation,"url") == 0) {
              if(is_valid_link(str.content) == -1) {
                str_init_growth(&str1,128);
                str_cstr_set(&str1,arg->error);
                err_occured = 1;
                array_push(errormessages,&str1);
              }
            }
          }
          else {
            if((regexp = pcre_compile(arg->validation,0,(const char **)&error,&erroffset,NULL)) == NULL) {
              fprintf(stderr,"Error in pattern '%s': %s\n",str.content,error);
              str_init_growth(&str1,128);
              str_cstr_set(&str1,arg->error);
              err_occured = 1;
              array_push(errormessages,&str1);
            }

            if(regexp && pcre_exec(regexp,NULL,str.content,str.len,0,0,NULL,0) < 0) {
              str_init_growth(&str1,128);
              str_cstr_set(&str1,arg->error);
              err_occured = 1;
              array_push(errormessages,&str1);
            }

            pcre_free(regexp);
          }

          if(err_occured == 0) {
            if(arg->parse && cf_strcmp(arg->parse,"date") == 0) {
            }
            else {
            }
          }
        }
        else {
          memset(&my_arg,0,sizeof(my_arg));

          if(touch_committed) {
            if(arg->ifnotcommitted) my_arg.val = arg->ifnotcommitted;
          }
          else {
            if((val = cfg_get_first_value(config,NULL,directive->name)) != NULL) my_arg.val = strdup(val->values[j]);
          }
        }

        if(err_occured == 0) array_push(&my_directive.arguments,&my_arg);
      }
    }
  }


  cf_uconf_cleanup_modxml(modxml);
  free(modxml);

  return NULL;
}

u_char *cf_write_uconf(uconf_userconfig_t *merged) {
  return NULL;
}

/* eof */
