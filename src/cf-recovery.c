/**
 * \file fo_recovery.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 * \brief The forum recovery program
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
#include <ctype.h>
#include <string.h>

#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

/* for sockets */
#include <sys/socket.h>
#include <netdb.h>
#include <sys/un.h>

#include <signal.h>
#include <pthread.h>
#include <getopt.h>

#ifdef CF_SHARED_MEM
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

#include "semaphores.h"
#endif

#include "cf_pthread.h"

#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "readline.h"
#include "serverutils.h"
#include "serverlib.h"
#include "fo_server.h"
/* }}} */

/* definition of the global variables; needed because of undefined references */
int    RUN;
t_head head;

#define CFFS_SUPPORTED_VERSION "0.2"

#if 0
/* {{{ read_posting_from_file */
void read_posting_from_file(FILE *fd,t_posting *p) {
  t_posting n;
  int fr = 0;

  /* in this case we only want to have the filepointer to the next posting */
  if(p == NULL) {
    p = &n;
    fr = 1;
  }

  /* username */
  fread(&p->user.name_len,sizeof(p->user.name_len),1,fd);

  p->user.name = fo_alloc(NULL,p->user.name_len+1,1,FO_ALLOC_MALLOC);
  fread(p->user.name,p->user.name_len,1,fd);
  p->user.name[p->user.name_len] = '\0';


  /* email */
  fread(&p->user.email_len,sizeof(p->user.email_len),1,fd);

  if(p->user.email_len) {
    p->user.email = fo_alloc(NULL,p->user.email_len+1,1,FO_ALLOC_MALLOC);
    fread(p->user.email,p->user.email_len,1,fd);
    p->user.email[p->user.email_len] = '\0';
  }
  else p->user.email = NULL;

  /* homepage */
  fread(&p->user.hp_len,sizeof(p->user.hp_len),1,fd);
  if(p->user.hp_len) {
    p->user.hp = fo_alloc(NULL,p->user.hp_len+1,1,FO_ALLOC_MALLOC);
    fread(p->user.hp,p->user.hp_len,1,fd);
    p->user.hp[p->user.hp_len] = '\0';
  }
  else p->user.hp = NULL;

  /* image */
  fread(&p->user.img_len,sizeof(p->user.img_len),1,fd);
  if(p->user.img_len) {
    p->user.img = fo_alloc(NULL,p->user.img_len+1,1,FO_ALLOC_MALLOC);
    fread(p->user.img,p->user.img_len,1,fd);
    p->user.img[p->user.img_len] = '\0';
  }
  else p->user.img = NULL;


  /* ip */
  fread(&p->user.ip_len,sizeof(p->user.ip_len),1,fd);
  p->user.ip = fo_alloc(NULL,p->user.ip_len+1,1,FO_ALLOC_MALLOC);
  fread(p->user.ip,p->user.ip_len,1,fd);
  p->user.ip[p->user.ip_len] = '\0';

  /* unique id */
  fread(&p->unid_len,sizeof(p->unid_len),1,fd);
  p->unid = fo_alloc(NULL,p->unid_len+1,1,FO_ALLOC_MALLOC);
  fread(p->unid,p->unid_len,1,fd);
  p->unid[p->unid_len] = '\0';

  /* subject */
  fread(&p->subject_len,sizeof(p->subject_len),1,fd);
  p->subject = fo_alloc(NULL,p->subject_len+1,1,FO_ALLOC_MALLOC);
  fread(p->subject,p->subject_len,1,fd);
  p->subject[p->subject_len] = '\0';

  /* category */
  fread(&p->category_len,sizeof(p->category_len),1,fd);
  if(p->category_len) {
    p->category = fo_alloc(NULL,p->category_len+1,1,FO_ALLOC_MALLOC);
    fread(p->category,p->category_len,1,fd);
    p->category[p->category_len] = '\0';
  }
  else p->category = NULL;

  /* content */
  fread(&p->content_len,sizeof(p->content_len),1,fd);
  p->content = fo_alloc(NULL,p->content_len+1,1,FO_ALLOC_MALLOC);
  fread(p->content,p->content_len,1,fd);
  p->content[p->content_len] = '\0';

  /* date */
  fread(&p->date,sizeof(u_int32_t),1,fd);

  /* votes */
  fread(&p->votes_good,sizeof(p->votes_good),1,fd);
  fread(&p->votes_bad,sizeof(p->votes_bad),1,fd);

  /* level */
  fread(&p->level,sizeof(p->level),1,fd);

  /* visibility */
  fread(&p->invisible,sizeof(p->invisible),1,fd);

  /* ok, end of posting */
  if(fr) {
    if(p->user.email) free(p->user.email);
    if(p->user.hp) free(p->user.hp);
    if(p->user.img) free(p->user.img);
    if(p->category) free(p->category);

    free(p->user.name);
    free(p->user.ip);
    free(p->subject);
    free(p->content);
    free(p->unid);
  }
}
/* }}} */
#endif

