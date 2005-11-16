/*
 * \file flt_remotesignature.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 * \brief Remote signature plugin
 *
 * This file is a plugin for fo_post. It fetches signature lines
 * from a remote hosts.
 *
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
#include <ctype.h>
#include <string.h>
#include <sys/types.h>

#include <time.h>

#include "readline.h"
#include "hashlib.h"
#include "utils.h"
#include "cfgcomp.h"
#include "cfcgi.h"
#include "template.h"
#include "clientlib.h"
#include "validate.h"
#include "charconvert.h"
#include "fo_post.h"
/* }}} */

/* {{{ flt_remotesignature_execute */
#ifdef CF_SHARED_MEM
int flt_remotesignature_execute(cf_hash_t *head,cf_configuration_t *dc,cf_configuration_t *pc,message_t *p,cl_thread_t *thr,void *mptr,int sock,int mode)
#else
int flt_remotesignature_execute(cf_hash_t *head,cf_configuration_t *dc,cf_configuration_t *pc,message_t *p,cl_thread_t *thr,int sock,int mode)
#endif
{
  u_char *rs = strstr(p->content.content,"[remote-signature:"),*tmp,*url,*bottom;
  register u_char *ptr;
  cf_string_t *str;
  cf_http_response_t *rsp;

  if(rs) {
    for(ptr=rs+18;*ptr && *ptr != ']' && !isspace(*ptr);ptr++);

    if(*ptr == ']') {
      tmp    = strndup(rs+18,ptr-(rs+18));
      url    = htmlentities_decode(tmp,NULL);
      bottom = strdup(ptr+1);
      free(tmp);

      /* we only accept strict URLs */
      if(is_valid_http_link(url,1) == 0) {
        /* get content from URL */
        if((rsp = cf_http_simple_get_uri(url,0)) != NULL) {
          str = body_plain2coded(rsp->content.content);

          cf_http_destroy_response(rsp);
          free(rsp);

          if(str) {
            if(is_valid_utf8_string(str->content,str->len) == 0) {
              p->content.len = rs-p->content.content;
              cf_str_str_append(&p->content,str);
              cf_str_chars_append(&p->content,bottom,strlen(bottom));
            }

            cf_str_cleanup(str);
            free(str);
          }
        }
      }

      free(url);
      free(bottom);
    }
  }

  return FLT_DECLINE;
}
/* }}} */

cf_conf_opt_t flt_remotesignature_config[] = {
  { NULL, NULL, 0, NULL }
};

cf_handler_config_t flt_remotesignature_handlers[] = {
  { NEW_POST_HANDLER, flt_remotesignature_execute },
  { 0, NULL }
};

cf_module_config_t flt_remotesignature = {
  MODULE_MAGIC_COOKIE,
  flt_remotesignature_config,
  flt_remotesignature_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};

/* eof */
