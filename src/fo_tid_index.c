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

#include <db.h>
#include <getopt.h>

#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "cfcgi.h"
#include "template.h"
#include "readline.h"
#include "clientlib.h"
#include "fo_tid_index.h"
/* }}} */

/** Database containig the index entries */
DB *Tdb = NULL;

/** contains forum name */
static u_char *forum_name = NULL;

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
void index_month(char *year,char *month) {
  t_name_value *apath = cfg_get_first_value(&fo_default_conf,forum_name,"ArchivePath");
  char path[256],path1[256],ym[256];
  t_tid_index midx;
  struct stat st;
  DBT key,data;
  size_t ym_len,len;
  t_string str;
  int ret;
  u_int64_t x;
  u_char y[50];

  DIR *m;
  struct dirent *ent;

  (void)snprintf(path,256,"%s/%s/%s",apath->values[0],year,month);
  (void)snprintf(path1,256,"%s/%s/%s/.leave",apath->values[0],year,month);
  ym_len = snprintf(ym,256,"%s/%s",year,month);

  if(stat(path1,&st) == 0) return;

  if((m = opendir(path)) == NULL) {
    fprintf(stderr,"opendir(%s): %s\n",path,strerror(errno));
    exit(EXIT_FAILURE);
  }

  memset(&key,0,sizeof(key));
  memset(&data,0,sizeof(data));
  str_init(&str);

  while((ent = readdir(m)) != NULL) {
    if(is_thread(ent->d_name) == -1) continue;

    x   = strtoull(ent->d_name+1,NULL,10);
    len = snprintf(y,50,"%llu",x);

    key.data = y;
    key.size = len;

    if(Tdb->get(Tdb,NULL,&key,&data,0) == 0) {
      str_chars_append(&str,data.data,data.size);
      str_char_append(&str,'\0');
      str_chars_append(&str,ym,ym_len);

      data.data = str.content;
      data.size = str.len + 1;

      Tdb->del(Tdb,NULL,&key,0);
      if((ret = Tdb->put(Tdb,NULL,&key,&data,0)) != 0) {
        fprintf(stderr,"DB error: %s\n",db_strerror(ret));
        exit(-1);
      }

      str_cleanup(&str);
    }
    else {
      data.data = ym;
      data.size = ym_len;

      if((ret = Tdb->put(Tdb,NULL,&key,&data,0)) != 0) {
        fprintf(stderr,"DB error: %s\n",db_strerror(ret));
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
void do_year(char *year) {
  t_name_value *apath = cfg_get_first_value(&fo_default_conf,NULL,"ArchivePath");
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

static struct option cmdline_options[] = {
  { "config-directory", 1, NULL, 'c' },
  { "forum-name",       1, NULL, 'f' },
  { NULL,               0, NULL, 0   }
};

/* {{{ usage */
void usage(void) {
  fprintf(stderr,"Usage:\n" \
    "[CF_CONF_DIR=\"/path/to/config\"] fo_tid_index [options]\n\n" \
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
  t_configfile dconf,sconf;
  t_name_value *ent,*idxfile;
  u_char c;

  DIR *years;
  struct dirent *year;
  int ret;

  static const u_char *wanted[] = {
    "fo_default"
  };

  /* {{{ read options from commandline */
  while((c = getopt_long(argc,argv,"c:f",cmdline_options,NULL)) > 0) {
    switch(c) {
      case 'c':
        if(!optarg) usage();
        setenv("CF_CONF_DIR",optarg,1);
        break;
      case 'f':
        if(!optarg) usage();
        forum_name = strdup(optarg);
        break;
      default:
        usage();
    }
  }
  /* }}} */

  if(!forum_name) usage();

  cfg_init();

  if((cfgfiles = get_conf_file(wanted,1)) == NULL) {
    fprintf(stderr,"error getting config files\n");
    return EXIT_FAILURE;
  }

  file = *((u_char **)array_element_at(cfgfiles,1));
  cfg_init_file(&dconf,file);
  free(file);

  cfg_register_options(&sconf,fo_server_options);

  if(read_config(&dconf,NULL,CFG_MODE_CONFIG) != 0) {
    fprintf(stderr,"config file error!\n");

    cfg_cleanup_file(&dconf);

    return EXIT_FAILURE;
  }

  if((ent = cfg_get_first_value(&fo_default_conf,forum_name,"ArchivePath")) == NULL) {
    fprintf(stderr,"error getting archive path\n");
    return EXIT_FAILURE;
  }
  if((idxfile = cfg_get_first_value(&fo_default_conf,forum_name,"ThreadIndexFile")) == NULL) {
    fprintf(stderr,"error getting index file\n");
    return EXIT_FAILURE;
  }

  /* {{{ open database */
  if((ret = db_create(&Tdb,NULL,0)) != 0) {
    fprintf(stderr,"DB error: %s\n",db_strerror(ret));
    return EXIT_FAILURE;
  }

  if((ret = Tdb->open(Tdb,NULL,idxfile->values[0],NULL,DB_BTREE,DB_CREATE,0644)) != 0) {
    fprintf(stderr,"DB error: %s\n",db_strerror(ret));
    return EXIT_FAILURE;
  }
  /* }}} */

  if((years = opendir(ent->values[0])) == NULL) {
    fprintf(stderr,"opendir(%s): %s\n",ent->values[0],strerror(errno));
    return EXIT_FAILURE;
  }

  while((year = readdir(years)) != NULL) {
    if(is_digit(year) == -1) continue;

    do_year(year->d_name);
  }

  closedir(years);

  /* {{{ close database */
  if(Tdb) Tdb->close(Tdb,0);
  /* }}} */

  cfg_cleanup(&fo_default_conf);
  cfg_cleanup_file(&dconf);

  array_destroy(cfgfiles);
  free(cfgfiles);

  return EXIT_SUCCESS;
}
/* }}} */

/* eof */
