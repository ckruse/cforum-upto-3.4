/**
 * \file flt_importexport.c
 * \author Christian Kruse, <cjk@wwwtech.de>
 *
 * This plugin sets userconf values to a nicer representation
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

#include <errno.h>

#include <gdome.h>

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
#include "userconf.h"
#include "xml_handling.h"
/* }}} */

static u_char *flt_importexport_fn   = NULL;
static u_char *flt_importexport_form = NULL;
static u_char *flt_importexport_ok   = NULL;

/* {{{ flt_importexport_export */
void flt_importexport_export(cf_hash_t *cgi,cf_configuration_t *dc,cf_configuration_t *uc,cf_configuration_t *uconf) {
  u_char *fn = cf_hash_get(GlobalValues,"FORUM_NAME",10),*uname = cf_hash_get(GlobalValues,"UserName",8);

  cf_uconf_userconfig_t *modxml;
  size_t i,j;
  cf_name_value_t *vals,*cs = cf_cfg_get_first_value(&fo_default_conf,fn,"ExternCharset");
  cf_string_t str;
  cf_uconf_directive_t *directive;

  if(uname == NULL) {
    printf("Status: 403 Forbidden\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
    cf_error_message("E_MUST_AUTH",NULL);
    return;
  }

  if((modxml = cf_uconf_read_modxml()) == NULL) {
    printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
    cf_error_message("E_dontknow",NULL);
    return;
  }

  cf_str_init(&str);
  cf_str_char_set(&str,"<!DOCTYPE CFConfig SYSTEM \"http://wwwtech.de/cforum/download/cfconfig-0.1.dtd\">\n<CFConfig version=\"0.1\">\n",105);

  for(i=0;i<modxml->directives.elements;++i) {
    directive = cf_array_element_at(&modxml->directives,i);
    if(directive->flags & CF_UCONF_FLAG_INVISIBLE) continue;

    if((vals = cf_cfg_get_first_value(uc,fn,directive->name)) != NULL) {
      cf_str_cstr_append(&str,"<Directive name=\"");
      cf_str_cstr_append(&str,directive->name);
      cf_str_chars_append(&str,"\">",2);

      for(j=0;j<vals->valnum;++j) {
        cf_str_cstr_append(&str,"<Argument><![CDATA[");
        cf_str_cstr_append(&str,vals->values[j]);
        cf_str_cstr_append(&str,"]]></Argument>");
      }

      cf_str_cstr_append(&str,"</Directive>\n");
    }
  }

  cf_str_cstr_append(&str,"</CFConfig>");

  printf("Content-Type: text/xml; charset=UTF-8\015\012Content-Disposition: attachment; filename=%s.xml\015\012\015\012",uname);
  fwrite(str.content,str.len,1,stdout);

  cf_str_cleanup(&str);
}
/* }}} */

/* {{{ flt_importexport_importform */
void flt_importexport_importform(cf_hash_t *cgi,cf_configuration_t *dc,cf_configuration_t *uc,cf_configuration_t *uconf) {
  u_char tplname[256];
  cf_template_t tpl;
  cf_name_value_t *cs = cf_cfg_get_first_value(&fo_default_conf,flt_importexport_fn,"ExternCharset"),
    *ubase  = cf_cfg_get_first_value(&fo_default_conf,flt_importexport_fn,"UBaseURL"),
    *script = cf_cfg_get_first_value(&fo_default_conf,flt_importexport_fn,"UserConfig");

  cf_gen_tpl_name(tplname,256,flt_importexport_form);
  if(cf_tpl_init(&tpl,tplname) != 0) {
    printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
    cf_error_message("E_TPL_NOT_FOUND",NULL);
    return;
  }

  printf("Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);

  cf_set_variable(&tpl,cs,"forumbase",ubase->values[0],strlen(ubase->values[0]),1);
  cf_set_variable(&tpl,cs,"script",script->values[0],strlen(script->values[0]),1);
  cf_set_variable(&tpl,cs,"charset",cs->values[0],strlen(cs->values[0]),1);

  cf_tpl_parse(&tpl);
  cf_tpl_finish(&tpl);
}
/* }}} */

/* {{{ flt_importexport_to_internal */
cf_uconf_userconfig_t *flt_importexport_to_internal(GdomeDocument *doc) {
  GdomeDOMString *tagname = gdome_str_mkref("Directive"),*name;
  GdomeNodeList *nl,*chlds;
  GdomeException exc;
  GdomeNode *ndirectve,*narg;

  gulong i,len,j,len1;

  cf_uconf_directive_t directive;
  cf_uconf_argument_t argument;
  cf_uconf_userconfig_t *ownuconf = cf_alloc(NULL,1,sizeof(*ownuconf),CF_ALLOC_MALLOC);

  cf_array_init(&ownuconf->directives,sizeof(directive),(void (*)(void *))cf_uconf_destroy_directive);

  if((nl = gdome_doc_getElementsByTagName(doc,tagname,&exc)) != NULL) {
    len = gdome_nl_length(nl,&exc);

    for(i=0;i<len;++i) {
      ndirectve = gdome_nl_item(nl,i,&exc);
      chlds = gdome_n_childNodes(ndirectve,&exc);

      memset(&directive,0,sizeof(directive));
      cf_array_init(&directive.arguments,sizeof(argument),(void (*)(void *))cf_uconf_destroy_argument);

      directive.name = xml_get_attribute(ndirectve,"name");

      len1 = gdome_nl_length(chlds,&exc);
      for(j=0;j<len1;++j) {
        memset(&argument,0,sizeof(argument));

        narg = gdome_nl_item(chlds,j,&exc);
        name = gdome_n_nodeName(narg,&exc);

        if(cf_strcmp(name->str,"Argument") == 0) {
          argument.val = xml_get_node_value(narg);
          cf_array_push(&directive.arguments,&argument);
        }

        gdome_str_unref(name);
        gdome_n_unref(narg,&exc);
      }

      gdome_n_unref(ndirectve,&exc);
      gdome_nl_unref(chlds,&exc);

      cf_array_push(&ownuconf->directives,&directive);
    }

  }
  else {
    gdome_str_unref(tagname);
    gdome_nl_unref(nl,&exc);
    free(ownuconf);
    return NULL;
  }

  gdome_str_unref(tagname);
  gdome_nl_unref(nl,&exc);

  return ownuconf;
}
/* }}} */

/* {{{ flt_importexport_get_directive */
cf_uconf_directive_t *flt_importexport_get_directive(cf_uconf_userconfig_t *xml,const u_char *name) {
  cf_uconf_directive_t *directive;
  size_t i;

  for(i=0;i<xml->directives.elements;++i) {
    directive = cf_array_element_at(&xml->directives,i);
    if(cf_strcmp(directive->name,name) == 0) return directive;
  }

  return NULL;
}
/* }}} */

/* {{{ flt_importexport_merge */
cf_uconf_userconfig_t *flt_importexport_merge(cf_configuration_t *uconf,cf_uconf_userconfig_t *intrnl) {
  size_t i,j;
  u_char *fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);

  cf_uconf_userconfig_t *modxml = cf_uconf_read_modxml(),*merged = cf_alloc(NULL,1,sizeof(*merged),CF_ALLOC_MALLOC);
  cf_uconf_directive_t directive,*dir,*mdir;
  cf_uconf_argument_t argument,*arg,*marg;
  cf_name_value_t *vals;

  cf_array_init(&merged->directives,sizeof(directive),(void (*)(void *))cf_uconf_destroy_directive);

  for(i=0;i<modxml->directives.elements;++i) {
    dir    = cf_array_element_at(&modxml->directives,i);
    vals   = cf_cfg_get_first_value(uconf,fn,dir->name);

    if((dir->flags & CF_UCONF_FLAG_INVISIBLE) || (mdir = flt_importexport_get_directive(intrnl,dir->name)) == NULL) { /* we got user value; validate it and, if it is valid, push it */
      if(vals) {
        memset(&directive,0,sizeof(directive));
        directive.name = strdup(dir->name);
        cf_array_init(&directive.arguments,sizeof(argument),(void (*)(void *))cf_uconf_destroy_argument);

        for(j=0;j<vals->valnum;++j) {
          memset(&argument,0,sizeof(argument));
          argument.val = strdup(vals->values[j]);
          cf_array_push(&directive.arguments,&argument);
        }

        cf_array_push(&merged->directives,&directive);
      }
    }
    else {
      memset(&directive,0,sizeof(directive));
      directive.name = strdup(dir->name);
      cf_array_init(&directive.arguments,sizeof(argument),(void (*)(void *))cf_uconf_destroy_argument);

      for(j=0;j<mdir->arguments.elements;++j) {
        arg  = cf_array_element_at(&dir->arguments,j);
        marg = cf_array_element_at(&mdir->arguments,j);

        memset(&argument,0,sizeof(argument));

        if(cf_uconf_validate(arg,marg->val,strlen(marg->val)) == 0) argument.val = strdup(marg->val);
        else {
          if(vals && j < vals->valnum) argument.val = strdup(vals->values[j]);
        }

        cf_array_push(&directive.arguments,&argument);
      }

      cf_array_push(&merged->directives,&directive);
    }
  }

  if(merged->directives.elements == 0) {
    free(merged);
    merged = NULL;
  }

  return merged;
}
/* }}} */

/* {{{ flt_importexport_import */
void flt_importexport_import(cf_hash_t *cgi,cf_configuration_t *dc,cf_configuration_t *uc,cf_configuration_t *uconf) {
  u_char *fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  register u_char *ptr;
  u_char *msg,tplname[256],*ucfg,*uname;

  GdomeException exc;
  GdomeDocument *doc;
  GdomeDOMImplementation *di;

  cf_string_t *imprtcfg,str1;
  cf_name_value_t *cs = cf_cfg_get_first_value(dc,fn,"ExternCharset"),
    *ubase  = cf_cfg_get_first_value(dc,flt_importexport_fn,"UBaseURL"),
    *script = cf_cfg_get_first_value(dc,flt_importexport_fn,"UserConfig");

  cf_uconf_userconfig_t *intrnl,*merged;

  cf_template_t tpl;

  if(!cgi || (imprtcfg = cf_cgi_get(cgi,"import")) == NULL) {
    printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
    cf_error_message("E_IMPORT_DATAFAILURE",NULL);
    return;
  }

  memset(&str1,0,sizeof(str1));

  for(ptr=imprtcfg->content;*ptr && (isspace(*ptr) || *ptr == '\015' || *ptr == '\012');++ptr);
  if(cf_strncmp(ptr,"<!DOCTYPE",9) == 0) {
    if(cf_strncmp(ptr,"<!DOCTYPE CFConfig",18) == 0) {
      if(cf_strncmp(ptr,"<!DOCTYPE CFConfig SYSTEM \"http://wwwtech.de/cforum/download/cfconfig",69) != 0) {
        printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
        cf_error_message("E_IMPORT_DATAFAILURE",NULL);
        return;
      }
    }
    else {
      printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
      cf_error_message("E_IMPORT_DATAFAILURE",NULL);
      return;
    }
  }
  else {
    cf_str_init(&str1);
    cf_str_char_set(&str1,"<!DOCTYPE CFConfig SYSTEM \"http://wwwtech.de/cforum/download/cfconfig-0.1.dtd\">\n",80);
    cf_str_str_append(&str1,imprtcfg);
  }

  di = gdome_di_mkref();

  if(str1.content) {
    /* create document from str1 */
    doc = gdome_di_createDocFromMemory(di,str1.content,GDOME_LOAD_PARSING|GDOME_LOAD_VALIDATING,&exc);
    cf_str_cleanup(&str1);
  }
  /* create document from CGI string */
  else doc = gdome_di_createDocFromMemory(di,imprtcfg->content,GDOME_LOAD_PARSING|GDOME_LOAD_VALIDATING,&exc);

  if(!doc) {
    printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
    cf_error_message("E_IMPORT_DATAFAILURE",NULL);
    gdome_di_unref(di,&exc);
    return;
  }

  if((intrnl = flt_importexport_to_internal(doc)) == NULL) {
    printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
    cf_error_message("E_IMPORT_DATAFAILURE",NULL);
    gdome_doc_unref(doc,&exc);
    gdome_di_unref(di,&exc);
    return;
  }

  gdome_doc_unref(doc,&exc);
  gdome_di_unref(di,&exc);

  if((merged = flt_importexport_merge(uconf,intrnl)) == NULL) {
    printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
    cf_error_message("E_IMPORT_DATAFAILURE",NULL);
    cf_uconf_cleanup_modxml(intrnl);
    return;
  }

  if(cf_run_uconf_write_handlers(cgi,dc,uc,uconf,merged) == FLT_EXIT) {
    //printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
    //cf_error_message("E_IMPORT_DATAFAILURE",NULL);

    cf_uconf_cleanup_modxml(intrnl);
    cf_uconf_cleanup_modxml(merged);
    return;
  }

  if((uname = cf_hash_get(GlobalValues,"UserName",8)) == NULL) {
    printf("Status: 403 Forbidden\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
    cf_error_message("E_MUST_AUTH",NULL);
    return;
  }

  if((ucfg = cf_get_uconf_name(uname)) == NULL) {
    printf("Status: 403 Forbidden\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
    cf_error_message("E_MUST_AUTH",NULL);
    return;
  }

  if((msg = cf_write_uconf(ucfg,merged)) != NULL) {
    printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
    cf_error_message(msg,NULL,strerror(errno));
  }
  else {
    cf_gen_tpl_name(tplname,256,flt_importexport_ok);
    if(cf_tpl_init(&tpl,tplname) != 0) {
      printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
      cf_error_message("E_TPL_NOT_FOUND",NULL);

      cf_uconf_cleanup_modxml(intrnl);
      cf_uconf_cleanup_modxml(merged);
      free(ucfg);
      return;
    }

    printf("Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);

    cf_set_variable(&tpl,cs,"forumbase",ubase->values[0],strlen(ubase->values[0]),1);
    cf_set_variable(&tpl,cs,"script",script->values[0],strlen(script->values[0]),1);
    cf_set_variable(&tpl,cs,"charset",cs->values[0],strlen(cs->values[0]),1);

    cf_tpl_parse(&tpl);
    cf_tpl_finish(&tpl);
  }

  free(ucfg);
  cf_uconf_cleanup_modxml(intrnl);
  cf_uconf_cleanup_modxml(merged);
}
/* }}} */

/* {{{ flt_importexport_init */
int flt_importexport_init(cf_hash_t *cgi,cf_configuration_t *dc,cf_configuration_t *vc) {
  cf_uconf_register_action_handler("exprt",flt_importexport_export);
  cf_uconf_register_action_handler("imprtform",flt_importexport_importform);
  cf_uconf_register_action_handler("imprt",flt_importexport_import);

  return FLT_OK;
}
/* }}} */

/* {{{ flt_importexport_cleanup */
void flt_importexport_cleanup(void) {
  if(flt_importexport_form) free(flt_importexport_form);
  if(flt_importexport_ok) free(flt_importexport_ok);
}
/* }}} */

/* {{{ flt_importexport_handle */
int flt_importexport_handle(cf_configfile_t *cf,cf_conf_opt_t *opt,const u_char *context,u_char **args,size_t argnum) {
  if(flt_importexport_fn == NULL) flt_importexport_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(flt_importexport_fn,context) != 0) return 0;

  if(cf_strcmp(opt->name,"ImportForm") == 0) {
    if(flt_importexport_form) free(flt_importexport_form);
    flt_importexport_form = strdup(args[0]);
  }
  else {
    if(flt_importexport_ok) free(flt_importexport_ok);
    flt_importexport_ok = strdup(args[0]);
  }

  return 0;
}
/* }}} */

cf_conf_opt_t flt_importexport_config[] = {
  { "ImportForm", flt_importexport_handle, CF_CFG_OPT_CONFIG|CF_CFG_OPT_NEEDED|CF_CFG_OPT_LOCAL, NULL },
  { "ImportOk",   flt_importexport_handle, CF_CFG_OPT_CONFIG|CF_CFG_OPT_NEEDED|CF_CFG_OPT_LOCAL, NULL },
  { NULL, NULL, 0, NULL }
};

cf_handler_config_t flt_importexport_handlers[] = {
  { INIT_HANDLER,  flt_importexport_init },
  { 0, NULL }
};

cf_module_config_t flt_importexport = {
  MODULE_MAGIC_COOKIE,
  flt_importexport_config,
  flt_importexport_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  flt_importexport_cleanup
};

/* eof */
