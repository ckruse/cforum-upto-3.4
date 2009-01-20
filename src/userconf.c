/**
 * \file userconf.c
 * \author Christian Kruse, <cjk@wwwtech.de>
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
#include "cfconfig.h"
#include "defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>

#include <gdome.h>

#include <pcre.h>

#include <sys/types.h>

#include "hashlib.h"
#include "utils.h"
#include "cfgcomp.h"
#include "template.h"
#include "readline.h"
#include "clientlib.h"
#include "cfcgi.h"
#include "validate.h"

#include "userconf.h"
#include "xml_handling.h"
/* }}} */

/* {{{ cf_uconf_destroy_argument */
void cf_uconf_destroy_argument(cf_uconf_argument_t *argument) {
  if(argument->param) free(argument->param);
  if(argument->ifnotcommitted) free(argument->ifnotcommitted);
  if(argument->deflt) free(argument->deflt);
  if(argument->validation) free(argument->validation);
  if(argument->error) free(argument->error);
  if(argument->val) free(argument->val);
}
/* }}} */

/* {{{ cf_uconf_destroy_directive */
void cf_uconf_destroy_directive(cf_uconf_directive_t *dir) {
  if(dir->name) free(dir->name);
  if(dir->access) free(dir->access);
  if(dir->arguments.elements) cf_array_destroy(&dir->arguments);
}
/* }}} */

/* {{{ cf_uconf_get_arg */
static int cf_uconf_get_arg(cf_uconf_directive_t *dir,GdomeNode *arg) {
  cf_uconf_argument_t argument;
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

  cf_array_push(&dir->arguments,&argument);

  return 0;
}
/* }}} */

/* {{{ cf_uconf_get_directive */
int cf_uconf_get_directive(cf_uconf_userconfig_t *domxml,GdomeNode *directive_node) {
  cf_uconf_directive_t directive;
  u_char *val;
  GdomeNodeList *nl;
  GdomeDOMString *str;
  GdomeException e;
  GdomeNode *n;

  gulong i,len;

  memset(&directive,0,sizeof(directive));
  cf_array_init(&directive.arguments,sizeof(cf_uconf_argument_t),(void (*)(void *))cf_uconf_destroy_argument);

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

  cf_array_push(&domxml->directives,&directive);
  return 0;
}
/* }}} */

/* {{{ cf_uconf_read_modxml */
cf_uconf_userconfig_t *cf_uconf_read_modxml() {
  GdomeException e;
  GdomeDOMImplementation *di;
  GdomeDocument *doc;

  GdomeDOMString *directive_str;
  GdomeNodeList *nl;
  GdomeNode *n;

  cf_uconf_userconfig_t *domxml;
  cf_uconf_directive_t directive;

  gulong i,len;

  u_char *fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  cf_name_value_t *path = cf_cfg_get_first_value(&fo_userconf_conf,fn,"ModuleConfig");

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

  domxml = cf_alloc(NULL,1,sizeof(*domxml),CF_ALLOC_MALLOC);
  cf_array_init(&domxml->directives,sizeof(directive),(void (*)(void *))cf_uconf_destroy_directive);

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
void cf_uconf_cleanup_modxml(cf_uconf_userconfig_t *modxml) {
  cf_array_destroy(&modxml->directives);
  free(modxml);
}
/* }}} */

/* {{{ cf_uconf_to_html */
void cf_uconf_to_html(cf_string_t *str) {
  cf_string_t local;
  register u_char *ptr;

  cf_str_init(&local);
  for(ptr=str->content;*ptr;++ptr) {
    switch(*ptr) {
      case '\\':
        switch(*(ptr+1)) {
          case '"':
            cf_str_char_append(&local,'"');
            ++ptr;
            break;
          case 'n':
            cf_str_chars_append(&local,"<br />",6);
            ++ptr;
            break;
          default:
            cf_str_char_append(&local,*ptr);
            break;
        }
        break;

      default:
        cf_str_char_append(&local,*ptr);
    }
  }

  cf_str_cleanup(str);
  str->content  = local.content;
  str->len      = local.len;
  str->reserved = local.reserved;
}
/* }}} */

/* {{{ cf_uconf_copy_values */
int cf_uconf_copy_values(cf_configuration_t *config,cf_uconf_directive_t *directive,cf_uconf_directive_t *my_directive,int do_if_empty) {
  cf_uconf_argument_t *arg,my_arg;
  cf_name_value_t *val;
  size_t i;
  int didit = 0;
  u_char *fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);

  memset(my_directive,0,sizeof(*my_directive));
  my_directive->name   = strdup(directive->name);
  if(my_directive->access) my_directive->access = strdup(directive->access);
  my_directive->flags  = directive->flags;
  my_directive->argnum = directive->argnum;

  cf_array_init(&my_directive->arguments,sizeof(my_arg),(void (*)(void *))cf_uconf_destroy_argument);
  memset(&my_arg,0,sizeof(my_arg));

  if((val = cf_cfg_get_first_value(config,fn,directive->name)) == NULL) {
    if(do_if_empty) {
      for(i=0;i<directive->arguments.elements;++i) {
        arg = cf_array_element_at(&directive->arguments,i);
        if(arg->val) {
          didit = 1;
          my_arg.val = strdup(arg->val);
        }

        cf_array_push(&my_directive->arguments,&my_arg);
      }
    }
  }
  else {
    didit = 1;

    for(i=0;i<val->valnum;++i) {
      my_arg.val = strdup(val->values[i]);
      cf_array_push(&my_directive->arguments,&my_arg);
    }
  }

  return didit;
}
/* }}} */

