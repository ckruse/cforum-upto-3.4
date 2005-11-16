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
#include "cfconfig.h"
#include "defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <inttypes.h>

#include <dirent.h>

/* socket includes */
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/un.h>

#include <db.h>

#include "hashlib.h"
#include "utils.h"
#include "cfgcomp.h"
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

/* {{{ gen_archive_url */
void gen_archive_url(u_char *buff,u_char *aurl,u_char *url,u_int64_t tid,u_int64_t mid) {
  register u_char *ptr = buff,*ptr1;
  int slash = 1;
  u_char *server,*port;
  size_t len;

  server = getenv("SERVER_NAME");
  port   = getenv("SERVER_NAME");


  if(!port || cf_strcmp(port,"80") == 0) len = snprintf(ptr,256,"http://%s/",server);
  else len = snprintf(ptr,256,"http://%s:%s/",server,port);
  ptr += len;

  for(ptr1=aurl;*ptr1;ptr1++) {
    if(*ptr1 == '/') {
      if(slash == 0) *ptr++ = '/';
      slash++;
    }
    else {
      *ptr++ = *ptr1;
      slash = 0;
    }
  }

  if(slash == 0) {
    *ptr++ = '/';
    slash = 1;
  }

  /* ok, append normal URI */
  for(ptr1=url;*ptr1;ptr1++) {
    if(*ptr1 == '/') {
      if(slash == 0) *ptr++ = '/';
      slash++;
    }
    else {
      *ptr++ = *ptr1;
      slash = 0;
    }
  }

  if(slash == 0) *ptr++ = '/';

  /* now, append tid */
  len = sprintf(ptr,"t%" PRIu64 "/",tid);
  ptr += len;

  if(mid) len = sprintf(ptr,"#m%" PRIu64,mid);
  ptr += len;

  *ptr = '\0';
}
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
  /* {{{ variables */
  u_char *forum_name;
  cf_array_t *cfgfiles;
  u_char *file;
  cf_configfile_t dconf;
  u_int64_t tid,mid = 0;
  u_char *ctid,buff[256];
  struct stat st;
  cf_name_value_t *v;
  cf_name_value_t *archive_path,*cs;
  cf_array_t infos;
  DB *Tdb;
  DBT key,data;
  int ret;
  size_t len;

  static const u_char *wanted[] = {
    "fo_default"
  };
  /* }}} */

  /* {{{ initialization */
  cf_cfg_init();
  cf_init();

  forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);

  cf_cgi_parse_path_info(&infos);

  if((cfgfiles = cf_get_conf_file(wanted,1)) == NULL) {
    fprintf(stderr,"error getting config files\n");
    return EXIT_FAILURE;
  }

  file = *((u_char **)cf_array_element_at(cfgfiles,0));
  cf_cfg_init_file(&dconf,file);
  cf_cfg_register_options(&dconf,default_options);
  free(file);

  if(cf_read_config(&dconf,NULL,CF_CFG_MODE_CONFIG) != 0) {
    fprintf(stderr,"config file error!\n");

    cf_cfg_cleanup_file(&dconf);
    return EXIT_FAILURE;
  }

  cs = cf_cfg_get_first_value(&fo_default_conf,forum_name,"ExternCharset");

  /* {{{ check for right number of arguments */
  if(infos.elements != 2 && infos.elements != 4) {
    printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]),
    cf_error_message("E_FO_500",NULL);
    fprintf(stderr,"Wrong argument count: %zu\n",infos.elements);
    return EXIT_FAILURE;
  }
  /* }}} */

  /* {{{ check if parameters are valid */
  ctid = *((u_char **)cf_array_element_at(&infos,1));
  if(is_tid(ctid) == -1) {
    printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]),
    cf_error_message("E_FO_500",NULL);
    fprintf(stderr,"Wrong argument, no tid\n");
    return EXIT_FAILURE;
  }
  /* }}} */

  v = cf_cfg_get_first_value(&fo_default_conf,forum_name,"ThreadIndexFile");
  archive_path = cf_cfg_get_first_value(&fo_default_conf,forum_name,"ArchiveURL");
  /* }}} */

  /* {{{ open database */
  if(stat(v->values[0],&st) == -1) {
    printf("Status: 404 Not Found\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
    cf_error_message("E_FO_404",NULL);
    return EXIT_FAILURE;
  }
  if((ret = db_create(&Tdb,NULL,0)) != 0) {
    printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]),
    cf_error_message("E_FO_500",NULL);
    fprintf(stderr,"db_create() error: %s\n",db_strerror(ret));
    return EXIT_FAILURE;
  }

  if((ret = Tdb->open(Tdb,NULL,v->values[0],NULL,DB_BTREE,DB_RDONLY,0)) != 0) {
    printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]),
    cf_error_message("E_FO_500",NULL);
    fprintf(stderr,"db->open(%s) error: %s\n",v->values[0],db_strerror(ret));
    return EXIT_FAILURE;
  }
  /* }}} */

  /* {{{ get URLs */
  tid = cf_str_to_uint64(ctid);
  if(infos.elements == 4) mid = cf_str_to_uint64(*((u_char **)cf_array_element_at(&infos,3)));

  memset(&key,0,sizeof(key));
  memset(&data,0,sizeof(data));

  key.data = ctid;
  key.size = strlen(ctid);

  if((ret = Tdb->get(Tdb,NULL,&key,&data,0)) != 0) {
    printf("Status: 404 Not Found\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
    cf_error_message("E_FO_404",NULL);
    return EXIT_FAILURE;
  }
  /* }}} */

  /* {{{ check if there is more than one entry */
  len = strlen(data.data);
  if(len < data.size) {
    /* k, we have got more than one thread. What to do? At the moment we ignore it. */
  }
  /* }}} */

  /* {{{ redirect */
  gen_archive_url(buff,archive_path->values[0],data.data,tid,mid);
  cf_http_redirect_with_nice_uri(buff,1);
  /* }}} */

  /* {{{ cleanup */
  cf_cfg_cleanup_file(&dconf);

  cf_array_destroy(cfgfiles);
  free(cfgfiles);
  cf_cfg_destroy();
  /* }}} */

  return EXIT_SUCCESS;
}
/* }}} */

/* eof */
