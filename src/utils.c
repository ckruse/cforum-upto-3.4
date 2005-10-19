/**
 * \file utils.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 * \brief utilities for the Classic Forum
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

#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>

#include <limits.h>

#include "charconvert.h"
#include "utils.h"
/* }}} */

/* {{{ cf_transform_date
 * Returns: time_t          the timestamp or (time_t)0
 * Parameters:
 *   - const u_char *datestr the date string
 *
 * This function tries to create a timestamp from dd.mm.yyyy[ hh:mm[:ss]]
 *
 */
time_t cf_transform_date(const u_char *datestr) {
  struct tm t;
  u_char *ptr,*before;                     /* two pointers to work with */
  u_char *str = cf_alloc(NULL,strlen(datestr)+1,1,CF_ALLOC_MALLOC); /* we need a copy of the string (we cannot change a const u_char *) */

  (void)strcpy(str,datestr);
  ptr = before = str;

  memset(&t,0,sizeof(t));        /* initialize the struct tm (set everything to 0) */

  for(;*ptr && *ptr != '.';ptr++);       /* search the first . (for the day) */
  if(*ptr == '.') {                      /* if this is not a dot, we have no valid date */
    *ptr = '\0';                         /* setting this byte to 0, we can use atoi() whithout creating a substring */
    t.tm_mday = atoi(before);            /* fill the mont day (1-31) */
    *ptr = '.';
  }
  else {
    free(str);
    return (time_t)0;                    /* no valid date */
  }

  for(before= ++ptr;*ptr && *ptr != '.';ptr++);        /* search the next dot (for the month) */
  if(*ptr == '.') {
    *ptr = '\0';
    t.tm_mon = atoi(before)-1;                         /* tm_mon contains the mont - 1 (0-11) */
    *ptr = '.';
  }
  else {
    free(str);
    return (time_t)0;                                  /* no valid date */
  }

  for(before= ++ptr;*ptr && !isspace(*ptr);ptr++);     /* search the '\0' or a whitespace; if a whitespace
                                                        * follows, there are also hours and mins and perhaps seconds */
  if(isspace(*ptr) || *ptr == '\0') {                  /* Is this a a valid entry? */
    t.tm_year = atoi(before) - 1900;                   /* tm_year contains the year - 1900 */
  }
  else {
    free(str);
    return (time_t)0;                                  /* not a valid entry */
  }

  if(*ptr == ' ') {                                    /* follows an hour and a minute? */
    for(;*ptr && *ptr == ' ';ptr++);                   /* skip trailing whitespaces */

    if(*ptr) {                                         /* have we got a string like "1.1.2001 "? */
      for(before= ++ptr;*ptr && *ptr != ':';ptr++);    /* search the next colon (Hours are seperated from minutes by
                    * colons
              */
      if(*ptr == ':') {
        *ptr = '\0';
        t.tm_hour = atoi(before);                      /* get the hour */
        *ptr = ':';
      }
      else {
        free(str);
        return (time_t)0;
      }

      for(before= ++ptr;*ptr && *ptr != ':';ptr++);    /* search for the end of the string or another colon */
      if(*ptr == ':' || *ptr == '\0') {
        if(*ptr == ':') {
          *ptr = '\0';
          t.tm_min = atoi(before);                       /* get the minutes */
          *ptr = ':';
        }
        else {
          t.tm_min = atoi(before);
        }
      }
      else {
        free(str);
        return (time_t)0;
      }

      if(*ptr == ':') {                                /* seconds following */
        before= ptr + 1;                               /* after the seconds, there can only follow the end of
                                                        * the string
                                                        */
        if(!*ptr) return (time_t)0;
        t.tm_sec = atoi(before);
      }
    }
  }

  free(str);
  return mktime(&t);                                   /* finally, try to generate the timestamp */
}
/* }}} */

