/**
 * \file fo_tid_index.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 * \brief This program creates a tid index
 *
 * This file creates an mid index to ensure access to messages in the
 * archive by tid
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
#include <dlfcn.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <dirent.h>

/* socket includes */
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/un.h>

#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "cfcgi.h"
#include "template.h"
#include "readline.h"
#include "clientlib.h"
#include "fo_tid_index.h"
/* }}} */

/** Array containig the index entries */
t_array idx;

/* {{{ is_digit */
/**
 * function checking if a directory entry consists of numbers
 * \param ent The directory entry
 * \return -1 if it consits not only of numbers, 0 if it does
 */
int is_digit(struct dirent *ent) {
  register char *ptr = ent->d_name;

  for(;*ptr;ptr++) {
    if(!isdigit(*ptr)) return -1;
  }

  return 0;
}
/* }}} */

/* {{{ is_thread */
/**
 * Function checking if a string describes a thread file
 * \param path The path to check
 */
int is_thread(const char *path) {
  register char *ptr = (char *)path;
  register int dg = 0;

  if(*ptr++ != 't') return -1;

  for(;*ptr && isdigit(*ptr);ptr++) {
    dg = 1;
  }

  if(cf_strcmp(ptr,".xml") != 0) return -1;
  if(dg == 0) return -1;

  return 0;
}
/* }}} */

/* {{{ index_month */
/**
 * Function to create the index for a month
 * \param year The year
 * \param month The month
 */
void index_month(char *year,char *month) {
  t_name_value *apath = cfg_get_first_value(&fo_default_conf,"ArchivePath");
  char path[256],path1[256];
  u_int64_t min = (u_int64_t)-1,max = 0,tmp;
  t_tid_index midx;
  struct stat st;

  DIR *m;
  struct dirent *ent;

  (void)snprintf(path,256,"%s/%s/%s",apath->values[0],year,month);
  (void)snprintf(path1,256,"%s/%s/%s/.leave",apath->values[0],year,month);

  if(stat(path1,&st) == 0) return;

  if((m = opendir(path)) == NULL) {
    fprintf(stderr,"opendir(%s): %s\n",path,strerror(errno));
    exit(EXIT_FAILURE);
  }

  while((ent = readdir(m)) != NULL) {
    if(is_thread(ent->d_name) == -1) continue;

    tmp = strtoull(ent->d_name+1,NULL,10);
    if(tmp > max) max = tmp;
    if(tmp < min) min = tmp;
  }

  closedir(m);

  midx.year  = atoi(year);
  midx.month = atoi(month);
  midx.start = min;
  midx.end   = max;

  array_push(&idx,&midx);
}
/* }}} */

/* {{{ do_year */
/**
 * Function for indexing a complete year
 * \param year The year
 */
void do_year(char *year) {
  t_name_value *apath = cfg_get_first_value(&fo_default_conf,"ArchivePath");
  char path[256];

  DIR *months;
  struct dirent *ent;

  (void)snprintf(path,256,"%s/%s",apath->values[0],year);

  if((months = opendir(path)) == NULL) {
    fprintf(stderr,"opendir(%s): %s\n",path,strerror(errno));
    exit(EXIT_FAILURE);
  }

  while((ent = readdir(months)) != NULL) {
    if(is_digit(ent) == -1) continue;
    index_month(year,ent->d_name);
  }

  closedir(months);

}
/* }}} */

/* {{{ cmp */
#ifndef DOXYGEN
int cmp(const void *elem1,const void *elem2) {
  t_tid_index *id1 = (t_tid_index *)elem1;
  t_tid_index *id2 = (t_tid_index *)elem2;

  if(id1->start > id2->start) return 1;
  if(id1->start < id2->start) return -1;

  return 0;
}
#endif
/* }}} */

/* {{{ main */
/**
 * Main function
 * \param argc Argument count
 * \param argv Argument vector
 * \param envp Environment vector
 */
int main(int argc,char *argv[],char *envp[]) {
  t_array *cfgfiles;
  u_char *file;
  t_configfile dconf;
  t_name_value *ent,*idxfile;
  FILE *fd;

  DIR *years;
  struct dirent *year;

  static const u_char *wanted[] = {
    "fo_default"
  };

  cfg_init();

  if((cfgfiles = get_conf_file(wanted,1)) == NULL) {
    fprintf(stderr,"error getting config files\n");
    return EXIT_FAILURE;
  }

  file = *((u_char **)array_element_at(cfgfiles,0));
  cfg_init_file(&dconf,file);
  cfg_register_options(&dconf,default_options);
  free(file);

  if(read_config(&dconf,NULL) != 0) {
    fprintf(stderr,"config file error!\n");

    cfg_cleanup_file(&dconf);

    return EXIT_FAILURE;
  }

  array_init(&idx,sizeof(t_tid_index),NULL);

  if((ent = cfg_get_first_value(&fo_default_conf,"ArchivePath")) == NULL) {
    fprintf(stderr,"error getting archive path\n");
    return EXIT_FAILURE;
  }
  if((idxfile = cfg_get_first_value(&fo_default_conf,"ThreadIndexFile")) == NULL) {
    fprintf(stderr,"error getting index file\n");
    return EXIT_FAILURE;
  }

  if((years = opendir(ent->values[0])) == NULL) {
    fprintf(stderr,"opendir(%s): %s\n",ent->values[0],strerror(errno));
    return EXIT_FAILURE;
  }

  while((year = readdir(years)) != NULL) {
    if(is_digit(year) == -1) continue;

    do_year(year->d_name);
  }

  closedir(years);

  array_sort(&idx,cmp);
  if((fd = fopen(idxfile->values[0],"w")) == NULL) {
    fprintf(stderr,"fopen(%s): %s\n",idxfile->values[0],strerror(errno));
    return EXIT_FAILURE;
  }
  fwrite(idx.array,idx.element_size,idx.elements,fd);
  fclose(fd);

  cfg_cleanup(&fo_default_conf);
  cfg_cleanup_file(&dconf);

  array_destroy(cfgfiles);
  free(cfgfiles);

  return EXIT_SUCCESS;
}
/* }}} */

/* eof */
