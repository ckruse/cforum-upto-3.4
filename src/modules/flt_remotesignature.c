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
#include "config.h"
#include "defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>

#include <time.h>

#include <WWWLib.h>
#include <WWWHTTP.h>
#include <WWWInit.h>

#include "readline.h"
#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "cfcgi.h"
#include "template.h"
#include "clientlib.h"
#include "validate.h"
#include "fo_post.h"
/* }}} */

/* {{{ printer */
PRIVATE int printer(const char * fmt, va_list pArgs) {
  return 0;
}
/* }}} */

/* {{{ tracer */
PRIVATE int tracer(const char * fmt, va_list pArgs) {
  return 0;
}
/* }}} */

/* {{{ terminate_handler */
PRIVATE int terminate_handler(HTRequest *request, HTResponse *response, void *param, int status)  {
  /* we're not handling other requests */
  HTEventList_stopLoop ();
 
  /* stop here */
  return HT_ERROR;
}
/* }}} */

/* {{{ flt_remotesignature_get_url */
u_char *flt_remotesignature_get_url(const u_char *url) {
  u_char *cwd,*absolute_url;
  HTRequest* request = HTRequest_new();
  HTChunk* chunk = NULL;
  u_char *string = NULL;

  /* Initialize libwww core */
  HTProfile_newPreemptiveClient("ClassicForum", CF_VERSION);

  /* Gotta set up our own traces */
  HTPrint_setCallback(printer);
  HTTrace_setCallback(tracer);

  /* We want raw output including headers */
  HTRequest_setOutputFormat(request, WWW_SOURCE);

  /* Close connection immediately */
  HTRequest_addConnection(request, "close", "");

  /* Add our own filter to handle termination */
  HTNet_addAfter(terminate_handler, NULL, NULL, HT_ALL, HT_FILTER_LAST);

  cwd = HTGetCurrentDirectoryURL();
  absolute_url = HTParse(url, cwd, PARSE_ALL);
  chunk = HTLoadToChunk(absolute_url, request);

  HT_FREE(absolute_url);
  HT_FREE(cwd);

  /* If chunk != NULL then we have the data */
  if (chunk) {
    /* Go into the event loop... */
    HTEventList_loop(request);

    /* print the chunk result */
    string = HTChunk_toCString(chunk);
  }
  
  /* Clean up the request */
  HTRequest_delete(request);

  /* Terminate the Library */
  HTProfile_delete();

  return string;
}
/* }}} */

/* {{{ flt_remotesignature_execute */
int flt_remotesignature_execute(t_cf_hash *head,t_configuration *dc,t_configuration *pc,t_message *p,int sock,int mode) {
  u_char *rs = strstr(p->content.content,"[remote-signature:"),*url,*cnt;
  register u_char *ptr;

  if(rs) {
    for(ptr=rs+18;*ptr && *ptr != ']' && !isspace(*ptr);ptr++);

    if(*ptr == ']') {
      url = strndup(rs+18,ptr-(rs+18));

      /* we only accept strict URLs */
      if(is_valid_http_link(url,1) == 0) {
        /* get content from URL */
        if((cnt = flt_remotesignature_get_url(url)) != NULL) {
          p->content.len = rs-p->content.content;
          str_chars_append(&p->content,cnt,strlen(cnt));

          free(cnt);
        }
      }

      free(url);
    }
  }
  
  return FLT_DECLINE;
}
/* }}} */

t_conf_opt flt_remotesignature_config[] = {
  { NULL, NULL, 0, NULL }
};

t_handler_config flt_remotesignature_handlers[] = {
  { NEW_POST_HANDLER, flt_remotesignature_execute },
  { 0, NULL }
};

t_module_config flt_remotesignature = {
  flt_remotesignature_config,
  flt_remotesignature_handlers,
  NULL,
  NULL,
  NULL,
  NULL
};

/* eof */
