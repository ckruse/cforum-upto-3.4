/**
 * \file wwwutils.c
 * \author Christian Kruse, <cjk@wwwtech.de>
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
#include "cfconfig.h"
#include "defines.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <errno.h>

#include <locale.h>
#include <sys/time.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>

#include <curl/curl.h>

#include "charconvert.h"
#include "utils.h"
/* }}} */


/* {{{ cf_http_header_callback */
size_t cf_http_header_callback(void *buffer, size_t size, size_t nmemb, void *userp) {
  cf_http_request_t *rq = (cf_http_request_t *)userp;
  size_t x = size * nmemb;
  float version;
  int status;
  u_char *ptr,*name,*value;

  if(x > 7 && cf_strncmp(buffer,"HTTP/",5) == 0) {
    version = atof(buffer+5);

    for(ptr=buffer+8;ptr < (u_char *)(buffer+x) && !isdigit(*ptr);++ptr);
    status = strtol(ptr,(char **)&ptr,10);

    if(status > 300 && status < 400 && status != 304) {
      if(rq->follow) rq->_donttouch = 1;
      else {
        for(;ptr < (u_char *)(buffer+x) && isspace(*ptr);++ptr);
        rq->rsp->reason = strndup(ptr,(u_char *)(buffer+x)-ptr);
        rq->rsp->status = status;
        rq->rsp->version= version;
      }
    }
    else {
      for(;ptr < (u_char *)(buffer+x) && isspace(*ptr);++ptr);

      rq->rsp->reason = strndup(ptr,(u_char *)(buffer+x)-ptr);
      rq->rsp->status = status;
      rq->rsp->version= version;
      rq->_donttouch = 0;
    }
  }
  else {
    if(rq->_donttouch == 0) {
      for(ptr=buffer;ptr<(u_char *)(buffer+x) && *ptr != ':';++ptr);

      if(*ptr == '\0') return x; /* ignore empty headers */
      name = strndup(buffer,ptr-(u_char *)buffer);

      for(++ptr;ptr < (u_char *)(buffer+x) && isspace(*ptr);++ptr);
      if(*ptr == '\0') {
        free(name);
        return x;
      }

      value = strndup(ptr,(u_char *)(buffer+x)-ptr-2); /* remove \r\n */

      if(rq->rsp->headers == NULL) rq->rsp->headers = cf_hash_new(NULL);
      cf_hash_set(rq->rsp->headers,name,strlen(name)+1,value,strlen(value)+1);

      free(name);
      free(value);
    }
  }

  return x;
}
/* }}} */

/* {{{ cf_http_data_callback */
size_t cf_http_data_callback(void *buffer, size_t size, size_t nmemb, void *userp) {
  cf_http_request_t *rq = (cf_http_request_t *)userp;
  size_t x = size * nmemb; /* avoid multiple multiplication */
  cf_str_chars_append(&rq->rsp->content,buffer,x);

  return x;
}
/* }}} */

/* {{{ cf_http_complex_request */
int cf_http_complex_request(cf_http_request_t *rq) {
  if(rq->rsp == NULL)    rq->rsp = cf_alloc(NULL,1,sizeof(*rq->rsp),CF_ALLOC_CALLOC);
  if(rq->handle == NULL) rq->handle = curl_easy_init();

  rq->_donttouch = 0;
  curl_easy_setopt(rq->handle, CURLOPT_URL, rq->uri);

  if(rq->follow)          curl_easy_setopt(rq->handle, CURLOPT_FOLLOWLOCATION, 1);
  if(rq->resume)          curl_easy_setopt(rq->handle, CURLOPT_RESUME_FROM,rq->resume);
  if(rq->user_pass)       curl_easy_setopt(rq->handle, CURLOPT_USERPWD,rq->user_pass);
  if(rq->custom_headers)  curl_easy_setopt(rq->handle, CURLOPT_HTTPHEADER, rq->custom_headers);
  if(rq->referer)         curl_easy_setopt(rq->handle, CURLOPT_REFERER, rq->referer);
  if(rq->cookies)         curl_easy_setopt(rq->handle, CURLOPT_COOKIE, rq->cookies);

  if(rq->ua)              curl_easy_setopt(rq->handle, CURLOPT_USERAGENT, rq->ua);
  else                    curl_easy_setopt(rq->handle, CURLOPT_USERAGENT, "Classic Forum/" CF_VERSION);
  if(rq->data_callback)   curl_easy_setopt(rq->handle, CURLOPT_WRITEFUNCTION, rq->data_callback);
  else                    curl_easy_setopt(rq->handle, CURLOPT_WRITEFUNCTION, cf_http_data_callback);
  if(rq->header_callback) curl_easy_setopt(rq->handle, CURLOPT_HEADERFUNCTION, rq->header_callback);
  else                    curl_easy_setopt(rq->handle, CURLOPT_HEADERFUNCTION, cf_http_header_callback);

  if(rq->rqdata) {
    curl_easy_setopt(rq->handle, CURLOPT_WRITEDATA, rq->rqdata);
    curl_easy_setopt(rq->handle, CURLOPT_WRITEHEADER, rq->rqdata);
  }
  else {
    curl_easy_setopt(rq->handle, CURLOPT_WRITEDATA, rq);
    curl_easy_setopt(rq->handle, CURLOPT_WRITEHEADER, rq);
  }

  if(rq->post_data.len) {
    curl_easy_setopt(rq->handle,CURLOPT_POSTFIELDS,rq->post_data.content);
    curl_easy_setopt(rq->handle,CURLOPT_POSTFIELDSIZE,rq->post_data.len);
  }

  if(rq->proxy) {
    curl_easy_setopt(rq->handle,CURLOPT_PROXY,rq->proxy);
    if(rq->proxy_user_pass) curl_easy_setopt(rq->handle,CURLOPT_PROXYUSERPWD,rq->proxy_user_pass);
  }

  if(rq->custom_rq) curl_easy_setopt(rq->handle, CURLOPT_CUSTOMREQUEST, rq->custom_rq);
  else {
    switch(rq->type) {
      case CF_HTTP_TYPE_POST:
        curl_easy_setopt(rq->handle, CURLOPT_POST, 1);
        break;
      case CF_HTTP_TYPE_HEAD:
        curl_easy_setopt(rq->handle, CURLOPT_NOBODY, 1);
        break;
      default:
        curl_easy_setopt(rq->handle, CURLOPT_HTTPGET, 1);
        break;
    }
  }

  return curl_easy_perform(rq->handle);
}
/* }}} */

