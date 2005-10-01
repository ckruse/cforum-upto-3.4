/**
 * \file wwwutils.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 * \brief WWW utilities for the Classic Forum
 *
 * This file contains some WWW utility functions for the Classic Forum, e.g. GET-request, POST-request,
 * etc
 */

/* {{{ Initial headers */
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <errno.h>

#include <sys/time.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>

#include <WWWLib.h>
#include <WWWHTTP.h>
#include <WWWInit.h>

#include "charconvert.h"
#include "utils.h"
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


/* {{{ cf_http_simple_get_uri */
cf_http_response_t *cf_http_simple_get_uri(const u_char *uri) {
  u_char *cwd,*absolute_url,*name,*value;
  HTRequest *request;
  HTChunk *chunk = NULL;
  HTResponse *response;
  HTParentAnchor *panch;

  cf_http_response_t *rsp = NULL;

  HTAssocList *hdrlst;
  HTAssoc *pres;

  /* Initialize libwww core */
  HTProfile_newPreemptiveClient("Classic Forum", CF_VERSION);

  /* Gotta set up our own traces */
  HTPrint_setCallback(printer);
  HTTrace_setCallback(tracer);

  /* create request */
  request = HTRequest_new();

  /* We want raw output including headers */
  HTRequest_setOutputFormat(request, WWW_SOURCE);

  /* Close connection immediately */
  HTRequest_addConnection(request, "close", "");

  /* Add our own filter to handle termination */
  HTNet_addAfter(terminate_handler, NULL, NULL, HT_ALL, HT_FILTER_LAST);

  cwd          = HTGetCurrentDirectoryURL();
  absolute_url = HTParse(uri, cwd, PARSE_ALL);
  chunk        = HTLoadToChunk(absolute_url, request);

  HT_FREE(absolute_url);
  HT_FREE(cwd);

  /* If chunk != NULL then we have the data */
  if (chunk) {
    rsp          = fo_alloc(NULL,1,sizeof(*rsp),FO_ALLOC_MALLOC);
    rsp->reason  = NULL;
    rsp->status  = 0;
    rsp->headers = cf_hash_new(NULL);

    str_init(&rsp->content);

    if((response = HTRequest_response(request)) != NULL) rsp->reason = strdup(HTResponse_reason(response));

    if((panch = HTRequest_anchor(request)) != NULL) {
      if((hdrlst = HTAnchor_header(panch)) != NULL) {
        while((pres = (HTAssoc *)HTAssocList_nextObject(hdrlst)) != NULL) {
          name  = HTAssoc_name(pres);
          value = HTAssoc_value(pres);

          cf_hash_set(rsp->headers,name,strlen(name),value,strlen(value)+1);
        }
      }
    }

    /* print the chunk result */
    rsp->content.len     = rsp->content.reserved = HTChunk_size(chunk);
    rsp->content.content = HTChunk_toCString(chunk);
  }
  
  /* Clean up the request */
  HTRequest_delete(request);

  /* Terminate the Library */
  HTProfile_delete();

  return rsp;
}
/* }}} */

