/**
 * \file flt_registerednames.c
 * \author Christian Kruse
 *
 * This plugin ensures that posters cannot post with registered names
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
#include <ctype.h>
#include <time.h>

#include <pthread.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <db.h>

struct sockaddr_un;

#include "cf_pthread.h"

#include "hashlib.h"
#include "utils.h"
#include "cfgcomp.h"
#include "readline.h"

#include "serverutils.h"
#include "serverlib.h"
#include "fo_server.h"
/* }}} */

typedef struct s_names_db {
  DB *NamesDB;
  u_char *AuthNames;
} names_db_t;

static cf_hash_t *flt_rn_namesdb = NULL;

void flt_registerednames_cleanup_hash(void *arg) {
  names_db_t *ndb = (names_db_t *)arg;

  ndb->NamesDB->close(ndb->NamesDB,0);
  free(ndb->AuthNames);
}

/* {{{ flt_registerednames_transform */
u_char *flt_registerednames_transform(const u_char *val) {
  register u_char *ptr = (u_char *)val;
  register int ws = 0;
  cf_string_t str;

  if(!ptr) return NULL;

  cf_str_init(&str);

  /* jump over leading whitespaces */
  for(;*ptr && isspace(*ptr);++ptr);

  for(;*ptr;++ptr) {
    /* we want &nbsp; as a whitespace, too */
    if(isspace(*ptr) || (*ptr == 0xC2 && *ptr == 0xA0)) {
      if(ws == 0) cf_str_char_append(&str,' ');
      ws = 1;
    }
    else {
      ws = 0;
      cf_str_char_append(&str,tolower(*ptr));
    }
  }

  /* delete trailing whitespaces */
  while(isspace(str.content[str.len-1])) str.len--;
  str.content[str.len] = '\0';

  return str.content;
}
/* }}} */

/* {{{ flt_registerednames_check_auth */
int flt_registerednames_check_auth(names_db_t *ndb,u_char *name,u_char *pass) {
  DBT key,data;
  int ret;

  memset(&key,0,sizeof(key));
  memset(&data,0,sizeof(data));

  key.data = name;
  key.size = strlen(name);

  if((ret = ndb->NamesDB->get(ndb->NamesDB,NULL,&key,&data,0)) != 0) {
    if(ret != DB_NOTFOUND) cf_log(CF_ERR,__FILE__,__LINE__,"db->get: %s\n",db_strerror(ret));
    return ret == DB_NOTFOUND ? 0 : -1;
  }

  if(!pass) return 1;
  if(cf_strcmp(data.data,pass) == 0) return 0;

  return 1;
}
/* }}} */

/* {{{ flt_registerednames_syncer */
void flt_registerednames_syncer(forum_t *forum) {
  int ret;
  names_db_t *ndb = cf_hash_get(flt_rn_namesdb,forum->name,strlen(forum->name));

  if((ret = ndb->NamesDB->sync(ndb->NamesDB,0)) != 0) {
    cf_log(CF_ERR,__FILE__,__LINE__,"DB->sync: %s\n",db_strerror(ret));
  }
}
/* }}} */

