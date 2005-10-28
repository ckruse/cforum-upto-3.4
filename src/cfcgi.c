/**
 * \file cfcgi.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 * \brief the implementation of the Classic Forum CGI library
 *
 * a small (really small!), but hopefully fast cgi-library. It can only
 * parse x-www-form/url-encoded
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

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include "hashlib.h"
#include "utils.h"
#include "cfcgi.h"
/* }}} */

/* {{{ internal function declarations */
/**
 * \defgroup cgi_privfuncs "private" CGI lib functions
 */
/*\@{*/

/**
 * This function parses a given parameter string into name-value pairs.
 * \ingroup cgi_privfuncs
 * \param hash the CGI hash
 * \param data the parameter string
 */
int _cf_cgi_parse_params(cf_hash_t *hash,u_char *data);

/**
 * \fn _cf_cgi_save_param(cf_hash_t *hash,u_char *name,int namlen,u_char *value)
 * \brief this function saves a parsed parameter to the CGI hash.
 * \ingroup cgi_privfuncs
 * \param hash    the CGI hash
 * \param name    the name of the parameter
 * \param namlen  the length of the name string
 * \param value   the value of the parameter
 */
int _cf_cgi_save_param(cf_hash_t *hash,u_char *name,size_t namlen,u_char *value,size_t vallen);

/**
 * \brief this function destroys a parameter list.
 * \ingroup cgi_privfuncs
 * \param data the given parameter list
 */
void cf_cgi_destroy_entry(void *data);


int _cf_cgi_parse_multipart(cf_hash_t *cgi,const u_char *data,size_t datalen,const u_char *boundary);
/*\@}*/
/* }}} */

/* {{{ cf_cgi_new()
 * Returns: cf_hash_t *    a hash with the cgi values in it
 * Parameters:
 *
 * this function parses the CGI paramters and returns a
 * hash with the entries in it.
 *
 */
cf_hash_t *cf_cgi_new(void) {
  u_char *clen = getenv("CONTENT_LENGTH"),*data,*ct = getenv("CONTENT_TYPE"),*boundary;
  cf_hash_t *hash;
  long len  = 0;
  int trash = 0,didone = 0;

  if(clen) len = strtol(clen,NULL,10);

  hash = cf_hash_new(cf_cgi_destroy_entry);

  if(len) {
    didone = 1;
    trash  = 1;

    data   = cf_alloc(NULL,len + 1,1,CF_ALLOC_MALLOC);

    fread(data,1,len,stdin);
    data[len] = '\0';

    if(data && *data) {
      if(ct && cf_strncmp(ct,"multipart/form-data",19) == 0) {
        /* get boundary */
        boundary = strstr(ct,"boundary");

        /* no boundary? this cannot happen, but who knows what stupid browsers out there */
        if(boundary == NULL || (boundary = strchr(boundary, '=')) == NULL) {
          cf_hash_destroy(hash);
          free(data);
          return NULL;
        }

        boundary = strdup(boundary+1);

        if(!_cf_cgi_parse_multipart(hash,data,(size_t)len,boundary)) {
          cf_hash_destroy(hash);
          free(boundary);
          free(data);
          return NULL;
        }

        free(boundary);
      }
      else {
        if(!_cf_cgi_parse_params(hash,data)) {
          cf_hash_destroy(hash);
          free(data);
          return NULL;
        }
      }
    }
  }

  if(trash) free(data);
  trash = 0;

  if((data = getenv("QUERY_STRING")) != NULL && *data) {
    if(!_cf_cgi_parse_params(hash,data)) {
      cf_hash_destroy(hash);
      return NULL;
    }

    didone = 1;
  }

  if(didone == 0) {
    cf_hash_destroy(hash);
    return NULL;
  }

  return hash;
}
/* }}} */

/* {{{ cf_cgi_parse_path_info */
void cf_cgi_parse_path_info(cf_array_t *ary) {
  u_char *data = getenv("PATH_INFO");
  register u_char *ptr = (u_char *)data+1;
  u_char *start = data,*name = NULL;

  cf_array_init(ary,sizeof(char **),NULL);

  if(!data) return;

  if(*ptr) {
    for(;*ptr;ptr++) {
      if(*ptr == '/') {
        if(start) {
          name = strndup(start+1,ptr-start-1);
          cf_array_push(ary,&name);
          start = ptr;
        }
        else {
          start = ptr;
        }
      }
    }

    if(*start != '/') {
      name = strdup(start+1);
      cf_array_push(ary,&name);
    }
  }
}
/* }}} */

