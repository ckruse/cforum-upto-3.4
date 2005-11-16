/**
 * \file flt_jsvalidation.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 *
 * This plugin can validate user input by JS
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
/* }}} */

void flt_jsvalidation_append_escaped(cf_string_t *str,const u_char *val) {
  register u_char *ptr;

  for(ptr=(u_char *)val;*ptr;++ptr) {
    switch(*ptr) {
      case '"': 
        cf_str_chars_append(str,"\\\"",2);
        break;
      case '\'':
        cf_str_chars_append(str,"\\'",2);
        break;
      case '\\':
        cf_str_chars_append(str,"\\\\",2);
        break;
      default:
        cf_str_char_append(str,*ptr);
    }
  }
}

void flt_jsvalidation_uconfjs(cf_hash_t *cgi,cf_configuration_t *dc,cf_configuration_t *uc,cf_template_t *tpl,cf_configuration_t *user) {
  size_t i,j;
  cf_uconf_userconfig_t *modxml = cf_uconf_read_modxml();
  cf_uconf_directive_t *directive;
  cf_uconf_argument_t *arg;
  cf_string_t str;

  if(!modxml) return;
  cf_str_init(&str);
  cf_str_char_set(&str,"js_validation = new Array();\n",29);

  for(i=0;i<modxml->directives.elements;++i) {
    directive = cf_array_element_at(&modxml->directives,i);
    if(directive->flags & CF_UCONF_FLAG_INVISIBLE) continue;

    for(j=0;j<directive->arguments.elements;++j) {
      arg = cf_array_element_at(&directive->arguments,j);

      cf_str_chars_append(&str,"js_validation.push([",20);
      cf_uint32_to_str(&str,(u_int32_t)arg->validation_type);
      cf_str_chars_append(&str,",'",2);
      flt_jsvalidation_append_escaped(&str,arg->validation);
      cf_str_chars_append(&str,"','",3);
      flt_jsvalidation_append_escaped(&str,directive->name);
      cf_str_chars_append(&str,"','",3);
      flt_jsvalidation_append_escaped(&str,arg->param);
      cf_str_chars_append(&str,"','",3);
      if(arg->error) flt_jsvalidation_append_escaped(&str,arg->error);
      cf_str_chars_append(&str,"']);\n",5);
    }
  }

  cf_tpl_setvalue(tpl,"js_validation",TPL_VARIABLE_STRING,str.content,str.len);

  cf_str_cleanup(&str);
  cf_uconf_cleanup_modxml(modxml);
}

cf_handler_config_t flt_jsvalidation_handlers[] = {
  { UCONF_DISPLAY_HANDLER,  flt_jsvalidation_uconfjs },
  { 0, NULL }
};

cf_module_config_t flt_jsvalidation = {
  MODULE_MAGIC_COOKIE,
  NULL,
  flt_jsvalidation_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};

/* eof */
