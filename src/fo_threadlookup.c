/**
 * \file fo_threadlookup.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 * \brief This program creates a tid index
 *
 * This file does a thread id lookup in the index files
 * and redirects to the right URL
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

/* {{{ is_tid */
/**
 * Function checking if a string contains of numbers
 * \param c The string
 * \return 0 if it consists of numbers, -1 if not
 */
int is_tid(const u_char *c) {
  register u_char *ptr = (u_char *)c;

  for(;*ptr;ptr++) {
    if(!isdigit(*ptr)) return -1;
  }

  return 0;
}
/* }}} */

/* {{{ cmp */
#ifndef DOXYGEN
int cmp(const void *elem1,const void *elem2) {
  u_int64_t *tid = (u_int64_t *)elem1;
  t_tid_index *id = (t_tid_index *)elem2;

  if(*tid >= id->start && *tid <= id->end) return 0;
  if(*tid < id->start) return -1;

  return 1;
}
#endif
/* }}} */

/* {{{ main */
/**
 * Main function
 * \param argc Argument count
 * \param argv Argument vector
 * \param envp Environment vector
 * \return EXIT_SUCCESS or EXIT_FAILURE
 */
int main(int argc,char *argv[],char *envp[]) {
  t_array *cfgfiles;
  u_char *file;
  t_configfile dconf;
  u_int64_t tid;
  u_char *ctid;
  t_array index;
  struct stat st;
  t_name_value *v;
  FILE *fd;
  t_tid_index *idx;
  t_name_value *archive_path;
  u_char *port = getenv("SERVER_PORT");
  t_array infos;

  static const u_char *wanted[] = {
    "fo_default"
  };

  cfg_init();

  cf_cgi_parse_path_info(&infos);

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

  if(infos.elements != 2 && infos.elements != 4) {
    /** \todo Cleanup code */
    return EXIT_FAILURE;
  }

  ctid = *((u_char **)array_element_at(&infos,1));
  if(is_tid(ctid) == -1) {
    /** \todo cleanup code */
    return EXIT_FAILURE;
  }
  if((v = cfg_get_first_value(&fo_default_conf,"ThreadIndexFile")) == NULL) {
    /** \todo Cleanup code */
    return EXIT_FAILURE;
  }
  if((archive_path = cfg_get_first_value(&fo_default_conf,"ArchiveURL")) == NULL) {
    /** \todo cleanup file */
    return EXIT_FAILURE;
  }
  if(stat(v->values[0],&st) == -1) {
    printf("Status: 404 Not Found\015\012Content-Type: text/html\015\012\015\012");
    str_error_message("E_FO_404",NULL,8);
    return EXIT_FAILURE;
  }

  tid = strtoull(ctid,NULL,10);

  /* do a lookup */
  array_init(&index,sizeof(t_tid_index),NULL);

  index.array    = fo_alloc(NULL,1,st.st_size,FO_ALLOC_MALLOC);
  index.reserved = st.st_size;
  index.elements = st.st_size / sizeof(t_tid_index);

  if((fd = fopen(v->values[0],"r")) == NULL) {
    printf("Status: 500 Internal Server Error\015\012Content-Type: text/html\015\012\015\012");
    str_error_message("E_ARCHIVE_ERROR",NULL,15);
    return EXIT_FAILURE;
  }
  fread(index.array,sizeof(t_tid_index),st.st_size/sizeof(t_tid_index),fd);
  fclose(fd);

  if((idx = array_bsearch(&index,(void *)&tid,cmp)) == NULL) {
    printf("Status: 404 Not Found\015\012Content-Type: text/html\015\012\015\012");
    str_error_message("E_FO_404",NULL,8);
    return EXIT_FAILURE;
  }

  printf("Status: 302 Moved Temporarily\015\012");


  if(!port || cf_strcmp(port,"80") == 0) {
    if(infos.elements == 4) {
      printf("Location: http://%s%s%d/%d/%s/#m%s\015\012\015\012",getenv("SERVER_NAME"),archive_path->values[0],idx->year,idx->month,ctid,*((char **)array_element_at(&infos,3)));
    }
    else {
      printf("Location: http://%s%s%d/%d/%s/\015\012\015\012",getenv("SERVER_NAME"),archive_path->values[0],idx->year,idx->month,ctid);
    }
  }
  else {
    if(infos.elements == 4) {
      printf("Location: http://%s:%s%s%d/%d/%s/#m%s\015\012\015\012",getenv("SERVER_NAME"),getenv("SERVER_PORT"),archive_path->values[0],idx->year,idx->month,ctid,*((char **)array_element_at(&infos,3)));
    }
    else {
      printf("Location: http://%s:%s%s%d/%d/%s/\015\012\015\012",getenv("SERVER_NAME"),getenv("SERVER_PORT"),archive_path->values[0],idx->year,idx->month,ctid);
    }
  }

  cfg_cleanup(&fo_default_conf);
  cfg_cleanup_file(&dconf);

  array_destroy(cfgfiles);
  free(cfgfiles);

  return EXIT_SUCCESS;
}
/* }}} */

/* eof */