/* {{{ cf_cgi_parse_path_info_nv */
cf_hash_t *cf_cgi_parse_path_info_nv(cf_hash_t *hash) {
  u_char *pi = getenv("PATH_INFO"),*start,*name,*value;
  register u_char *ptr;
  size_t len;

  start = name = value = NULL;

  if(pi) {
    if(hash == NULL) hash = cf_hash_new(cf_cgi_destroy_entry);

    for(ptr=pi;*ptr;ptr++) {
      if(*ptr == '/') {
        if(start == NULL) start = ptr;
        else {
          if(name == NULL) {
            *ptr = '\0';
            len  = ptr-start;
            name = cf_cgi_url_decode(start+1,&len);
            *ptr = '/';
          }
          else {
            *ptr  = '\0';
            len   = ptr-start;
            value = cf_cgi_url_decode(start+1,&len);
            *ptr  = '/';

            _cf_cgi_save_param(hash,name,strlen(name),value,len);

            name = value = NULL;
          }

          start = ptr;
        }
      }
    }

    if(*(ptr-1) != '/') {
      len   = ptr-start-1;
      value = cf_cgi_url_decode(start+1,&len);
      _cf_cgi_save_param(hash,name,strlen(name),value,len);
    }
  }

  return hash;
}
/* }}} */

/* {{{ cf_cgi_parse_cookies */
void cf_cgi_parse_cookies(cf_hash_t *hash) {
  u_char *cookies = getenv("HTTP_COOKIE");
  u_char *pos = cookies,*pos1 = cookies;
  u_char *name = NULL,*value = NULL;
  size_t len = 0,namlen = 0,vallen = 0;

  if(cookies) {
    while((pos = strstr(pos1,"=")) != NULL) {
      for(;*pos1 && isspace(*pos1);++pos1);

      namlen = pos - pos1;
      *pos   = 0;
      name   = cf_cgi_url_decode(pos1,&namlen);
      *pos   = '=';

      pos1 = strstr(pos,";");
      if(!pos1) break;

      vallen  = pos1 - pos;
      *pos1   = 0;
      value   = cf_cgi_url_decode(pos+1,&vallen);
      *pos1++ = ';';

      for(pos = name;*pos && isspace(*pos);++pos);

      if(!_cf_cgi_save_param(hash,name,namlen,value,vallen)) {
        free(name);
        free(value);

        fprintf(stderr,"%s[%d]: out of memory!\n",__FILE__,__LINE__);
        return;
      }
    }

    if(pos && *pos) {
      len   = strlen(pos+1);
      value = cf_cgi_url_decode(pos+1,&len);

      for(pos = name;*pos && isspace(*pos);++pos);

      if(!_cf_cgi_save_param(hash,pos,namlen,value,len)) {
        free(name);
        free(value);

        fprintf(stderr,"%s[%d]: out of memory!\n",__FILE__,__LINE__);
        return;
      }
    }
  }

}
/* }}} */

/* {{{ cf_cgi_url_decode */
u_char *cf_cgi_url_decode(const u_char *str,size_t *len) {
  u_char *ret = cf_alloc(NULL,*len + 1,1,CF_ALLOC_MALLOC); /* the new string can be as long as the old (but not longer) */
  register u_char *ptr1,*ptr2;
  u_char ptr3[3] = { '\0','\0','\0' };

  if(!ret) return NULL;

  for(ptr1=ret,ptr2=(u_char *)str;*ptr2;ptr2++,ptr1++) {
    switch(*ptr2) {
      case '+':
        *ptr1 = ' ';
        break;
      case '%':
        memcpy(ptr3,ptr2+1,2);
        *ptr1 = strtol(ptr3,NULL,16);
        ptr2 += 2;
        break;
      default:
        *ptr1 = *ptr2;
    }
  }

  *ptr1 = '\0';
  *len  = ptr1-ret;

  return ret;
}
/* }}} */

/* {{{ cf_cgi_url_encode */
u_char *cf_cgi_url_encode(const u_char *str,size_t *len) {
  long nlen = 3 * *len + 1; /* new string can max 3x longer than old string (plus 0-byte) */
  u_char *nstr = cf_alloc(NULL,nlen,1,CF_ALLOC_MALLOC);
  register u_char *ptr1,*ptr2;

  for(ptr1=(u_char *)str,ptr2=nstr;*ptr1;ptr2++,ptr1++) {
    if((*ptr1 >= 48 && *ptr1 <= 122) || (*ptr1 == '_' || *ptr1 == '.' || *ptr1 == '-')) {
      *ptr2 = *ptr1;
    }
    else {
      if(*ptr1 == ' ') {
        *ptr2 = '+';
      }
      else {
        sprintf(ptr2,"%%%02X",(int)*ptr1);
        ptr2 += 2;
      }
    }
  }

  *ptr2 = '\0';
  *len  = ptr2-nstr;

  return nstr;
}
/* }}} */

