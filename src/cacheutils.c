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
void cf_cache_genname(u_char *base,u_char *uri,t_string *fname) {
  str_init(fname);
  str_char_set(fname,base,strlen(base));
  str_chars_append(fname,uri,strlen(uri));

  if((fname->content+fname->len) == '/') str_chars_append(fname,"idx",3);
}
/* }}} */

/* {{{ cf_cache_outdated */
int cf_cache_outdated(const u_char *base,const u_char *uri,const u_char *file) {
  struct stat st1,st2;
  t_string fname;

  cf_cache_genname(base,uri,&fname);

  if(stat(fname.content,&st1) == -1) {
    str_cleanup(&fname);
    return -1;
  }

  str_cleanup(&fname);

  if(stat(file,&st2) == -1) return -1;
  if(st1.st_mtime >= st2.st_mtime) return 0;

  return -1;
}
/* }}} */

/* {{{ cf_cache_outdated_date */
int cf_cache_outdated_date(const u_char *base,const u_char *uri,time_t date) {
  struct stat st1;
  t_string fname;

  cf_cache_genname(base,uri,&fname);

  if(stat(fname.content,&st1) == -1) {
    str_cleanup(&fname);
    return -1;
  }

  str_cleanup(&fname);

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
int cf_cache_create_path(const u_char *path) {
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
int cf_cache(const u_char *base,const u_char *uri,const u_char *content,size_t len) {
  FILE *fd;
  t_string fname;

  cf_cache_genname(base,uri,&fname);

  if(cf_cache_create_path(fname.content) == 0) {
    if((fd = fopen(fname.content,"w")) != NULL) {
      fwrite(content,1,len,fd);
      fclose(fd);
      str_cleanup(&fname);
      return 0;
    }
  }

  str_cleanup(&fname);
  return -1;
}
/* }}} */

/* {{{ cf_get_cache */
t_cache_entry *cf_get_cache(u_char *base,u_char *uri) {
  int fd;
  t_string fname;
  void *ptr;
  t_cache_entry *ent;
  struct stat st;

  cf_cache_genname(base,uri,&fname);

  if(stat(fname.content,&st) == -1) {
    str_cleanup(&fname);
    return NULL;
  }

  if((fd = open(fname.content,O_RDONLY)) == -1) {
    str_cleanup(&fname);
    return NULL;
  }

  if((caddr_t)(ptr = mmap(0,st.st_size,PROT_READ,MAP_FILE|MAP_SHARED,fd,0)) == (caddr_t)-1) {
    close(fd);
    str_cleanup(&fname);
    return NULL;
  }

  ent = fo_alloc(NULL,1,sizeof(*ent),FO_ALLOC_MALLOC);
  ent->fd   = fd;
  ent->ptr  = ptr;
  ent->size = st.st_size;

  return ent;
}
/* }}} */

/* {{{ cf_cache_destroy */
void cf_cache_destroy(t_cache_entry *ent) {
  munmap(ent->ptr,ent->size);
  close(ent->fd);
  free(ent);
}
/* }}} */

/* eof */
