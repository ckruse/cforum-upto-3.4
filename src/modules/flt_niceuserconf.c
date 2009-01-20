/**
 * \file flt_niceuserconf.c
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

/* {{{ flt_niceuserconf_nicevalues */
void flt_niceuserconf_nicevalues(cf_hash_t *cgi,cf_configuration_t *dc,cf_configuration_t *uc,cf_template_t *tpl,cf_configuration_t *user) {
  u_char *fn = cf_hash_get(GlobalValues,"FORUM_NAME",10),**list;
  register u_char *ptr;

  cf_name_value_t *values,*cs = cf_cfg_get_first_value(&fo_default_conf,fn,"ExternCharset");
  cf_tpl_variable_t var;
  size_t i,len;
  cf_string_t str;

  if((values = cf_cfg_get_first_value(user,fn,"HighlightCategories")) != NULL) {
    cf_tpl_var_init(&var,TPL_VARIABLE_ARRAY);
    len = cf_split(values->values[0],",",&list);

    for(i=0;i<len;++i) {
      cf_add_variable(&var,cs,list[i],strlen(list[i]),1);
      free(list[i]);
    }

    free(list);
    cf_tpl_setvar(tpl,"highcats",&var);
  }

  if((values = cf_cfg_get_first_value(user,fn,"BlackList")) != NULL) {
    cf_str_init(&str);
    for(ptr=values->values[0];*ptr;++ptr) {
      if(*ptr == ',') cf_str_char_append(&str,'\n');
      else cf_str_char_append(&str,*ptr);
    }

    cf_set_variable(tpl,cs,"blacklist",str.content,str.len,1);
    cf_str_cleanup(&str);
  }

  if((values = cf_cfg_get_first_value(user,fn,"WhiteList")) != NULL) {
    cf_str_init(&str);
    for(ptr=values->values[0];*ptr;++ptr) {
      if(*ptr == ',') cf_str_char_append(&str,'\n');
      else cf_str_char_append(&str,*ptr);
    }

    cf_set_variable(tpl,cs,"whitelst",str.content,str.len,1);
    cf_str_cleanup(&str);
  }
}
/* }}} */

/* {{{ flt_niceuserconf_transformer */
int flt_niceuserconf_transformer(cf_hash_t *cgi,cf_configuration_t *dc,cf_configuration_t *uc,cf_configuration_t *olduconf,cf_uconf_userconfig_t *newconf) {
  size_t i;
  cf_uconf_directive_t *dir;
  cf_uconf_argument_t *arg;

  cf_string_t str;
  register u_char *ptr;

  for(i=0;i<newconf->directives.elements;++i) {
    dir = cf_array_element_at(&newconf->directives,i);

    if(cf_strcmp(dir->name,"WhiteList") == 0 || cf_strcmp(dir->name,"BlackList") == 0) {
      if((arg = cf_array_element_at(&dir->arguments,0)) != NULL && arg->val != NULL) {
        cf_str_init(&str);

        for(ptr=arg->val;*ptr;++ptr) {
          if(*ptr == '\012') {
            if(*(ptr+1) == '\015') ++ptr;
            cf_str_char_append(&str,',');
          }
          else if(*ptr == '\015') {
            if(*(ptr+1) == '\012') ++ptr;
            cf_str_char_append(&str,',');
          }
          else cf_str_char_append(&str,*ptr);
        }

        free(arg->val);
        arg->val = str.content;
      }
    }
  }

  return FLT_OK;
}
/* }}} */

cf_handler_config_t flt_niceuserconf_handlers[] = {
  { UCONF_DISPLAY_HANDLER,  flt_niceuserconf_nicevalues },
  { UCONF_WRITE_HANDLER,    flt_niceuserconf_transformer },
  { 0, NULL }
};

cf_module_config_t flt_niceuserconf = {
  MODULE_MAGIC_COOKIE,
  NULL,
  flt_niceuserconf_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};

/* eof */