/* {{{ _cf_cgi_parse_multipart */
int _cf_cgi_parse_multipart(cf_hash_t *cgi,const u_char *data,size_t datalen,const u_char *boundary) {
  size_t blen = strlen(boundary),namlen,vallen;
  u_char
    *start_boundary = cf_alloc(NULL,1,blen+3,CF_ALLOC_MALLOC),
    *start,*name,*value;

  register u_char *ptr;

  *start_boundary = '-';
  *(start_boundary+1) = '-';
  strcpy(start_boundary+2,boundary);

  for(ptr=(u_char *)data;*ptr;++ptr) {
    if(cf_strncmp(ptr,start_boundary,blen+2) == 0) {
      ptr += blen + 2;

      if(*ptr == '-' && *(ptr+1) == '-') break;

      if(ptr >= data+datalen) {
        free(start_boundary);
        #ifdef CF_CGI_DBG
        fprintf(stderr,"[%s:%d] ptr is smaller than data+datalen (%d) \n",__FILE__,__LINE__,(int)(data+datalen));
        #endif
        return 0;
      }
      for(;*ptr && cf_strncmp(ptr,"Content-Disposition: form-data;",31) != 0;++ptr);
      if(*ptr == '\0') {
        free(start_boundary);
        #ifdef CF_CGI_DBG
        fprintf(stderr,"[%s:%d] *ptr is \\0 after Content-Disposition: form-data;\n",__FILE__,__LINE__);
        #endif
        return 0;
      }

      ptr += 31;
      for(;*ptr && isspace(*ptr);++ptr);
      if(cf_strncmp(ptr,"name=\"",6) != 0) {
        free(start_boundary);
        #ifdef CF_CGI_DBG
        fprintf(stderr,"[%s:%d] name=\" is missing (%s)\n",__FILE__,__LINE__,ptr);
        #endif
        return 0;
      }

      ptr += 6;
      start = ptr;

      for(++ptr;*ptr && *ptr != '"';++ptr);
      if(*ptr == '\0') {
        free(start_boundary);
        #ifdef CF_CGI_DBG
        fprintf(stderr,"[%s:%d] ptr is \\0 after name=\"\n",__FILE__,__LINE__);
        #endif
        return 0;
      }

      namlen = ptr - start;
      name   = strndup(start,namlen);

      /* search content; go to first \r\n */
      for(;*ptr && cf_strncmp(ptr,"\015\012\015\012",4) != 0;++ptr);
      if(*ptr == '\0') {
        free(start_boundary);
        free(name);
        #ifdef CF_CGI_DBG
        fprintf(stderr,"[%s:%d] ptr is \\0, failure!",__FILE__,__LINE__);
        #endif
        return 0;
      }

      ptr += 4;
      start = ptr;

      /* search end of value */
      for(ptr=start;*ptr && cf_strncmp(ptr,start_boundary,blen+2) != 0;++ptr);
      if(*ptr == '\0') {
        free(start_boundary);
        free(name);
        #ifdef CF_CGI_DBG
        fprintf(stderr,"[%s:%d] ptr is \\0 after value!\n",__FILE__,__LINE__);
        #endif
        return 0;
      }

      vallen = ptr - start - 2;
      value = strndup(start,vallen);

      _cf_cgi_save_param(cgi,name,namlen,value,vallen);

      free(name);
      free(value);

      --ptr;
    }
  }

  free(start_boundary);

  return 1;
}
/* }}} */

/* {{{ _cf_cgi_save_param */
int _cf_cgi_save_param(cf_hash_t *hash,u_char *name,size_t namlen,u_char *value,size_t vallen) {
  cf_cgi_param_t *ent,*ent1;

  namlen = strlen(name);

  ent = cf_hash_get(hash,name,namlen);
  if(!ent) {
    ent        = cf_alloc(NULL,1,sizeof(cf_cgi_param_t),CF_ALLOC_CALLOC);

    ent->name  = name;

    cf_str_init_growth(&ent->value,vallen+3);
    cf_str_char_set(&ent->value,value,vallen);

    cf_hash_set(hash,name,namlen,ent,sizeof(cf_cgi_param_t));

    /* we make a copy, so we do not need it again */
    free(ent);

    ent = cf_hash_get(hash,name,strlen(name));
  }
  else {
    ent1            = cf_alloc(NULL,1,sizeof(cf_cgi_param_t),CF_ALLOC_CALLOC);

    ent1->name  = name;

    cf_str_init_growth(&ent->value,vallen+3);
    cf_str_char_set(&ent->value,value,vallen);

    if(ent->last) ent->last->next = ent1;
    else ent->next       = ent1;

    ent->last         = ent1;
  }

  return 1;
}
/* }}} */

