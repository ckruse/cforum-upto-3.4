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

#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "template.h"
#include "readline.h"
#include "clientlib.h"

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
  t_name_value *path = cfg_get_first_value(&fo_userconf_conf,fn,"ModuleConfig");

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

/* eof */
