/**
 * \file cacheutils.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 * \brief cache utilities for the Classic Forum
 *
 * These utilities are written for the Classic Forum. Hope, they're useful.
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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <sys/mman.h>
#include <fcntl.h>

#include <zlib.h>

#include "charconvert.h"
#include "utils.h"
/* }}} */

/* {{{ cf_cache_genname */
/**
 * Internal function to generate a cache entry name
 * \param base The base cache directory
 * \param uri The URI of the entry
 * \param fname The target string
 */
void cf_cache_genname(const u_char *base,const u_char *uri,cf_string_t *fname) {
  cf_str_init(fname);
  cf_str_char_set(fname,base,strlen(base));

  if(*(fname->content+fname->len-1) != '/') cf_str_char_append(fname,'/');
  cf_str_chars_append(fname,uri,strlen(uri));
  if(*(fname->content+fname->len-1) == '/') cf_str_chars_append(fname,"idx",3);
}
/* }}} */

/* {{{ cf_cache_outdated */
int cf_cache_outdated(const u_char *base,const u_char *uri,const u_char *file) {
  struct stat st1,st2;
  cf_string_t fname;

  cf_cache_genname(base,uri,&fname);

  if(stat(fname.content,&st1) == -1) {
    cf_str_cleanup(&fname);
    return -1;
  }

  cf_str_cleanup(&fname);

  if(stat(file,&st2) == -1) return -1;
  if(st1.st_mtime >= st2.st_mtime) return 0;

  return -1;
}
/* }}} */

/* {{{ cf_cache_outdated_date */
int cf_cache_outdated_date(const u_char *base,const u_char *uri,time_t date) {
  struct stat st1;
  cf_string_t fname;

  cf_cache_genname(base,uri,&fname);

  if(stat(fname.content,&st1) == -1) {
    cf_str_cleanup(&fname);
    return -1;
  }

  cf_str_cleanup(&fname);

  if(st1.st_mtime >= date) return 0;

  return -1;
}
/* }}} */

/* {{{ cf_cache_create_path */
/**
 * Internal function to create a path to a cache entry
 * \param path The path
 * \return -1 on error, 0 on success
 */
int cf_cache_create_path(u_char *path) {
  register u_char *ptr = path+1;
  int ret;

  /*
   * search whole string for a directory separator
   */
  for (;*ptr;ptr++) {
    /*
     * when a directory separator is given, create path 'till there
     */
    if (*ptr == '/') {
      *ptr = '\0';
      ret = mkdir(path,S_IRWXU|S_IRWXG|S_IRWXO);
      *ptr = '/';

      if (ret && errno != EEXIST) return -1;
    }
  }

  return 0;
}
/* }}} */

/* {{{ cf_cache */
int cf_cache(const u_char *base,const u_char *uri,const u_char *content,size_t len,int gzip) {
  FILE *fd;
  gzFile gzfd;
  cf_string_t fname;
  char buff[5] = "wb";

  cf_cache_genname(base,uri,&fname);

  if(cf_cache_create_path(fname.content) == 0) {
    if(gzip) {
      buff[3] = gzip + '0';
      buff[4] = '\0';

      if((gzfd = gzopen(fname.content,buff)) != NULL) {
        gzwrite(gzfd,(void *)content,len);
        gzclose(gzfd);
        cf_str_cleanup(&fname);
        return 0;
      }
    }
    else {
      if((fd = fopen(fname.content,"w")) != NULL) {
        fwrite(content,1,len,fd);
        fclose(fd);
        cf_str_cleanup(&fname);
        return 0;
      }
    }
  }

  cf_str_cleanup(&fname);
  return -1;
}
/* }}} */

/* {{{ cf_get_cache */
cf_cache_entry_t *cf_get_cache(u_char *base,u_char *uri,int gzip) {
  int fd;
  cf_string_t fname;
  void *ptr;
  cf_cache_entry_t *ent = NULL;
  struct stat st;
  gzFile gzfd;
  char buff[BUFSIZ];
  int status;
  cf_string_t tmp;

  cf_cache_genname(base,uri,&fname);

  if(stat(fname.content,&st) == -1) {
    cf_str_cleanup(&fname);
    return NULL;
  }

  if(gzip) {
    if((gzfd = gzopen(fname.content,"rb")) != NULL) {
      cf_str_init(&tmp);

      while((status = gzread(gzfd,buff,BUFSIZ)) > 0) {
        cf_str_chars_append(&tmp,buff,status);
      }

      gzclose(gzfd);

      if(status != -1) {
        ent = cf_alloc(NULL,1,sizeof(*ent),CF_ALLOC_MALLOC);
        ent->fd   = -1;
        ent->ptr  = tmp.content;
        ent->size = tmp.len;
      }
      else cf_str_cleanup(&tmp);
    }
  }
  else {
    if((fd = open(fname.content,O_RDONLY)) == -1) {
      cf_str_cleanup(&fname);
      return NULL;
    }

    if((caddr_t)(ptr = mmap(0,st.st_size,PROT_READ,MAP_FILE|MAP_SHARED,fd,0)) == (caddr_t)-1) {
      close(fd);
      cf_str_cleanup(&fname);
      return NULL;
    }

    ent = cf_alloc(NULL,1,sizeof(*ent),CF_ALLOC_MALLOC);
    ent->fd   = fd;
    ent->ptr  = ptr;
    ent->size = st.st_size;
  }

  return ent;
}
/* }}} */

/* {{{ cf_cache_destroy */
void cf_cache_destroy(cf_cache_entry_t *ent) {
  if(ent->fd != -1) {
    munmap(ent->ptr,ent->size);
    close(ent->fd);
  }
  else free(ent->ptr);

  free(ent);
}
/* }}} */

/* eof */