/* {{{ _cf_cgi_parse_params */
int _cf_cgi_parse_params(cf_hash_t *hash,u_char *data) {
  u_char  *pos = data,*pos1 = data;
  u_char *name = NULL,*value = NULL;
  size_t namlen = 0,vallen = 0;

  while((pos = strstr(pos1,"=")) != NULL) {
    namlen = pos - pos1;
    *pos   = 0;
    name   = cf_cgi_url_decode(pos1,&namlen);
    *pos   = '=';

    pos1 = strstr(pos,"&");
    if(!pos1) break;

    vallen  = pos1 - pos;
    *pos1   = 0;
    value   = cf_cgi_url_decode(pos+1,&vallen);
    *pos1++ = '&';

    if(!_cf_cgi_save_param(hash,name,namlen,value,vallen)) {
      free(name);
      free(value);

      fprintf(stderr,"%s[%d]: out of memory!\n",__FILE__,__LINE__);
      return 0;
    }
  }

  if(pos && *pos) {
    vallen= strlen(pos+1);
    value = cf_cgi_url_decode(pos+1,&vallen);

    if(!_cf_cgi_save_param(hash,name,namlen,value,vallen)) {
      free(name);
      free(value);

      fprintf(stderr,"%s[%d]: out of memory!\n",__FILE__,__LINE__);
      return 0;
    }
  }

  return 1;
}
/* }}} */

/* {{{ cf_cgi_destroy_entry */
void cf_cgi_destroy_entry(void *data) {
  cf_cgi_param_t *ent = (cf_cgi_param_t *)data,*ent1;

  for(;ent;ent=ent1) {
    free(ent->name);
    cf_str_cleanup(&ent->value);

    ent1 = ent->next;

    if(ent != data) free(ent);
  }
}
/* }}} */

/* {{{ cf_cgi_get */
cf_string_t *cf_cgi_get(cf_hash_t *cgi,const u_char *name) {
  cf_cgi_param_t *p = cf_hash_get(cgi,(u_char *)name,strlen(name));

  if(p) return &p->value;
  return NULL;
}
/* }}} */

/* {{{ cf_cgi_set */
void cf_cgi_set(cf_hash_t *hash,const u_char *name,const u_char *value,size_t vallen) {
  size_t nlen = strlen(name);
  cf_cgi_param_t *p = cf_hash_get(hash,(u_char *)name,nlen),*p1,*p2;
  cf_cgi_param_t par;

  if(p) {
    if(p->next) {
      for(p1=p->next;p1;p1=p2) {
        free(p1->name);
        cf_str_cleanup(&p1->value);
        p2 = p1->next;
        free(p1);
      }
    }

    cf_str_char_set(&p->value,value,vallen);
  }
  else {
    par.name  = strdup(name);

    cf_str_init_growth(&par.value,vallen+3);
    cf_str_char_set(&par.value,value,vallen);

    par.next  = NULL;
    par.last  = NULL;

    cf_hash_set(hash,(u_char *)name,nlen,&par,sizeof(par));
  }
}
/* }}} */

/* {{{ cf_cgi_get_multiple */
cf_cgi_param_t *cf_cgi_get_multiple(cf_hash_t *hash,const u_char *param) {
  return (cf_cgi_param_t *)cf_hash_get(hash,(u_char *)param,strlen(param));
}
/* }}} */

/* {{{ cf_cgi_destroy */
void cf_cgi_destroy(cf_hash_t *hash) {
  cf_hash_destroy(hash);
}
/* }}} */

/* {{{ cf_cgi_path_info_parsed */
u_int32_t cf_cgi_path_info_parsed(u_char ***infos) {
  u_char *path = getenv("PATH_INFO"),*prev,**list = NULL;
  register u_char *ptr;
  int len = 0;
  size_t len1;

  if(path) {
    for(prev=ptr=path+1;*ptr;ptr++) {
      if(*ptr == '/') {
        *ptr        = '\0';
        list        = cf_alloc(list,++len,sizeof(char **),CF_ALLOC_REALLOC);
        len1        = ptr-prev;
        list[len-1] = cf_cgi_url_decode(prev,&len1);
        prev        = ptr+1;
        *ptr        = '/';
      }
    }
  }

  *infos = list;
  return len;
}
/* }}} */

/* eof */
