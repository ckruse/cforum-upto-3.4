/**
 * \file fo_cleanup.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 * \brief This program does cleanup work
 *
 * This file cleans up all visited and deleted files
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

#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>

/* socket includes */
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/un.h>

#include <sys/file.h>

#include <db.h>
#include <getopt.h>

#include "readline.h"
#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "cfcgi.h"
#include "template.h"
#include "clientlib.h"
#include "readline.h"
/* }}} */

static u_char *forum_name = NULL;

/* {{{ cleanup_deleted_file */
/**
 * function for cleaning up a file containing message ids of deleted threads
 * \param sock The socket to the server
 * \param st Reference pointer to the stat buffer
 * \param str Reference to a string containing the filename
 */
int cleanup_deleted_file(int sock,rline_t *tsd,struct stat *st,t_string *str) {
  u_char buff[512];
  char *line;

  size_t len;
  int ret;

  DBC *cursor;
  DB *db;
  DBT db_key;
  DBT db_data;
  int fd;

  if((ret = db_create(&db,NULL,0)) != 0) {
    fprintf(stderr,"DB error: %s\n",db_strerror(ret));
    return EXIT_FAILURE;
  }
  
  if((ret = db->open(db,NULL,str->content,NULL,DB_BTREE,0,0)) != 0) {
    fprintf(stderr,"[%s] DB error: %s\n",str->content,db_strerror(ret));
    return EXIT_FAILURE;
  }

  /* {{{ lock database */
  if((ret = db->fd(db,&fd)) != 0) {
    fprintf(stderr,"[%s] DB error: %s\n",str->content,db_strerror(ret));
    return EXIT_FAILURE;
  }

  if((ret = flock(fd,LOCK_EX)) != 0) {
    fprintf(stderr,"[%s] DB error: %s\n",str->content,db_strerror(ret));
    return EXIT_FAILURE;
  }
  /* }}} */

  if((ret = db->cursor(db,NULL,&cursor,0)) != 0) {
    fprintf(stderr,"[%s] DB error: %s\n",str->content,db_strerror(ret));
    return EXIT_FAILURE;
  }

  while((ret = cursor->c_get(cursor,&db_key,&db_data,DB_NEXT)) == 0) {
    len = snprintf(buff,512,"STAT THREAD t%s\n",(u_char *)db_key.data);
    writen(sock,buff,len);

    if((line = readline(sock,tsd)) == NULL) {
      perror("readline");
      return EXIT_FAILURE;
    }

    if(cf_strncmp(line,"404",3) == 0) db->del(db,NULL,&db_key,0);

    free(line);
  }

  cursor->c_close(cursor);
  db->close(db,0);

  return EXIT_SUCCESS;
}
/* }}} */

/* {{{ compare */
#ifndef DOXYGEN
int compare(t_cf_tree_dataset *a,t_cf_tree_dataset *b) {
  if(*((u_int64_t *)a->key) < *((u_int64_t *)b->key)) return -1;
  if(*((u_int64_t *)a->key) > *((u_int64_t *)b->key)) return 1;

  return 0;
}
#endif
/* }}} */

/* {{{ cleanup_visited_file */
/**
 * function for cleaning up a file containing the visited postings of a user
 * \param sock The socket to the server
 * \param st The stat buffer
 * \param str Reference pointer to a string containing the filename
 */
