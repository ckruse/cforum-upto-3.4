/**
 * \file fo_vote.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 * \brief The forum voting program
 */

/* {{{ Initial comment */
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
#include <errno.h>
#include <ctype.h>

#include <sys/types.h>
#include <signal.h>

/* socket includes */
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/un.h>

#include <db.h>

#include "readline.h"
#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "cfcgi.h"
#include "template.h"
#include "clientlib.h"
/* }}} */

/* {{{ signal handler for bad signals */
void sighandler(int segnum) {
  FILE *fd = fopen(PROTOCOL_FILE,"a");
  u_char buff[10],*uname = NULL,*qs = NULL;

  if(fd) {
    qs    = getenv("QUERY_STRING");
    if(GlobalValues) uname = cf_hash_get(GlobalValues,"UserName",8);
    
    switch(segnum) {
      case SIGSEGV:
        snprintf(buff,10,"SIGSEGV");
        break;
      case SIGILL:
        snprintf(buff,10,"SIGILL");
        break;
      case SIGFPE:
        snprintf(buff,10,"SIGFPE");
        break;
      case SIGBUS:
        snprintf(buff,10,"SIGBUS");
        break;
      default:
        snprintf(buff,10,"UKNOWN");
        break;
    }

    fprintf(fd,"fo_vote: Got signal %s!\nUsername: %s\nQuery-String: %s\n----\n",buff,uname?uname:(u_char *)"(null)",qs?qs:(u_char *)"(null)");
    fclose(fd);
  }

  exit(0);
}
/* }}} */

/* {{{ is_id */
int is_id(const u_char *id) {
  register const u_char *ptr;

  for(ptr=id;*ptr;ptr++) {
    if(!isdigit(*ptr)) return 0;
  }

  return 1;
}
/* }}} */

/**
 * The main function of the forum voting program. No command line switches
 * used.
 * \param argc The argument count
 * \param argv The argument vector
 * \param env The environment vector
 * \return EXIT_SUCCESS on success, EXIT_FAILURE on error
 */
int main(int argc,char *argv[],char *env[]) {
  /* {{{ initialization */
  static const u_char *wanted[] = {
    "fo_default", "fo_vote"
  };

  int sock,ret;
  t_array *cfgfiles;
  t_configfile dconf,conf;
  u_char *fname,*ctid,*cmid,*a,buff[512],*uname;
  t_cf_hash *head;
  size_t len;
  DB *db;
  DBT key,data;
  t_name_value *dbname;

  /* set signal handler for SIGSEGV (for error reporting) */
  signal(SIGSEGV,sighandler);
  signal(SIGILL,sighandler);
  signal(SIGFPE,sighandler);
  signal(SIGBUS,sighandler);

  if((cfgfiles = get_conf_file(wanted,2)) == NULL) {
    return EXIT_FAILURE;
  }

  cf_init();
  init_modules();
  cfg_init();

  sock = 0;

  fname = *((u_char **)array_element_at(cfgfiles,0));
  cfg_init_file(&dconf,fname);
  free(fname);

  fname = *((u_char **)array_element_at(cfgfiles,1));
  cfg_init_file(&conf,fname);
  free(fname);

  cfg_register_options(&dconf,default_options);
  cfg_register_options(&conf,fo_vote_options);

  if(read_config(&dconf,NULL) != 0 || read_config(&conf,NULL) != 0) {
    fprintf(stderr,"config file error!\n");
    cfg_cleanup_file(&dconf);
    cfg_cleanup_file(&conf);
    return EXIT_FAILURE;
  }

  head   = cf_cgi_new();
  dbname = cfg_get_first_value(&fo_vote_conf,"VotingDatabase");

  /* first action: authorization modules */
  if(Modules[AUTH_HANDLER].elements) {
    size_t i;
    t_filter_begin exec;
    t_handler_config *handler;

    ret = FLT_DECLINE;

    for(i=0;i<Modules[AUTH_HANDLER].elements && ret == FLT_DECLINE;i++) {
      handler = array_element_at(&Modules[AUTH_HANDLER],i);

      exec = (t_filter_begin)handler->func;
      ret = exec(head,&fo_default_conf,&fo_vote_conf);
    }
  }
  /* }}} */

  if((uname = cf_hash_get(GlobalValues,"UserName",8)) == NULL) printf("Status: 403 Forbidden\015\012\015\012");
  
  if(head && uname) {
    ctid = cf_cgi_get(head,"t");
    cmid = cf_cgi_get(head,"m");
    a    = cf_cgi_get(head,"a");

    if(cmid && ctid && a && is_id(cmid) && is_id(ctid)) {
      if((sock = set_us_up_the_socket()) != -1) {
        if((ret = db_create(db,NULL,0)) != 0) {
          printf("Status: 500 Internal Server Error\015\012\015\012");
          str_error_message("E_FO_500",NULL,8);
          fprintf(stderr,"DB error: %s\n",db_strerror(ret));
          return EXIT_FAILURE;
        }

        if((ret = db->open(db,NULL,dbname->values[0],NULL,DB_BTREE,DB_CREATE,0644)) != 0) {
          printf("Status: 500 Internal Server Error\015\012\015\012");
          str_error_message("E_FO_500",NULL,8);
          fprintf(stderr,"DB error: %s\n",db_strerror(ret));
          return EXIT_FAILURE;
        }

        key.data = uname;
        key.size = strlen(uname);
        data.data = "1";
        data.size = 1;

        if((ret = db->put(db,NULL,&key,&data,DB_NODUPDATA|DB_NOOVERWRITE)) != 0) {
          printf("Status: 500 Internal Server Error\015\012\015\012");
          str_error_message("E_FO_500",NULL,8);
          fprintf(stderr,"DB error: %s\n",db_strerror(ret));
          return EXIT_FAILURE;
        }

        printf("Status: 204 No Content\015\012\015\012");

        len = snprintf(buff,512,"VOTE %s\nTid: %s\nMid: %s\n\nQUIT\n",*a=='g'?"GOOD":"BAD",ctid,cmid);
        writen(sock,buff,len);
        close(sock);
      }
    }
  }
  else printf("Status: 204 No Content\015\012\015\012");


  /* {{{ cleanup */
  if(head) cf_hash_destroy(head);

  /* cleanup source */
  cfg_cleanup(&fo_default_conf);
  cfg_cleanup_file(&dconf);

  array_destroy(cfgfiles);
  free(cfgfiles);

  cleanup_modules(Modules);
  cf_fini();

  return EXIT_SUCCESS;
  /* }}} */
}

/* eof */