/* {{{ cf_http_simple_head_uri */
cf_http_response_t *cf_http_simple_head_uri(const u_char *uri) {
  cf_http_request_t rq;
  cf_http_response_t *rsp = cf_alloc(NULL,1,sizeof(*rsp),CF_ALLOC_CALLOC);

  memset(&rq,0,sizeof(rq));
  rq.rsp    = rsp;
  rq.uri    = (u_char *)uri;
  rq.follow = 0;
  rq.type   = CF_HTTP_TYPE_HEAD;

  cf_http_complex_request(&rq);
  curl_easy_cleanup(rq.handle);

  return rsp;
}
/* }}} */

/* {{{ cf_http_simple_get_uri */
cf_http_response_t *cf_http_simple_get_uri(const u_char *uri,time_t lm) {
  cf_http_request_t rq;
  cf_http_response_t *rsp = cf_alloc(NULL,1,sizeof(*rsp),CF_ALLOC_CALLOC);
  u_char buff[512];
  struct tm tm;

  memset(&rq,0,sizeof(rq));
  rq.rsp    = rsp;
  rq.uri    = (u_char *)uri;
  rq.follow = 1;
  rq.type   = CF_HTTP_TYPE_GET;

  if(lm) {
    setlocale(LC_TIME,"C");
    gmtime_r(&lm,&tm);
    // Last-Modified: Fri, 21 Oct 2005 12:42:36 GMT
    strftime(buff,512,"If-Modified-Since: %a, %d %b %G %H:%M:%S GMT",&tm);
    rq.custom_headers = curl_slist_append(rq.custom_headers, buff);;
  }

  rsp->headers = cf_hash_new(NULL);

  cf_http_complex_request(&rq);
  curl_easy_cleanup(rq.handle);

  return rsp;
}
/* }}} */

/* {{{ cf_http_simple_post_uri */
cf_http_response_t *cf_http_simple_post_uri(const u_char *uri,const u_char *postdata,size_t len) {
  cf_http_request_t rq;
  cf_http_response_t *rsp = cf_alloc(NULL,1,sizeof(*rsp),CF_ALLOC_CALLOC);

  memset(&rq,0,sizeof(rq));
  rq.rsp    = rsp;
  rq.uri    = (u_char *)uri;
  rq.follow = 1;
  rq.type   = CF_HTTP_TYPE_POST;

  rq.post_data.content  = (u_char *)postdata;
  rq.post_data.len      = len;
  rq.post_data.reserved = 0;

  rsp->headers = cf_hash_new(NULL);

  cf_http_complex_request(&rq);
  curl_easy_cleanup(rq.handle);

  return rsp;
}
/* }}} */

/* {{{ cf_http_destroy_response */
void cf_http_destroy_response(cf_http_response_t *rsp) {
  if(rsp->reason) free(rsp->reason);
  if(rsp->headers) cf_hash_destroy(rsp->headers);

  cf_str_cleanup(&rsp->content);
}
/* }}} */