/* {{{ cf_uconf_cleanup_err */
void cf_uconf_cleanup_err(cf_uconf_error_t *err) {
  if(err->directive) free(err->directive);
  if(err->param) free(err->param);
  if(err->error) free(err->error);
}
/* }}} */

/* {{{ cf_uconf_validate */
int cf_uconf_validate(cf_uconf_argument_t *arg,const u_char *content,size_t len) {
  u_char *error;
  int erroffset;
  pcre *regexp;

  /* {{{ validation by type */
  if(arg->validation_type) {
    if(cf_strcmp(arg->validation,"email") == 0)                return is_valid_mailaddress(content);
    else if(cf_strcmp(arg->validation,"http-url") == 0)        return is_valid_http_link(content,0);
    else if(cf_strcmp(arg->validation,"http-url-strict") == 0) return is_valid_http_link(content,1);
    else if(cf_strcmp(arg->validation,"url") == 0)             return is_valid_link(content);
  }
  /* }}} */
  /* {{{ regexp validation */
  else {
    if((regexp = pcre_compile(arg->validation,0,(const char **)&error,&erroffset,NULL)) == NULL) {
      fprintf(stderr,"Error in pattern '%s': %s\n",arg->validation,error);
      return -1;
    }

    if(regexp && pcre_exec(regexp,NULL,content,len,0,0,NULL,0) < 0) {
      pcre_free(regexp);
      return -1;
    }

    pcre_free(regexp);
  }
  /* }}} */

  return 0;
}
/* }}} */

/* {{{ cf_uconf_merge_config */
cf_uconf_userconfig_t *cf_uconf_merge_config(cf_hash_t *head,cf_configuration_t *config,cf_array_t *errormessages,int touch_committed) {
  cf_uconf_userconfig_t *modxml = cf_uconf_read_modxml(),*merged;
  cf_uconf_directive_t *directive,my_directive;
  cf_uconf_argument_t *arg,my_arg;
  cf_cgi_param_t *mult;
  size_t i,j,len;
  cf_string_t str;
  cf_name_value_t *val;
  time_t t;
  u_char buff[512];

  cf_uconf_error_t our_err;

  int didit = 0,err_occured = 0;

  u_char *fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);

  if(!modxml) return NULL;

  merged = cf_alloc(NULL,1,sizeof(*merged),CF_ALLOC_MALLOC);
  cf_array_init(&merged->directives,sizeof(my_directive),(void (*)(void *))cf_uconf_destroy_directive);
  cf_array_init(errormessages,sizeof(our_err),(void (*)(void *))cf_uconf_cleanup_err);

  for(didit=0,i=0;i<modxml->directives.elements;++i,didit=0) {
    directive = cf_array_element_at(&modxml->directives,i);

    memset(&my_directive,0,sizeof(my_directive));
    my_directive.name   = strdup(directive->name);
    if(my_directive.access) my_directive.access = strdup(directive->access);
    my_directive.flags  = directive->flags;
    my_directive.argnum = directive->argnum;

    cf_array_init(&my_directive.arguments,sizeof(my_arg),(void (*)(void *))cf_uconf_destroy_argument);

    if(directive->flags & CF_UCONF_FLAG_INVISIBLE) didit = cf_uconf_copy_values(config,directive,&my_directive,0);
    else {
      for(j=0;j<directive->arguments.elements;++j) {
        arg = cf_array_element_at(&directive->arguments,j);
        memset(&my_arg,0,sizeof(my_arg));
        cf_str_init_growth(&str,128);

        if((mult = cf_cgi_get_multiple(head,arg->param)) != NULL && mult->value.content && *mult->value.content) {
          /* {{{ create new value from CGI parameter(s) */
          cf_str_str_set(&str,&mult->value);

          for(mult=mult->next;mult;mult=mult->next) {
            cf_str_char_append(&str,',');
            cf_str_str_append(&str,&mult->value);
          }
          /* }}} */

          if(cf_uconf_validate(arg,str.content,str.len) != 0) {
            our_err.error     = strdup(arg->error);
            our_err.param     = strdup(arg->param);
            our_err.directive = strdup(directive->name);

            err_occured = 1;
            cf_array_push(errormessages,&our_err);
          }

          /* {{{ closing work, copy value to argument */
          if(err_occured == 0) {
            if(arg->parse && cf_strcmp(arg->parse,"date") == 0) {
              t = cf_transform_date(str.content);
              len = snprintf(buff,512,"%lu",(unsigned long)t);
              cf_str_char_set(&str,buff,len);
            }

            my_arg.val = str.content;
          }
          /* }}} */
        }
        else {
          /* shall we also set new values when value was not committed? */
          if(touch_committed) {
            /* this is only possible if ifNotCommitted was set, else we set it (automatically) to a NULL value */
            if(arg->ifnotcommitted) my_arg.val = strdup(arg->ifnotcommitted);
          }
          else {
            /* we should not touch the not-committed, so copy value from config */
            if((val = cf_cfg_get_first_value(config,fn,directive->name)) != NULL) my_arg.val = strdup(val->values[j]);
          }
        }

        if(err_occured == 0 && my_arg.val) {
          didit = 1;
          cf_array_push(&my_directive.arguments,&my_arg);
        }
      }
    }

    if(didit) cf_array_push(&merged->directives,&my_directive);
  }


  cf_uconf_cleanup_modxml(modxml);
  if(err_occured) {
    cf_uconf_cleanup_modxml(merged);
    merged = NULL;
  }

  return merged;
}
/* }}} */

