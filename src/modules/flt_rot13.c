/**
 * \file flt_rot13.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 *
 * This plugin handles lot of standard directives
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
/* }}} */

static int flt_rot13_decoded = 0;

/* {{{ flt_rot13_execute */
int flt_rot13_execute(t_configuration *fdc,t_configuration *fvc,t_cl_thread *thread,const u_char *directive,const u_char **parameters,size_t plen,t_string *bco,t_string *bci,t_string *content,t_string *cite,const u_char *qchars,int sig) {
  register u_char *ptr,b1,b2;
  u_char *forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  u_char *link;
  size_t l;

  t_string cnt;

  int un = cf_hash_get(GlobalValues,"UserName",8) != NULL;
  t_name_value *v = cfg_get_first_value(fdc,forum_name,un ? "UPostingURL" : "PostingURL");

  if(!thread) return FLT_DECLINE;

  if(flt_rot13_decoded == 0) {
    link = cf_advanced_get_link(v->values[0],thread->tid,thread->threadmsg->mid,NULL,1,&l,"rot13","decoded");

    str_chars_append(bco,"<a href=\"",9);
    str_chars_append(bco,link,l);
    str_chars_append(bco,"\">",2);

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

    str_chars_append(bco,"</a>",4);
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
int flt_rot13_init(t_cf_hash *cgi,t_configuration *dc,t_configuration *vc) {
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

t_conf_opt flt_rot13_config[] = {
  { NULL, NULL, 0, NULL }
};

t_handler_config flt_rot13_handlers[] = {
  { INIT_HANDLER,     flt_rot13_init },
  { 0, NULL }
};

t_module_config flt_rot13 = {
  flt_rot13_config,
  flt_rot13_handlers,
  NULL,
  NULL,
  NULL,
  NULL
};

/* eof */
