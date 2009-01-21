/**
 * \file fo_tid_index.c
 * \author Christian Kruse, <cjk@wwwtech.de>
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

#include <errno.h>
#include <dirent.h>

/* socket includes */
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/un.h>

#include <db.h>
#include <getopt.h>

#include "hashlib.h"
#include "utils.h"
#include "cfgcomp.h"
#include "cfcgi.h"
#include "template.h"
#include "readline.h"
#include "clientlib.h"
#include "fo_tid_index.h"
/* }}} */

/** Database containig the index entries */
DB *Tdb = NULL;

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

  for(;*ptr && isdigit(*ptr);++ptr) dg = 1;

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
void index_month(cf_cfg_config_t *cfg,char *year,char *month) {
  cf_cfg_config_value_t *apath = cf_cfg_get_value(cfg,"ArchivePath");
  char path[256],path1[256],ym[256];
  struct stat st;
  DBT key,data;
  size_t ym_len,len;
  cf_string_t str;
  int ret;
  u_int64_t x;
  u_char y[50];

  DIR *m;
  struct dirent *ent;

  (void)snprintf(path,256,"%s/%s/%s",apath->sval,year,month);
  (void)snprintf(path1,256,"%s/%s/%s/.leave",apath->sval,year,month);
  ym_len = snprintf(ym,256,"%s/%s",year,month);

  if(stat(path1,&st) == 0) return;

  if((m = opendir(path)) == NULL) {
    fprintf(stderr,"cf-tid_index: opendir(%s): %s\n",path,strerror(errno));
    exit(EXIT_FAILURE);
  }

  memset(&key,0,sizeof(key));
  memset(&data,0,sizeof(data));
  cf_str_init(&str);

  while((ent = readdir(m)) != NULL) {
    if(is_thread(ent->d_name) == -1) continue;

    x   = cf_str_to_uint64(ent->d_name+1);
    len = snprintf(y,50,"%" PRIu64,x);

    key.data = y;
    key.size = len;

    if(Tdb->get(Tdb,NULL,&key,&data,0) == 0) {
      cf_str_chars_append(&str,data.data,data.size);
      cf_str_char_append(&str,'\0');
      cf_str_chars_append(&str,ym,ym_len);

      data.data = str.content;
      data.size = str.len + 1;

      Tdb->del(Tdb,NULL,&key,0);
      if((ret = Tdb->put(Tdb,NULL,&key,&data,0)) != 0) {
        fprintf(stderr,"cf-tid_index: db->put(%s,%s) error: %s\n",y,str.content,db_strerror(ret));
        exit(-1);
      }

      cf_str_cleanup(&str);
    }
    else {
      data.data = ym;
      data.size = ym_len;

      if((ret = Tdb->put(Tdb,NULL,&key,&data,0)) != 0) {
        fprintf(stderr,"cf-tid_index: db->put(%s,%s) error: %s\n",y,(u_char *)data.data,db_strerror(ret));
        exit(-1);
      }
    }
  }

  closedir(m);
}
/* }}} */

/* {{{ do_year */
/**
 * Function for indexing a complete year
 * \param year The year
 */
void do_year(cf_cfg_config_t *cfg,char *year) {
  cf_cfg_config_value_t *apath = cf_cfg_get_value(cfg,"ArchivePath");
  char path[256];

  DIR *months;
  struct dirent *ent;

  (void)snprintf(path,256,"%s/%s",apath->sval,year);

  if((months = opendir(path)) == NULL) {
    fprintf(stderr,"cf-tid_index: opendir(%s): %s\n",path,strerror(errno));
    exit(EXIT_FAILURE);
  }

  while((ent = readdir(months)) != NULL) {
    if(is_digit(ent) == -1) continue;
    index_month(cfg,year,ent->d_name);
  }

  closedir(months);
}
/* }}} */

static struct option cmdline_options[] = {
  { "config-directory", 1, NULL, 'c' },
  { "forum-name",       1, NULL, 'f' },
  { NULL,               0, NULL, 0   }
};

/* {{{ usage */
void usage(void) {
  fprintf(stderr,"Usage:\n" \
    "[CF_CONF_DIR=\"/path/to/config\"] cf-tid_index [options]\n\n" \
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

/**
 * Main function
 * \param argc Argument count
 * \param argv Argument vector
 * \param envp Environment vector
 */
int main(int argc,char *argv[],char *envp[]) {
  cf_cfg_config_value_t *ent,*idxfile;
  char c;

  u_char *forum_name = NULL;

  DIR *years;
  struct dirent *year;
  int ret;

  static const u_char *wanted[] = {
    "fo_server","fo_default"
  };

  cf_cfg_config_t cfg;

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

  /* {{{ configuration files */
  setenv("CF_FORUM_NAME",forum_name,1);

  if(cf_cfg_get_conf(&cfg,wanted,2)) {
    fprintf(stderr,"Config error\n");
    return EXIT_FAILURE;
  }
  /* }}} */

  ent = cf_cfg_get_value(&cfg,"ArchivePath");
  idxfile = cf_cfg_get_value(&cfg,"DF:ThreadIndexFile");

  /* {{{ open database */
  if((ret = db_create(&Tdb,NULL,0)) != 0) {
    fprintf(stderr,"cf-tid_index: db_create() error: %s\n",db_strerror(ret));
    return EXIT_FAILURE;
  }

  if((ret = Tdb->open(Tdb,NULL,idxfile->sval,NULL,DB_BTREE,DB_CREATE,0644)) != 0) {
    fprintf(stderr,"cf-tid_index: db->open(%s) error: %s\n",idxfile->sval,db_strerror(ret));
    return EXIT_FAILURE;
  }
  /* }}} */

  if((years = opendir(ent->sval)) == NULL) {
    fprintf(stderr,"cf-tid_index: opendir(%s): %s\n",ent->sval,strerror(errno));
    return EXIT_FAILURE;
  }

  while((year = readdir(years)) != NULL) {
    if(is_digit(year) == -1) continue;

    do_year(&cfg,year->d_name);
  }

  closedir(years);

  if(Tdb) Tdb->close(Tdb,0);

  cf_cfg_config_destroy(&cfg);

  return EXIT_SUCCESS;
}

/* eof */