/* {{{ flt_registerednames_handler */
int flt_registerednames_handler(int connfd,forum_t *forum,const u_char **tokens,int tnum,rline_t *tsd) {
  u_char *ln = NULL,*tmp,*names[2] = { NULL, NULL },*pass = NULL;
  long llen;
  cf_hash_t *infos;
  DBT key,data;
  int ret;
  names_db_t *ndb = cf_hash_get(flt_rn_namesdb,forum->name,strlen(forum->name));
  int status;

  memset(&key,0,sizeof(key));
  memset(&data,0,sizeof(data));

  if(cf_strcmp(tokens[0],"AUTH") == 0) {
    infos = cf_hash_new(NULL);

    do {
      ln  = readline(connfd,tsd);
      cf_log(CF_DBG,__FILE__,__LINE__,"%s",ln);

      if(ln) {
        llen = tsd->rl_len;
        while(ln[llen] != '\n') ln[llen--] = '\0'; /* delete the \n */
        ln[llen--] = '\0';

        tmp = strstr(ln,":");

        if(tmp) {
          cf_hash_set(infos,ln,tmp-ln,tmp+2,llen-(int)(tmp-ln));
        }
        else {
          if(*ln == '\0') break;

          free(ln);
          ln = NULL;
          writen(connfd,"503 Bad request\n",16);
          break;
        }

        free(ln);
      }
    } while(ln);

    if(ln) {
      if(tnum == 2 || tnum == 3) {
        /* {{{ AUTH CHECK */
        if(cf_strncmp(tokens[1],"CHECK",5) == 0) {
          names[0] = flt_registerednames_transform(cf_hash_get(infos,"Name",4));
          pass     = cf_hash_get(infos,"Pass",4);

          if(!names[0]) {
            writen(connfd,"503 Bad request\n",16);
          }
          else {
            if(flt_registerednames_check_auth(ndb,names[0],pass) == 0) writen(connfd,"200 Ok\n",7);
            else writen(connfd,"504 Auth required\n",18);
          }
        }
        /* }}} */
        /* {{{ AUTH DELETE */
        else if(cf_strncmp(tokens[1],"DELETE",6) == 0) {
          names[0] = flt_registerednames_transform(cf_hash_get(infos,"Name",4));
          pass     = cf_hash_get(infos,"Pass",4);

          if(!names[0] || (!pass && tnum != 3)) {
            writen(connfd,"504 Auth required\n",18);
          }
          else {
            if(tnum != 3 && (status = flt_registerednames_check_auth(ndb,names[0],pass)) != 0) {
              if(status == -1) writen(connfd,"500 Internal Error\n",19);
              else writen(connfd,"504 Auth required\n",18);
            }
            else {
              key.data = names[0];
              key.size = strlen(names[0]);
              if((ret = ndb->NamesDB->del(ndb->NamesDB,NULL,&key,0)) != 0) {
                if(ret != DB_NOTFOUND) {
                  cf_log(CF_ERR,__FILE__,__LINE__,"DB->del: %s\n",db_strerror(ret));
                  writen(connfd,"500 Internal Error\n",19);
                }
                /* key could not be found, so deletion was successful */
                else writen(connfd,"200 Ok\n",7);
              }
              else writen(connfd,"200 Ok\n",7);
            }
          }
        }
        /* }}} */
        /* {{{ AUTH SET */
        else if(cf_strncmp(tokens[1],"SET",3) == 0) {
          names[0] = flt_registerednames_transform(cf_hash_get(infos,"Name",4));
          names[1] = flt_registerednames_transform(cf_hash_get(infos,"New-Name",8));
          pass     = cf_hash_get(infos,"Pass",4);

          if(!names[1] || !pass) {
            writen(connfd,"504 Auth required\n",18);
          }
          else {
            if(names[0]) {
              if((status = flt_registerednames_check_auth(ndb,names[0],pass)) == 0) {
                /* check if registered and set auth */
                key.data = names[1];
                key.size = strlen(names[1]);

                if((ret = ndb->NamesDB->get(ndb->NamesDB,NULL,&key,&data,0)) == 0) writen(connfd,"504 Auth required\n",18);
                else {
                  if(ret == DB_NOTFOUND) {
                    data.data = pass;
                    data.size = strlen(pass)+1; // don't forget the terminating \0
                    if((ret = ndb->NamesDB->put(ndb->NamesDB,NULL,&key,&data,0)) != 0) {
                      cf_log(CF_ERR,__FILE__,__LINE__,"DB->put: %s\n",db_strerror(ret));
                      writen(connfd,"500 Internal Error\n",19);
                    }
                    else {
                      writen(connfd,"200 Ok\n",7);

                      key.data = names[0];
                      key.size = strlen(names[0]);

                      if((ret = ndb->NamesDB->del(ndb->NamesDB,NULL,&key,0)) != 0) {
                        if(ret != DB_NOTFOUND) cf_log(CF_ERR,__FILE__,__LINE__,"ALERT! DB->del failed: %s\n",db_strerror(ret));
                      }
                    }
                  }
                  else {
                    cf_log(CF_ERR,__FILE__,__LINE__,"DB->get: %s\n",db_strerror(ret));
                    writen(connfd,"500 Internal Error\n",19);
                  }
                }
              }
              else {
                if(status == -1) writen(connfd,"500 Internal Error\n",19);
                else writen(connfd,"504 Auth required\n",18);
              }
            }
            else {
              /* check if registered and set auth */
              key.data = names[1];
              key.size = strlen(names[1]);

              if((ret = ndb->NamesDB->get(ndb->NamesDB,NULL,&key,&data,0)) == 0) {
                if(cf_strcmp(data.data,pass) == 0) writen(connfd,"200 Ok\n",7); /* he tried to re-register */
                else writen(connfd,"504 Auth required\n",18);
              }
              else {
                if(ret == DB_NOTFOUND) {
                  data.data = pass;
                  data.size = strlen(pass)+1;
                  if((ret = ndb->NamesDB->put(ndb->NamesDB,NULL,&key,&data,0)) != 0) {
                    cf_log(CF_ERR,__FILE__,__LINE__,"DB->put: %s\n",db_strerror(ret));
                    writen(connfd,"500 Internal Error\n",19);
                  }
                  else writen(connfd,"200 Ok\n",7);
                }
                else {
                  cf_log(CF_ERR,__FILE__,__LINE__,"DB->get: %s\n",db_strerror(ret));
                  writen(connfd,"500 Internal Error\n",18);
                }
              }
            }
          }

        }
        /* }}} */
        /* {{{ AUTH GET */
        else if(cf_strncmp(tokens[1],"GET",3) == 0) {
          names[0] = flt_registerednames_transform(cf_hash_get(infos,"Name",4));
          key.data = names[0];
          key.size = strlen(names[0]);

          if((ret = ndb->NamesDB->get(ndb->NamesDB,NULL,&key,&data,0)) == 0) {
            writen(connfd,"504 Auth required\n",18);
          }

        }
        /* }}} */
        else {
          writen(connfd,"503 Sorry, I do not understand\n",31);
        }
      }
      else {
        writen(connfd,"503 Sorry, I do not understand\n",31);
      }

      free(ln);
    }

    if(infos) cf_hash_destroy(infos);
    if(names[0]) free(names[0]);
    if(names[1]) free(names[1]);

    return FLT_OK;
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_registerednames_init_module */
int flt_registerednames_init_module(int sock) {
  int ret;
  names_db_t *ndb;
  cf_name_value_t *forums = cf_cfg_get_first_value(&fo_server_conf,NULL,"Forums");
  size_t i;

  if(!flt_rn_namesdb) {
    cf_log(CF_ERR,__FILE__,__LINE__,"I need a database to save the names in!\n");
    return FLT_EXIT;
  }

  for(i=0;i<forums->valnum;++i) {
    if((ndb = cf_hash_get(flt_rn_namesdb,forums->values[i],strlen(forums->values[i]))) == NULL) {
      cf_log(CF_ERR,__FILE__,__LINE__,"could not get AuthNames database for forum %s!\n",forums->values[i]);
      return FLT_EXIT;
    }

    if((ret = db_create(&ndb->NamesDB,NULL,0)) != 0) {
      cf_log(CF_ERR,__FILE__,__LINE__,"db_create: %s\n",db_strerror(ret));
      return FLT_EXIT;
    }

    if((ret = ndb->NamesDB->open(ndb->NamesDB,NULL,ndb->AuthNames,NULL,DB_BTREE,DB_CREATE,0644)) != 0) {
      cf_log(CF_ERR,__FILE__,__LINE__,"DB->open(%s): %s\n",ndb->AuthNames,db_strerror(ret));
      return FLT_EXIT;
    }

    cf_log(CF_DBG,__FILE__,__LINE__,"Created database for %s\n",forums->values[i]);
  }

  cf_register_protocol_handler("AUTH",flt_registerednames_handler);

  return FLT_OK;
}
/* }}} */

/* {{{ flt_registerednames_handle_command */
int flt_registerednames_handle_command(cf_configfile_t *cf,cf_conf_opt_t *opt,const u_char *context,u_char **args,size_t argnum) {
  names_db_t *ndb,ndb1;

  if(flt_rn_namesdb == NULL) flt_rn_namesdb = cf_hash_new(flt_registerednames_cleanup_hash);

  if(argnum == 1) {
    if((ndb = cf_hash_get(flt_rn_namesdb,(u_char *)context,strlen(context))) == NULL) {
      ndb1.AuthNames = strdup(args[0]);
      ndb1.NamesDB = NULL;

      cf_hash_set(flt_rn_namesdb,(u_char *)context,strlen(context),&ndb1,sizeof(ndb1));
    }
    else {
      free(ndb->AuthNames);
      ndb->AuthNames = strdup(args[0]);
    }
  }
  else {
    cf_log(CF_ERR,__FILE__,__LINE__,"flt_registerednames: expecting one argument for directive AuthNames!\n");
    return 1;
  }

  return 0;
}
/* }}} */

void flt_registerednames_cleanup(void) {
  if(flt_rn_namesdb) cf_hash_destroy(flt_rn_namesdb);
}

cf_conf_opt_t flt_registerednames_config[] = {
  { "AuthNames", flt_registerednames_handle_command, CF_CFG_OPT_CONFIG|CF_CFG_OPT_NEEDED|CF_CFG_OPT_LOCAL, NULL },
  { NULL, NULL, 0, NULL }
};

cf_handler_config_t flt_registerednames_handlers[] = {
  { INIT_HANDLER,            flt_registerednames_init_module   },
  { THRDLST_WRITTEN_HANDLER, flt_registerednames_syncer        }
  { 0, NULL }
};

cf_module_config_t flt_registerednames = {
  MODULE_MAGIC_COOKIE,
  flt_registerednames_config,
  flt_registerednames_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  flt_registerednames_cleanup
};

/* eof */