static struct option cmdline_options[] = {
  { "config-directory", 1, NULL, 'c' },
  { "forum-name",       1, NULL, 'f' },
  { NULL,               0, NULL, 0   }
};

/* {{{ usage */
void usage(void) {
  fprintf(stderr,"Usage:\n" \
    "[CF_CONF_DIR=\"/path/to/config\"] cf-recovery [options]\n\n" \
    "where options are:\n" \
    "\t-c, --config-directory  Path to the configuration directory\n" \
    "\t-f, --forum-name        Name of the forum to index\n" \
    "\t-h, --help              Show this help screen\n\n" \
    "One of both must be set: config-directory option or CF_CONF_DIR\n" \
    "environment variable\n\n"
  );
  exit(-1);
}
/* }}} */

int main(int argc,char *argv[]) {
  u_char *forum_name = NULL;
  char c;

  u_char *file;
  t_array *cfgfiles;

  t_configfile sconf,dconf;

  t_forum forum;

  static const u_char *wanted[] = {
    "fo_server","fo_default"
  };

  /* {{{ read options from commandline */
  while((c = getopt_long(argc,argv,"c:f:",cmdline_options,NULL)) > 0) {
    switch(c) {
      case 'c':
        if(!optarg) {
          fprintf(stderr,"no configuration path given for argument --config-directory\n");
          usage();
        }
        setenv("CF_CONF_DIR",optarg,1);
        break;

      case 'f':
        if(!optarg) {
          fprintf(stderr,"no forum name given for argument --forum-name\n");
          usage();
        }
        forum_name = strdup(optarg);
        break;

      default:
        fprintf(stderr,"unknown option: %d\n",c);
        usage();
    }
  }
  /* }}} */

  if(!forum_name) {
    fprintf(stderr,"forum name not given!\n");
    usage();
  }

  cfg_init();

  /* {{{ configuration files */
  if((cfgfiles = get_conf_file(wanted,2)) == NULL) {
    fprintf(stderr,"error getting config files\n");
    return EXIT_FAILURE;
  }

  file = *((u_char **)array_element_at(cfgfiles,0));
  cfg_init_file(&sconf,file);
  free(file);

  file = *((u_char **)array_element_at(cfgfiles,1));
  cfg_init_file(&dconf,file);
  free(file);


  cfg_register_options(&dconf,default_options);
  cfg_register_options(&sconf,fo_server_options);

  if(read_config(&dconf,NULL,CFG_MODE_CONFIG) != 0 || read_config(&sconf,NULL,CFG_MODE_CONFIG) != 0) {
    fprintf(stderr,"config file error!\n");

    cfg_cleanup_file(&dconf);

    return EXIT_FAILURE;
  }
  /* }}} */


  forum.name = forum_name;

  forum.cache.fresh = 0;
  str_init(&forum.cache.visible);
  str_init(&forum.cache.invisible);

  forum.date.visible = 0;
  forum.date.invisible = 0;

  forum.locked = 0;

  #ifdef CF_SHARED_MEM
  forum.shm.ids = { 0, 0 };
  forum.shm.sem = 0;
  forum.shm.ptrs = { NULL, NULL };
  #endif

  forum->threads.last_tid = forum->threads.last_mid = 0;
  forum->threads.threads  = cf_hash_new(NULL);
  forum->threads.list     = NULL;
  forum->threads.last     = NULL;

  forum->uniques.ids = cf_hash_new(NULL);

  if(cf_load_data(&forum) == -1) {
    fprintf(stderr,"could not load forum data for forum %s\n",forum_name);
    return EXIT_FAILURE;
  }


  return 0;
}


/* eof */
