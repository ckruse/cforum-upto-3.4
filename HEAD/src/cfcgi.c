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

#include "hashlib.h"
#include "utils.h"
#include "cfcgi.h"
/* }}} */

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
int _cf_cgi_parse_params(t_cf_hash *hash,u_char *data);

/**
 * \fn _cf_cgi_save_param(t_cf_hash *hash,u_char *name,int namlen,u_char *value)
 * \brief this function saves a parsed parameter to the CGI hash.
 * \ingroup cgi_privfuncs
 * \param hash    the CGI hash
 * \param name    the name of the parameter
 * \param namlen  the length of the name string
 * \param value   the value of the parameter
 */
int _cf_cgi_save_param(t_cf_hash *hash,u_char *name,int namlen,u_char *value);

/**
 * \brief this function destroys a parameter list.
 * \ingroup cgi_privfuncs
 * \param data the given parameter list
 */
void cf_cgi_destroy_entry(void *data);
/*\@}*/

/*
 * Returns: t_cf_hash *    a hash with the cgi values in it
 * Parameters:
 *
 * this function parses the CGI paramters and returns a
 * hash with the entries in it.
 *
 */
t_cf_hash *cf_cgi_new() {
  u_char *clen = getenv("CONTENT_LENGTH"),*rqmeth = getenv("REQUEST_METHOD"),*data;
  t_cf_hash *hash;
  long len  = 0;
  int trash = 0;

  if(!rqmeth) return NULL;
  if(clen) len = strtol(clen,NULL,10);

  hash = cf_hash_new(cf_cgi_destroy_entry);

  if(strcmp(rqmeth,"GET") == 0) {
    data = getenv("QUERY_STRING");
  }
  else {
    if(!len) {
      cf_hash_destroy(hash);
      return NULL;
    }

    trash = 1;
    data  = fo_alloc(NULL,len + 1,1,FO_ALLOC_MALLOC);

    fread(data,1,len,stdin);
    data[len] = '\0';
  }

  if(data && *data) {
    if(!_cf_cgi_parse_params(hash,data)) {
      cf_hash_destroy(hash);
      if(trash) free(data);
      return NULL;
    }

    return hash;
  }
  else {
    cf_hash_destroy(hash);
  }

  return NULL;
}

void cf_cgi_parse_path_info(t_array *ary) {
  u_char *data = getenv("PATH_INFO");
  register u_char *ptr = (u_char *)data+1;
  u_char *start = data,*name = NULL;

  array_init(ary,sizeof(char **),NULL);

  if(!data) return;

  if(*ptr) {
    for(;*ptr;ptr++) {
      if(*ptr == '/') {
        if(start) {
          name = strndup(start+1,ptr-start);
          array_push(ary,&name);
          start = ptr;
        }
        else {
          start = ptr;
        }
      }
    }

    name = strdup(start+1);
    array_push(ary,&name);
  }
}

/*
 * Returns: u_char *         the url decoded string
 * Parameters:
 *   - const u_char *str     the string to decode
 *   - long len            the length of the string
 *
 * This function decodes a url-encoded string
 *
 */
u_char *cf_cgi_url_decode(const u_char *str,size_t len) {
  u_char *ret = fo_alloc(NULL,len + 1,1,FO_ALLOC_MALLOC); /* the new string can be as long as the old (but not longer) */
  register u_char *ptr1,*ptr2;
  u_char ptr3[2];

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
  return ret;
}

/*
 * Returns: u_char *         the url-encoded string
 * Parameters:
 *   - const u_char *str     the string to encode
 *   - long len            the length of the string
 *
 * This function url-encodes a string
 *
 */
u_char *cf_cgi_url_encode(const u_char *str,size_t len) {
  long nlen = 3 * len + 1; /* new string can max 3x longer than old string (plus 0-byte) */
  u_char *nstr = fo_alloc(NULL,nlen,1,FO_ALLOC_MALLOC);
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
  nstr  = fo_alloc(nstr,ptr2-nstr,1,FO_ALLOC_REALLOC);

  return nstr;
}

/*
 * Returns: int            0 on failure, 1 on success
 * Parameters:
 *   - t_cf_hash *hash     the cgi-hash
 *   - u_char *name         the name of the field
 *   - int namlen          the length of the name-field
 *   - u_char *value        the value of the field
 *
 * This function saves a cgi-parameter to the hash
 *
 */
int _cf_cgi_save_param(t_cf_hash *hash,u_char *name,int namlen,u_char *value) {
  t_cf_cgi_param *ent,*ent1;

  ent = cf_hash_get(hash,name,namlen);
  if(!ent) {
    ent        = fo_alloc(NULL,1,sizeof(t_cf_cgi_param),FO_ALLOC_CALLOC);

    ent->name  = name;
    ent->value = value;

    cf_hash_set(hash,name,namlen,ent,sizeof(t_cf_cgi_param));

    /* we make a copy, so we do not need it again */
    free(ent);
  }
  else {
    ent1            = fo_alloc(NULL,1,sizeof(t_cf_cgi_param),FO_ALLOC_CALLOC);

    ent1->name  = name;
    ent1->value = value;

    if(ent->last)
      ent->last->next = ent1;
    else
      ent->next       = ent1;

    ent->last         = ent1;
  }

  return 1;
}