int cleanup_visited_file(int sock,rline_t *rsd,struct stat *st,t_string *str) {
  t_cf_tree tree;
  u_char *line;
  u_int64_t mid;
  t_cf_tree_dataset data = { NULL, NULL };

  int ret,fd;

  DBC *cursor;
  DB *db;
  DBT db_key;
  DBT db_data;

  if((ret = db_create(&db,NULL,0)) != 0) {
    fprintf(stderr,"DB error: %s\n",db_strerror(ret));
    return EXIT_FAILURE;
  }
  
  if((ret = db->open(db,NULL,str->content,NULL,DB_BTREE,0,0)) != 0) {
    fprintf(stderr,"[%s] DB error: %s\n",str->content,db_strerror(ret));
    return EXIT_FAILURE;
  }

  /* {{{ lock database */
  if((ret = db->fd(db,&fd)) != 0) {
    fprintf(stderr,"[%s] DB error: %s\n",str->content,db_strerror(ret));
    return EXIT_FAILURE;
  }

  if((ret = flock(fd,LOCK_EX)) != 0) {
    fprintf(stderr,"[%s] DB error: %s\n",str->content,db_strerror(ret));
    return EXIT_FAILURE;
  }
  /* }}} */

  cf_tree_init(&tree,compare,NULL);

  data.data = NULL;

  writen(sock,"GET MIDLIST\n",12);
  line = readline(sock,rsd);

  if(!line || cf_strncmp(line,"200",3)) {
    if(line) free(line);
    cf_tree_destroy(&tree);
    fprintf(stderr,"server returned not 200\n");
    return EXIT_FAILURE;
  }

  while((line = readline(sock,rsd)) != NULL) {
    if(*line == '\n') {
      free(line);
      break;
    }

    mid       = strtoull(line+1,NULL,10);
    data.key  = memdup(&mid,sizeof(mid));
    data.data = NULL;

    cf_tree_insert(&tree,NULL,&data);

    free(line);
  }

  if((ret = db->cursor(db,NULL,&cursor,0)) != 0) {
    cf_tree_destroy(&tree);
    fprintf(stderr,"[%s] DB error: %s\n",str->content,db_strerror(ret));
    return EXIT_FAILURE;
  }

  while((ret = cursor->c_get(cursor,&db_key,&db_data,DB_NEXT)) == 0) {
    mid = strtoull(db_key.data,NULL,10);

    data.key  = &mid;
    data.data = NULL;

    if(cf_tree_find(&tree,tree.root,&data) == NULL) db->del(db,NULL,&db_key,0);
  }

  cursor->c_close(cursor);
  db->close(db,0);
  cf_tree_destroy(&tree);

  return EXIT_SUCCESS;
}
/* }}} */

/* {{{ cleanup_files */
/**
 * Function for reading all users visited and deleted files
 * \param sock The socket to the server
 */