/* {{{ _uconf_append_escaped */
static void _uconf_append_escaped(cf_string_t *str,const u_char *val) {
  register u_char *ptr;

  for(ptr=(u_char *)val;*ptr;++ptr) {
    switch(*ptr) {
      case '"':
        cf_str_chars_append(str,"\\\"",2);
        break;
      case '\\':
        cf_str_chars_append(str,"\\\\",2);
        break;
      case '\012':
        if(*(ptr+1) == '\015') ++ptr;
        cf_str_chars_append(str,"\\n",2);
        break;
      case '\015':
        if(*(ptr+1) == '\012') ++ptr;
        cf_str_chars_append(str,"\\n",2);
        break;
      default:
        cf_str_char_append(str,*ptr);
    }
  }
}
/* }}} */

/* {{{ cf_write_uconf */
u_char *cf_write_uconf(const u_char *filename,cf_uconf_userconfig_t *merged) {
  cf_uconf_directive_t *directive;
  cf_uconf_argument_t *arg;
  cf_string_t str;
  FILE *fd;

  size_t i,j;
  cf_str_init_growth(&str,512);


  for(i=0;i<merged->directives.elements;++i) {
    directive = cf_array_element_at(&merged->directives,i);

    cf_str_cstr_append(&str,directive->name);

    for(j=0;j<directive->arguments.elements;++j) {
      arg = cf_array_element_at(&directive->arguments,j);

      if(arg->val) {
        cf_str_chars_append(&str," \"",2);
        _uconf_append_escaped(&str,arg->val);
        cf_str_char_append(&str,'"');
      }
    }

    cf_str_char_append(&str,'\n');
  }

  if((fd = fopen(filename,"w")) == NULL) {
    cf_str_cleanup(&str);
    return "E_IO_ERR";
  }
  fwrite(str.content,1,str.len,fd);
  fclose(fd);
  cf_str_cleanup(&str);


  return NULL;
}
/* }}} */

static cf_hash_t *action_handlers = NULL;

/* {{{ cf_uconf_register_action_handler */
int cf_uconf_register_action_handler(u_char *name,cf_uconf_action_handler_t action) {
  if(action_handlers == NULL) action_handlers = cf_hash_new(NULL);
  if(cf_hash_get(action_handlers,name,strlen(name)) != NULL) return -1;

  cf_hash_set_static(action_handlers,name,strlen(name),action);

  return 0;
}
/* }}} */

/* {{{ cf_uconf_get_action_handler */
cf_uconf_action_handler_t cf_uconf_get_action_handler(u_char *name) {
  if(action_handlers == NULL) return NULL;
  return cf_hash_get(action_handlers,name,strlen(name));
}
/* }}} */

/* {{{ cf_uconf_get_conf_val */
const u_char *cf_uconf_get_conf_val(cf_uconf_userconfig_t *uconf,const u_char *name,int argnum) {
  cf_uconf_directive_t *directive;
  cf_uconf_argument_t *arg;
  size_t i;

  for(i=0;i<uconf->directives.elements;++i) {
    directive = cf_array_element_at(&uconf->directives,i);

    if(cf_strcmp(directive->name,name) == 0) {
      arg = cf_array_element_at(&directive->arguments,argnum);

      if(arg) return arg->val;
    }
  }

  return NULL;
}
/* }}} */

/* eof */