/*
 * Returns: int            0 on failure, 1 on success
 * Parameters:
 *   - t_cf_hash *hash     the cgi-hash
 *   - u_char *data         the url-encoded data-string
 *
 * This function parses an url-encoded data-string
 *
 */
int _cf_cgi_parse_params(t_cf_hash *hash,u_char *data) {
  u_char  *pos = data,*pos1 = data;
  u_char *name = NULL,*value = NULL;
  int len = 0,namlen = 0,vallen = 0;

  while((pos = strstr(pos1,"=")) != NULL) {
    namlen = pos - pos1;
    *pos   = 0;
    name   = cf_cgi_url_decode(pos1,namlen);
    *pos   = '=';

    pos1 = strstr(pos,"&");
    if(!pos1) break;

    vallen  = pos1 - pos;
    *pos1   = 0;
    value   = cf_cgi_url_decode(pos+1,vallen);
    *pos1++ = '&';

    if(!_cf_cgi_save_param(hash,name,namlen,value)) {
      free(name);
      free(value);

      fprintf(stderr,"%s[%d]: out of memory!\n",__FILE__,__LINE__);
      return 0;
    }
  }

  if(pos && *pos) {
    len   = strlen(pos+1);
    value = cf_cgi_url_decode(pos+1,len);

    if(!_cf_cgi_save_param(hash,name,namlen,value)) {
      free(name);
      free(value);

      fprintf(stderr,"%s[%d]: out of memory!\n",__FILE__,__LINE__);
      return 0;
    }
  }

  return 1;
}

/*
 * Returns:                nothing
 * Parameters:
 *   - void *data          the hash-entry
 *
 * This function destroys a hash-entry
 *
 */
void cf_cgi_destroy_entry(void *data) {
  t_cf_cgi_param *ent = (t_cf_cgi_param *)data,*ent1;

  for(;ent;ent=ent1) {
    free(ent->name);
    free(ent->value);

    ent1 = ent->next;

    if(ent != data) free(ent);
  }
}

/*
 * Returns:                the value of the field
 * Parameters:
 *   - t_cf_hash *hash     the cgi-hash
 *   - const u_char *name   the name of the field
 *
 * this function gets a single value
 *
 */
u_char *cf_cgi_get(t_cf_hash *hash,u_char *name) {
  t_cf_cgi_param *p = cf_hash_get(hash,name,strlen(name));

  if(p) {
    return p->value;
  }

  return NULL;
}

void cf_cgi_set(t_cf_hash *hash,const u_char *name,const u_char *value) {
  size_t nlen = strlen(name);
  t_cf_cgi_param *p = cf_hash_get(hash,(u_char *)name,nlen),*p1,*p2;
  t_cf_cgi_param par;

  if(p) {
    if(p->next) {
      for(p1=p->next;p1;p1=p2) {
        free(p1->name);
        free(p1->value);
        p2 = p1->next;
        free(p1);
      }
    }

    free(p->value);
    p->value = strdup(value);
  }
  else {
    par.name  = strdup(name);
    par.value = strdup(value);
    par.next  = NULL;
    par.last  = NULL;

    cf_hash_set(hash,(u_char *)name,nlen,&par,sizeof(par));
  }
}

/*
 * Returns:                 the value of the field
 * Parameters:
 *   - t_cf_hash *hash      the CGI hash
 *   - const u_char *param   the name of the parameter
 *
 * this function returns a multiple value list
 */
t_cf_cgi_param *cf_cgi_get_multiple(t_cf_hash *hash,u_char *param) {
  return (t_cf_cgi_param *)cf_hash_get(hash,param,strlen(param));
}

/*
 * Returns:                 nothing
 * Parameters:
 *   - t_cf_hash *hash      the CGI hash
 *
 * this function destroys a CGI hash
 */
void cf_cgi_destroy(t_cf_hash *hash) {
  cf_hash_destroy(hash);
}

/**
 * This function parses a PATH_INFO string
 */
u_int32_t path_info_parsed(u_char ***infos) {
  u_char *path = getenv("PATH_INFO"),*prev,**list = NULL;
  register u_char *ptr;
  int len = 0;

  if(path) {
    for(prev=ptr=path+1;*ptr;ptr++) {
      if(*ptr == '/') {
        *ptr        = '\0';
        list        = fo_alloc(list,++len,sizeof(char **),FO_ALLOC_REALLOC);
        list[len-1] = cf_cgi_url_decode(prev,ptr-prev);
        prev        = ptr+1;
        *ptr        = '/';
      }
    }
  }

  *infos = list;
  return len;
}

/* eof */
