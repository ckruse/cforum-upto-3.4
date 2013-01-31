/**
 * \file flt_rot13.c
 * \author Christian Kruse, <cjk@wwwtech.de>
 *
 * This plugin implements the rot13-feature for the forum
 */

/* {{{ Initial comments */
/*
 * $LastChangedDate: 2009-01-16 14:32:24 +0100 (Fri, 16 Jan 2009) $
 * $LastChangedRevision: 1639 $
 * $LastChangedBy: ckruse $
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
/* }}} */

static int flt_rot13_decoded = 0;

/* {{{ flt_rot13_execute */
int flt_rot13_execute(configuration_t *fdc,configuration_t *fvc,cl_thread_t *thread,const u_char *directive,const u_char **parameters,size_t plen,string_t *bco,string_t *bci,string_t *content,string_t *cite,const u_char *qchars,int sig) {
  register u_char *ptr,b1,b2;
  u_char *link;
  size_t l;

  int un = cf_hash_get(GlobalValues,"UserName",8) != NULL;
  cf_readmode_t *rm = cf_hash_get(GlobalValues,"RM",2);

  if(flt_rot13_decoded == 0 || !thread) {
    if(thread) {
      link = cf_advanced_get_link(rm->posting_uri[un?1:0],thread->tid,thread->threadmsg->mid,NULL,1,&l,"rot13","decoded");

      str_chars_append(bco,"<a href=\"",9);
      str_chars_append(bco,link,l);
      str_chars_append(bco,"\">",2);
    }

    for(ptr=content->content,b1=*ptr;*ptr;++ptr,b1=*ptr) {
      if(*ptr == '<') {
        str_char_append(bco,*ptr);
        for(++ptr;*ptr && *ptr != '>';++ptr) str_char_append(bco,*ptr);
        str_char_append(bco,*ptr);
        continue;
      }

      b2 = b1 & 32;
      b1 &= ~b2;
      b1 = ((b1 >= 'A' && b1 <= 'Z') ? ((b1 - 'A' + 13) % 26 + 'A') : b1) | b2;

      str_char_append(bco,b1);
    }

    if(thread) str_chars_append(bco,"</a>",4);
  }
  else str_str_append(bco,content);

  if(cite) {
    str_chars_append(bci,"[rot13]",7);
    str_str_append(bci,cite);
    str_chars_append(bci,"[/rot13]",8);
  }

  return FLT_OK;
}
/* }}} */

/* {{{ flt_rot13_init */
int flt_rot13_init(cf_hash_t *cgi,configuration_t *dc,configuration_t *vc) {
  u_char *ptr;

  cf_html_register_directive("rot13",flt_rot13_execute,CF_HTML_DIR_TYPE_ARG|CF_HTML_DIR_TYPE_BLOCK);

  if(cgi) {
    if((ptr = cf_cgi_get(cgi,"rot13")) != NULL) {
      flt_rot13_decoded = cf_strcmp(ptr,"decoded") == 0;
    }
  }

  return FLT_OK;
}
/* }}} */

conf_opt_t flt_rot13_config[] = {
  { NULL, NULL, 0, NULL }
};

handler_config_t flt_rot13_handlers[] = {
  { INIT_HANDLER,     flt_rot13_init },
  { 0, NULL }
};

module_config_t flt_rot13 = {
  MODULE_MAGIC_COOKIE,
  flt_rot13_config,
  flt_rot13_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};

/* eof */