/* {{{ cf_http_redirect_nice_uri */
void cf_http_redirect_with_nice_uri(const u_char *ruri,int perm) {
  u_char *tmp,*port,*https;
  register u_char *ptr;
  cf_string_t uri;
  int slash = 0;

  cf_str_init_growth(&uri,256);

  if(cf_strncmp(ruri,"http://",7) != 0 && cf_strncmp(ruri,"https://",8) != 0) {
    port  = getenv("SERVER_PORT");
    https = getenv("HTTPS");

    if(https || cf_strncmp(port,"443",3) == 0) cf_str_chars_append(&uri,"https://",8);
    else cf_str_chars_append(&uri,"http://",7);

    tmp = getenv("SERVER_NAME");
    cf_str_chars_append(&uri,tmp,strlen(tmp));

    if(cf_strcmp(port,"80") != 0 && cf_strcmp(port,"443") != 0) {
      cf_str_char_append(&uri,':');
      cf_str_chars_append(&uri,tmp,strlen(tmp));
    }

    cf_str_char_append(&uri,'/');
    slash = 1;

    ptr = (u_char *)ruri;
  }
  else if(cf_strncmp(ruri,"http://",7) == 0) {
    cf_str_chars_append(&uri,"http://",7);
    ptr = (u_char *)ruri + 7 * sizeof(u_char);
  }
  else {
    cf_str_chars_append(&uri,"https://",8);
    ptr = (u_char *)ruri + 8 * sizeof(u_char);
  }

  for(;*ptr;++ptr) {
    switch(*ptr) {
      case '/':
        if(slash == 0) cf_str_char_append(&uri,'/');
        slash = 1;
        break;
      default:
        slash = 0;
        cf_str_char_append(&uri,*ptr);
    }
  }

  if(perm) printf("Status: 301 Moved Permanently\015\012");
  else printf("Status: 302 Moved Temporarily\015\012");

  printf("Location: %s\015\012\015\012",uri.content);
}
/* }}} */

/* {{{ cf_urlify */
/**
 * generates a string from a  given value which can be used as an URL
 * \param val The value to create the URL string from
 * \return The URL string
 */
u_char *cf_urlify(const u_char *val) {
  cf_string_t str;
  register u_char *ptr;
  int i;
  u_int32_t num;

  cf_str_init(&str);

  for(ptr=(u_char *)val;*ptr;++ptr) {
    if(isalnum(*ptr) || *ptr == '_' || *ptr == '-') cf_str_char_append(&str,tolower(*ptr));
    else if(isspace(*ptr)) cf_str_char_append(&str,'-');
    else {
      i = utf8_to_unicode(ptr,strlen(ptr),&num);
      ptr += i - 1;

      switch(num) {
        case 0xC4:
        case 0xE4:
          cf_str_chars_append(&str,"ae",2);
          break;
        case 0xD6:
        case 0xF6:
          cf_str_chars_append(&str,"oe",2);
          break;
        case 0xDC:
        case 0xFC:
          cf_str_chars_append(&str,"ue",2);
          break;
        case 0xDF:
          cf_str_chars_append(&str,"ss",2);
          break;

        case 0xC0:
        case 0xC1:
        case 0xC2:
        case 0xC3:
        case 0xC5:
        case 0xC6:
        case 0xE0:
        case 0xE1:
        case 0xE2:
        case 0xE3:
        case 0xE5:
        case 0xE6:
        case 0x100:
        case 0x101:
        case 0x102:
        case 0x103:
        case 0x104:
        case 0x105:
          cf_str_char_append(&str,'a');
          break;

        case 0xC7:
        case 0xE7:
        case 0x106:
        case 0x107:
        case 0x108:
        case 0x109:
        case 0x10A:
        case 0x10B:
        case 0x10C:
        case 0x10D:
          cf_str_char_append(&str,'c');
          break;

        case 0xD0:
        case 0x10E:
        case 0x10F:
        case 0x110:
        case 0x111:
          cf_str_char_append(&str,'d');
          break;

        case 0xC8:
        case 0xC9:
        case 0xCA:
        case 0xCB:
        case 0xE8:
        case 0xE9:
        case 0xEA:
        case 0xEB:
        case 0x112:
        case 0x113:
        case 0x114:
        case 0x115:
        case 0x116:
        case 0x117:
        case 0x118:
        case 0x119:
        case 0x11A:
        case 0x11B:
          cf_str_char_append(&str,'e');
          break;

        case 0xCC:
        case 0xCD:
        case 0xCE:
        case 0xCF:
        case 0xEC:
        case 0xED:
        case 0xEE:
        case 0xEF:
          cf_str_char_append(&str,'i');
          break;

        case 0xD1:
        case 0xF1:
          cf_str_char_append(&str,'n');
          break;

        case 0xD2:
        case 0xD3:
        case 0xD4:
        case 0xD5:
        case 0xD7:
        case 0xD8:
        case 0xF2:
        case 0xF3:
        case 0xF4:
        case 0xF5:
        case 0xF8:
          cf_str_char_append(&str,'o');
          break;

        case 0xD9:
        case 0xDA:
        case 0xDB:
        case 0xF9:
        case 0xFA:
        case 0xFB:
          cf_str_char_append(&str,'u');
          break;

        case 0xDD:
        case 0xFD:
        case 0xFF:
          cf_str_char_append(&str,'y');
          break;

        default:
          cf_str_char_append(&str,'-');
      }
    }
  }

  return str.content;
}
/* }}} */

/* eof */
