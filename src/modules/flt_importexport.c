/**
 * \file flt_importexport.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
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
#include "userconf.h"
/* }}} */

/* {{{ flt_importexport_export */
void flt_importexport_export(cf_hash_t *cgi,configuration_t *dc,configuration_t *uc) {
  u_char *fn = cf_hash_get(GlobalValues,"FORUM_NAME",10),*uname = cf_hash_get(GlobalValues,"UserName",8);

  uconf_userconfig_t *modxml;
  size_t i,j;
  name_value_t *vals,*cs = cfg_get_first_value(&fo_default_conf,fn,"ExternCharset");
  string_t str;
  uconf_directive_t *directive;
  uconf_argument_t *arg;

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

  str_init(&str);
  str_cstr_set(&str,"<CFConfig version=\"0.1\">\n");

  for(i=0;i<modxml->directives.elements;++i) {
    directive = array_element_at(&modxml->directives,i);
    if(directive->flags & CF_UCONF_FLAG_INVISIBLE) continue;

    if((vals = cfg_get_first_value(uc,fn,directive->name)) != NULL) {
      str_cstr_append(&str,"<Directive name=\"");
      str_cstr_append(&str,directive->name);
      str_chars_append(&str,"\">",2);

      for(j=0;j<vals->valnum;++j) {
        str_cstr_append(&str,"<Argument><![CDATA[");
        str_cstr_append(&str,vals->values[j]);
        str_cstr_append(&str,"]]></Argument>");
      }

      str_cstr_append(&str,"</Directive>\n");
    }
  }

  str_cstr_append(&str,"</CFConfig>");

  printf("Content-Type: text/xml; charset=UTF-8\015\012Content-Disposition: attachment; filename=%s.xml\015\012\015\012",uname);
  fwrite(str.content,str.len,1,stdout);

  str_cleanup(&str);
}
/* }}} */

int flt_importexport_init(cf_hash_t *cgi,configuration_t *dc,configuration_t *vc) {
  uconf_register_action_handler("exprt",flt_importexport_export);

  return FLT_OK;
}

handler_config_t flt_importexport_handlers[] = {
  { INIT_HANDLER,  flt_importexport_init },
  { 0, NULL }
};

module_config_t flt_importexport = {
  MODULE_MAGIC_COOKIE,
  NULL,
  flt_importexport_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};

/* eof */