int cleanup_files() {
  t_name_value *v;
  DIR *udir,*d_char1,*d_char2,*d_char3;
  struct dirent *dp,*char1,*char2,*char3;
  t_string str;
  u_int32_t len;
  int ret;
  struct stat st;
  u_char *line;
  u_char buff[256];
  size_t len1;
  rline_t rsd;
  int sock;

  v = cfg_get_first_value(&fo_default_conf,forum_name,"ConfigDirectory");
  udir = opendir(v->values[0]);
  len = strlen(v->values[0]);

  cf_hash_set(GlobalValues,"FORUM_NAME",10,forum_name,strlen(forum_name));
  if((sock = cf_socket_setup()) < 0) {
    perror("socket");
    return EXIT_FAILURE;
  }

  if(!udir) {
    perror("opendir");
    return EXIT_FAILURE;
  }

  memset(&rsd,0,sizeof(rsd));

  /* {{{ select forum */
  len1 = snprintf(buff,256,"SELECT %s\n",forum_name);
  writen(sock,buff,len1);
  line = readline(sock,&rsd);
  if(!line || cf_strncmp(line,"200",3)) {
    if(line) free(line);
    fprintf(stderr,"server returned not 200\n");
    return EXIT_FAILURE;
  }
  /* }}} */


  str_init(&str);

  while((char1 = readdir(udir)) != NULL) {
    /* we don't want . and .. */
    if(*char1->d_name == '.') continue;

    str_chars_append(&str,v->values[0],len);
    str_char_append(&str,'/');
    str_chars_append(&str,char1->d_name,strlen(char1->d_name));

    /* we only want it if it is a directory */
    if(stat(str.content,&st) == -1) {
      str_cleanup(&str);
      continue;
    }
    if(!S_ISDIR(st.st_mode)) {
      str_cleanup(&str);
      continue;
    }

    if((d_char1 = opendir(str.content)) == NULL) {
      str_cleanup(&str);
      continue;
    }

    while((char2 = readdir(d_char1)) != NULL) {
      /* we don't want . and .. */
      if(*char2->d_name == '.') continue;

      str_char_append(&str,'/');
      str_chars_append(&str,char2->d_name,strlen(char2->d_name));

      if(stat(str.content,&st) == -1) {
        str.len -= strlen(char2->d_name) + 1;
        continue;
      }
      if(!S_ISDIR(st.st_mode)) {
        str.len -= strlen(char2->d_name) + 1;
        continue;
      }

      if((d_char2 = opendir(str.content)) == NULL) {
        str.len -= strlen(char2->d_name) + 1;
        continue;
      }

      while((char3 = readdir(d_char2)) != NULL) {
        if(*char3->d_name == '.') continue;

        str_char_append(&str,'/');
        str_chars_append(&str,char3->d_name,strlen(char3->d_name));

        if(stat(str.content,&st) == -1) {
          str.len -= strlen(char3->d_name) + 1;
          continue;
        }
        if(!S_ISDIR(st.st_mode)) {
          str.len -= strlen(char3->d_name) + 1;
          continue;
        }

        if((d_char3 = opendir(str.content)) == NULL) {
          str.len -= strlen(char3->d_name) + 1;
          continue;
        }

        while((dp = readdir(d_char3)) != NULL) {
          if(*dp->d_name == '.') continue;

          str_char_append(&str,'/');
          str_chars_append(&str,dp->d_name,strlen(dp->d_name));

          if(stat(str.content,&st) == -1) {
            str.len -= strlen(dp->d_name) + 1;
            continue;
          }
          if(!S_ISDIR(st.st_mode)) {
            str.len -= strlen(dp->d_name) + 1;
            continue;
          }

          str_chars_append(&str,"/dt.dat",7);

          /* we have to check if there is a deleted.dat. If not, it is no valid user directory */
          if(stat(str.content,&st) == -1) {
            str.len -= strlen(dp->d_name) + 13;
            continue;
          }

          if((ret = cleanup_deleted_file(sock,&rsd,&st,&str)) != EXIT_SUCCESS) {
            closedir(udir);
            str_cleanup(&str);
            return ret;
          }

          str.len -= 12;
          str_chars_append(&str,"/vt.dat",7);

          /* we have to check if there is a visited.dat. If not, it is no valid user directory */
          if(stat(str.content,&st) == -1) {
            str.len -= strlen(dp->d_name) + 13;
            continue;
          }

          if((ret = cleanup_visited_file(sock,&rsd,&st,&str)) != EXIT_SUCCESS) {
            closedir(udir);
            str_cleanup(&str);
            return ret;
          }

          str.len -= strlen(dp->d_name) + 13;
        }

        str.len -= strlen(char3->d_name) + 1;
        closedir(d_char3);
      }

      str.len -= strlen(char2->d_name) + 1;
      closedir(d_char2);
    }

    closedir(d_char1);
    str_cleanup(&str);
  }


  /* cleanup source */
  writen(sock,"QUIT\n",5);
  close(sock);

  closedir(udir);

  return EXIT_SUCCESS;
}
/* }}} */

/**
 * Dummy function, for ignoring unknown directives
 */
int ignre(t_configfile *cfile,const u_char *context,u_char *name,u_char **args,size_t len) {
  return 0;
}