/* {{{ cf_http_simple_post_uri */
cf_http_response_t *cf_http_simple_post_uri(const u_char *uri,const u_char *postdata,size_t len) {
  HTRequest * request = NULL;
  HTParentAnchor *src = NULL;
  HTAnchor *dst = NULL;
  HTStream *target_stream;
  HTChunk *response_data;
  HTResponse *response;
  HTParentAnchor *panch;

  HTAssocList *hdrlst;
  HTAssoc *pres;

  cf_http_response_t *rsp = NULL;
  
  BOOL status = NO;
  u_char *cwd,*name,*value;

  /* Create a new premptive client */
  HTProfile_newNoCacheClient("CForum", CF_VERSION);

  /* Need our own trace and print functions */
  HTPrint_setCallback(printer);
  HTTrace_setCallback(tracer);

  /* Add our own filter to update the history list */
  HTNet_addAfter(terminate_handler, NULL, NULL, HT_ALL, HT_FILTER_LAST);

  cwd     = HTGetCurrentDirectoryURL();
  request = HTRequest_new();
  dst     = HTAnchor_findAddress(uri);
  src     = HTTmpAnchor(NULL);

  target_stream = HTStreamToChunk(request,&response_data, 0);

  HTRequest_setOutputStream(request, target_stream);    
  HTRequest_setOutputFormat(request, WWW_SOURCE);

  HTAnchor_setDocument(src,(char *)postdata);
  HTAnchor_setFormat(src,WWW_PLAINTEXT);
  HTAnchor_setLength(src,len);

  status = HTPostAnchor(src,dst,request);

  HT_FREE(cwd);

  if(status == YES) {
    HTEventList_loop(request);

    rsp          = fo_alloc(NULL,1,sizeof(*rsp),FO_ALLOC_MALLOC);
    rsp->reason  = NULL;
    rsp->status  = 0;
    rsp->headers = cf_hash_new(NULL);

    str_init(&rsp->content);

    if((response = HTRequest_response(request)) != NULL) rsp->reason = strdup(HTResponse_reason(response));

    if((panch = HTRequest_anchor(request)) != NULL) {
      if((hdrlst = HTAnchor_header(panch)) != NULL) {
        while((pres = (HTAssoc *)HTAssocList_nextObject(hdrlst)) != NULL) {
          name  = HTAssoc_name(pres);
          value = HTAssoc_value(pres);

          cf_hash_set(rsp->headers,name,strlen(name),value,strlen(value)+1);
        }
      }
    }

    /* print the chunk result */
    rsp->content.len     = rsp->content.reserved = HTChunk_size(response_data);
    rsp->content.content = HTChunk_toCString(response_data);
  }


  /* Clean up the request */
  HTRequest_delete(request);

  /* Terminate the Library */
  HTProfile_delete();

  return rsp;
}
/* }}} */

/* {{{ cf_http_destroy_response */
void cf_http_destroy_response(cf_http_response_t *rsp) {
  if(rsp->reason) free(rsp->reason);
  if(rsp->headers) cf_hash_destroy(rsp->headers);

  str_cleanup(&rsp->content);
}
/* }}} */

/* {{{ cf_http_redirect_nice_uri */
void cf_http_redirect_with_nice_uri(const u_char *ruri,int perm) {
  u_char *tmp,*port,*https;
  register u_char *ptr;
  string_t uri;
  int slash = 0;

  str_init_growth(&uri,256);

  if(cf_strncmp(ruri,"http://",7) != 0 && cf_strncmp(ruri,"https://",8) != 0) {
    port  = getenv("SERVER_PORT");
    https = getenv("HTTPS");

    if(https || cf_strncmp(port,"443",3) == 0) str_chars_append(&uri,"https://",8);
    else str_chars_append(&uri,"http://",7);

    tmp = getenv("SERVER_NAME");
    str_chars_append(&uri,tmp,strlen(tmp));

    if(cf_strcmp(port,"80") != 0 && cf_strcmp(port,"443") != 0) {
      str_char_append(&uri,':');
      str_chars_append(&uri,tmp,strlen(tmp));
    }

    str_char_append(&uri,'/');
    slash = 1;

    ptr = (u_char *)ruri;
  }
  else if(cf_strncmp(ruri,"http://",7) == 0) {
    str_chars_append(&uri,"http://",7);
    ptr = (u_char *)ruri + 7 * sizeof(u_char);
  }
  else {
    str_chars_append(&uri,"https://",8);
    ptr = (u_char *)ruri + 8 * sizeof(u_char);
  }

  for(;*ptr;++ptr) {
    switch(*ptr) {
      case '/':
        if(slash == 0) str_char_append(&uri,'/');
        slash = 1;
        break;
      default:
        slash = 0;
        str_char_append(&uri,*ptr);
    }
  }

  if(perm) printf("Status: 301 Moved Permanently\015\012");
  else printf("Status: 302 Moved Temporarily\015\012");

  printf("Location: %s\015\012\015\012",uri.content);
}
/* }}} */



/* eof */