/* {{{ cf_gen_unid */
int cf_gen_unid(u_char *buff,int maxlen) {
  int i,l;
  u_char *remaddr = getenv("REMOTE_ADDR");
  static const u_char chars[] = "AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz0123456789_-";
  struct timeval  tp;

  if(!remaddr) remaddr = "127.0.0.1";
  if(gettimeofday(&tp,NULL) != 0) return 0;

  srand(tp.tv_usec);
  l = strlen(remaddr);

  if(maxlen <= l) l = maxlen - 1;

  for(i=0;i<l;++i) buff[i] = chars[remaddr[i] % 59 + (unsigned int)(rand() % 3)];

  l = rand() % maxlen;
  if(l == maxlen) --l;
  for(;i<l;++i) buff[i] = chars[rand() % 63];

  buff[i] = '\0';

  return i - 1;
}
/* }}} */

/* {{{ cf_remove_recursive */
int cf_remove_recursive(const u_char *path) {
  DIR *dir;
  struct dirent *ent;
  char buff[PATH_MAX];
  struct stat st;

  if(lstat(path, &st) < 0) {
    fprintf(stderr,"utils: cf_remove_recursive: could not lstat '%s': %s\n",path,strerror(errno));
    return -1;
  }

  if(S_ISDIR(st.st_mode)) {
    if((dir = opendir(path)) == NULL) {
      fprintf(stderr,"utils: cf_remove_recursive: could not opendir '%s': %s\n",path,strerror(errno));
      return -1;
    }

    while((ent = readdir(dir)) != NULL) {
      /* take care of . and .. */
      if(cf_strcmp(ent->d_name,".") == 0 || cf_strcmp(ent->d_name,"..") == 0) continue;

      /* Recursively call to remove the current entry */
      snprintf(buff,PATH_MAX, "%s/%s",path,ent->d_name);
      if(cf_remove_recursive(buff) != 0) return -1;
    }

    if(rmdir(path) < 0) {
      fprintf(stderr,"utils: cf_remove_recursive: could not rmdir '%s': %s\n",path,strerror(errno));
      return -1;
    }
  }
  else {
    /* path is not a directory; just unlink() it */
    if(unlink(path) < 0) {
      fprintf(stderr,"utils: cf_remove_recursive: could not unlink '%s': %s\n",path,strerror(errno));
      return -1;
    }
  }

  return 0;
}
/* }}} */

/* {{{ cf_make_path */
int cf_make_path(const u_char *path,mode_t mode) {
  u_char *mpath = strdup(path);
  register u_char *ptr;

  for(ptr=mpath+1;*ptr;++ptr) {
    if(*ptr == '/') {
      *ptr = '\0';

      if(mkdir(mpath,mode) != 0) {
        if(errno != EEXIST) return -1;
      }

      *ptr = '/';
    }
  }

  if(*(ptr-1) != '/') {
    if(mkdir(mpath,mode) != 0) {
      if(errno != EEXIST) return -1;
    }
  }

  return 0;
}
/* }}} */

#ifdef HAS_NO_GETLINE
/* {{{ getline */
ssize_t getline(char **lineptr,size_t *n,FILE *stream) {
  return getdelim(lineptr,n,'\n',stream);
}
/* }}} */
#endif

#ifdef HAS_NO_GETDELIM
/* {{{ getdelim
 * Returns: ssize_t         The number of bytes read or -1 on failure
 * Parameters:
 *   - u_char **lineptr      The line pointer
 *   - size_t *n            The number of bytes allocated
 *   - int delim            The delimiter character
 *   - FILE *stream         The stream pointer
 *
 * This function reads until 'delim' has been found
 *
 */
ssize_t getdelim(char **lineptr,size_t *n,int delim,FILE *stream) {
  cf_string_t buf;
  register u_char c;

  cf_str_init(&buf);

  while(!feof(stream) && !ferror(stream)) {
    c = fgetc(stream);
    cf_str_char_append(&buf,c);

    if(c == delim) break;
  }

  if(*lineptr) free(*lineptr);

  *lineptr = buf.content;
  *n       = buf.len;

  if(feof(stream) || ferror(stream)) return -1;

  return buf.len;
}
/* }}} */
#endif

/* {{{ cf_timegm, from timegm(3) of glibc */
time_t cf_timegm(struct tm *tm) {
  time_t ret;
  char *tz;

  tz = getenv("TZ");
  setenv("TZ", "", 1);
  tzset();
  ret = mktime(tm);
  if(tz) setenv("TZ", tz, 1);
  else unsetenv("TZ");
  tzset();
  return ret;
}
/* }}} */

/* eof */