/* {{{ usage */
void usage(void) {
  fprintf(stderr,"Usage:\n" \
    "[CF_CONF_DIR=\"/path/to/config\"] fo_cleanup [options]\n\n" \
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

/* {{{ cleanup_votes */
void cleanup_votes() {
  t_name_value *v;
  size_t len;
  u_char *line;
  rline_t rsd;
  t_cf_tree tree;
  u_int64_t mid;
  t_cf_tree_dataset data = { NULL, NULL };
  DBC *cursor;
  DB *db;
  DBT db_key;
  DBT db_data;
  int fd,ret,sock;
  u_char buff[256];


  v = cfg_get_first_value(&fo_vote_conf,forum_name,"VotingDatabase");

  if((ret = db_create(&db,NULL,0)) != 0) {
    fprintf(stderr,"DB error: %s\n",db_strerror(ret));
    return;
  }
  
  if((ret = db->open(db,NULL,v->values[0],NULL,DB_BTREE,0,0)) != 0) {
    fprintf(stderr,"[%s] DB error: %s\n",v->values[0],db_strerror(ret));
    return;
  }

  /* {{{ lock database */
  if((ret = db->fd(db,&fd)) != 0) {
    fprintf(stderr,"[%s] DB error: %s\n",v->values[0],db_strerror(ret));
    return;
  }

  if((ret = flock(fd,LOCK_EX)) != 0) {
    fprintf(stderr,"[%s] DB error: %s\n",v->values[0],db_strerror(ret));
    return;
  }
  /* }}} */

  cf_tree_init(&tree,compare,NULL);
  memset(&rsd,0,sizeof(rsd));

  cf_hash_set(GlobalValues,"FORUM_NAME",10,forum_name,strlen(forum_name));
  if((sock = cf_socket_setup()) < 0) {
    perror("socket");
    return;
  }

  /* {{{ select forum */
  len = snprintf(buff,256,"SELECT %s\n",forum_name);
  writen(sock,buff,len);
  line = readline(sock,&rsd);
  if(!line || cf_strncmp(line,"200",3)) {
    if(line) free(line);
    cf_tree_destroy(&tree);
    fprintf(stderr,"server returned not 200\n");
    return;
  }
  /* }}} */

  writen(sock,"GET MIDLIST\n",12);
  line = readline(sock,&rsd);
  if(!line || cf_strncmp(line,"200",3)) {
    if(line) free(line);
    cf_tree_destroy(&tree);
    fprintf(stderr,"server returned not 200\n");
    return;
  }

  while((line = readline(sock,&rsd)) != NULL) {
    if(*line == '\n') {
      free(line);
      break;
    }

    mid       = strtoull(line+1,NULL,10);
    data.key  = memdup(&mid,sizeof(mid));
    data.data = NULL;

    cf_tree_insert(&tree,NULL,&data);

    free(line);
  }

  if((ret = db->cursor(db,NULL,&cursor,0)) != 0) {
    cf_tree_destroy(&tree);
    fprintf(stderr,"[%s] DB error: %s\n",v->values[0],db_strerror(ret));
    return;
  }

  while((ret = cursor->c_get(cursor,&db_key,&db_data,DB_NEXT)) == 0) {
    mid = strtoull(db_key.data,NULL,10);

    data.key  = &mid;
    data.data = NULL;

    if(cf_tree_find(&tree,tree.root,&data) == NULL) db->del(db,NULL,&db_key,0);
  }

  cursor->c_close(cursor);
  db->close(db,0);
  cf_tree_destroy(&tree);
}
/* }}} */

static struct option cmdline_options[] = {
  { "config-directory", 1, NULL, 'c' },
  { "forum-name",       1, NULL, 'f' },
  { NULL,               0, NULL, 0   }
};

/* {{{ main */
/**
 * The main function
 * \param argc Argument count
 * \param argv Argument vector
 * \param envp Environment vector
 * \return EXIT_SUCCESS or EXIT_FAILURE
 */
int main(int argc,char *argv[],char *envp[]) {
  int    sock,ret;
  t_array *cfgfiles;
  u_char *file;
  t_configfile conf,dconf,vconf;
  char c;

  static const u_char *wanted[] = {
    "fo_default","fo_view","fo_vote"
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

  cf_init();
  init_modules();
  cfg_init();

  if((cfgfiles = get_conf_file(wanted,3)) == NULL) {
    fprintf(stderr,"coult not get config files\n");
    return EXIT_FAILURE;
  }

  sock = 0;

  file = *((u_char **)array_element_at(cfgfiles,0));
  cfg_init_file(&dconf,file);
  free(file);

  file = *((u_char **)array_element_at(cfgfiles,1));
  cfg_init_file(&conf,file);
  free(file);

  file = *((u_char **)array_element_at(cfgfiles,2));
  cfg_init_file(&vconf,file);
  free(file);

  cfg_register_options(&dconf,default_options);
  cfg_register_options(&conf,fo_view_options);
  cfg_register_options(&vconf,fo_vote_options);

  if(read_config(&dconf,ignre,CFG_MODE_CONFIG|CFG_MODE_NOLOAD) != 0 ||
     read_config(&conf,ignre,CFG_MODE_CONFIG|CFG_MODE_NOLOAD) != 0 ||
     read_config(&vconf,ignre,CFG_MODE_CONFIG|CFG_MODE_NOLOAD) != 0) {
    fprintf(stderr,"config file error!\n");

    return EXIT_FAILURE;
  }

  ret = cleanup_files();
  cleanup_votes();

  return ret;
}
/* }}} */

/* eof */
