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
#include "config.h"
#include "defines.h"

#include <db.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <pthread.h>

#include "cf_pthread.h"

#include "readline.h"
#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "readline.h"
#include "fo_server.h"
#include "serverlib.h"
/* }}} */

static DB *NamesDB = NULL;
static u_char *AuthNames = NULL;

u_char *transform(const u_char *val) {
  register u_char *ptr = (u_char *)val;
  register int ws = 0;
  t_string str;

  if(!ptr) return NULL;

  str_init(&str);

  /* jump over leading whitespaces */
  for(;*ptr && isspace(*ptr);++ptr);

  for(;*ptr;++ptr) {
    /* we want &nbsp; as a whitespace, too */
    if(isspace(*ptr) || (*ptr == 0xC2 && *ptr == 0xA0)) {
      if(ws == 0) str_char_append(&str,' ');
      ws = 1;
    }
    else {
      ws = 0;
      str_char_append(&str,tolower(*ptr));
    }
  }

  /* delete trailing whitespaces */
  while(isspace(str.content[str.len-1])) str.len--;
  str.content[str.len] = '\0';

  return str.content;
}

int check_auth(u_char *name,u_char *pass) {
  DBT key,data;
  int ret;

  memset(&key,0,sizeof(key));
  memset(&data,0,sizeof(data));

  key.data = name;
  key.size = strlen(name);

  if((ret = NamesDB->get(NamesDB,NULL,&key,&data,0)) != 0) {
    if(ret != DB_NOTFOUND) cf_log(LOG_ERR,__FILE__,__LINE__,"db->get: %s\n",db_strerror(ret));
    return ret == DB_NOTFOUND ? 0 : -1;
  }

  if(!pass) return 1;
  if(cf_strcmp(data.data,pass) == 0) return 0;

  return 1;
}

unsigned long long hashval(DB *db,u_char *key,size_t length) {
  return lookup(key,length,0);
}

int flt_registerednames_handler(int connfd,const u_char **tokens,int tnum,rline_t *tsd) {
  u_char *ln = NULL,*tmp,*names[2] = { NULL, NULL },*pass = NULL;
  long llen;
  t_cf_hash *infos;
  DBT key,data;
  int ret;

  memset(&key,0,sizeof(key));
  memset(&data,0,sizeof(data));

  if(cf_strcmp(tokens[0],"AUTH") == 0) {
    infos = cf_hash_new(NULL);

    do {
      ln  = readline(connfd,tsd);
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
        if(cf_strncmp(tokens[1],"CHECK",5) == 0) {
          names[0] = transform(cf_hash_get(infos,"Name",4));
          pass     = cf_hash_get(infos,"Pass",4);

          if(!names[0]) {
            writen(connfd,"503 Bad request\n",16);
          }
          else {
            if(check_auth(names[0],pass) == 0) writen(connfd,"200 Ok\n",7);
            else writen(connfd,"504 Auth required\n",18);
          }
        }
        else if(cf_strncmp(tokens[1],"DELETE",6) == 0) {
          names[0] = transform(cf_hash_get(infos,"Name",4));
          pass     = cf_hash_get(infos,"Pass",4);

          if(!names[0] || (!pass && tnum != 3)) {
            writen(connfd,"504 Auth required\n",18);
          }
          else {
            if(tnum != 3 && check_auth(names[0],pass) != 0) {
              writen(connfd,"504 Auth required\n",18);
            }
            else {
              key.data = names[0];
              key.size = strlen(names[0]);
              if((ret = NamesDB->del(NamesDB,NULL,&key,0)) != 0) {
                if(ret != DB_NOTFOUND) cf_log(LOG_ERR,__FILE__,__LINE__,"DB->del: %s\n",db_strerror(ret));
                writen(connfd,"504 Auth required\n",18);
              }
              else {
                writen(connfd,"200 Ok\n",7);
              }
            }
          }
        }
        else if(cf_strncmp(tokens[1],"SET",3) == 0) {
          names[0] = transform(cf_hash_get(infos,"Name",4));
          names[1] = transform(cf_hash_get(infos,"New-Name",8));
          pass     = cf_hash_get(infos,"Pass",4);

          if(!names[1] || !pass) {
            writen(connfd,"504 Auth required\n",18);
          }
          else {
            if(names[0]) {
              if(check_auth(names[0],pass) == 0) {
                /* check if registered and set auth */
                key.data = names[1];
                key.size = strlen(names[1]);

                if((ret = NamesDB->get(NamesDB,NULL,&key,&data,0)) == 0) {
                  writen(connfd,"504 Auth required\n",18);
                }
                else {
                  if(ret == DB_NOTFOUND) {
                    data.data = pass;
                    data.size = strlen(pass)+1; // don't forget the terminating \0
                    if((ret = NamesDB->put(NamesDB,NULL,&key,&data,0)) != 0) {
                      cf_log(LOG_ERR,__FILE__,__LINE__,"DB->put: %s\n",db_strerror(ret));
                      writen(connfd,"504 Auth required\n",18);
                    }
                    else {
                      writen(connfd,"200 Ok\n",7);

                      key.data = names[0];
                      key.size = strlen(names[0]);

                      if((ret = NamesDB->del(NamesDB,NULL,&key,0)) != 0) {
                        if(ret != DB_NOTFOUND) cf_log(LOG_ERR,__FILE__,__LINE__,"ALERT! DB->del failed: %s\n",db_strerror(ret));
                      }
                    }
                  }
                  else {
                    cf_log(LOG_ERR,__FILE__,__LINE__,"DB->get: %s\n",db_strerror(ret));
                    writen(connfd,"504 Auth required\n",18);
                  }
                }
              }
              else {
                writen(connfd,"504 Auth required\n",18);
              }
            }
            else {
              /* check if registered and set auth */
              key.data = names[1];
              key.size = strlen(names[1]);

              if((ret = NamesDB->get(NamesDB,NULL,&key,&data,0)) == 0) {
                writen(connfd,"504 Auth required\n",18);
              }
              else {
                if(ret == DB_NOTFOUND) {
                  data.data = pass;
                  data.size = strlen(pass)+1;
                  if((ret = NamesDB->put(NamesDB,NULL,&key,&data,0)) != 0) {
                    cf_log(LOG_ERR,__FILE__,__LINE__,"DB->put: %s\n",db_strerror(ret));
                    writen(connfd,"504 Auth required\n",18);
                  }
                  else {
                    writen(connfd,"200 Ok\n",7);
                  }
                }
                else {
                  cf_log(LOG_ERR,__FILE__,__LINE__,"DB->get: %s\n",db_strerror(ret));
                  writen(connfd,"504 Auth required\n",18);
                }
              }
            }
          }

        }
        else if(cf_strncmp(tokens[1],"GET",3) == 0) {
          names[0] = transform(cf_hash_get(infos,"Name",4));
          key.data = names[0];
          key.size = strlen(names[0]);

          if((ret = NamesDB->get(NamesDB,NULL,&key,&data,0)) == 0) {
            writen(connfd,"504 Auth required\n",18);
          }

        }
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

int flt_registerednames_init_module(int sock) {
  int ret;

  if(!AuthNames) {
    cf_log(LOG_ERR,__FILE__,__LINE__,"I need a database to save the names in!\n");
    return FLT_EXIT;
  }

  if((ret = db_create(&NamesDB,NULL,0)) != 0) {
    cf_log(LOG_ERR,__FILE__,__LINE__,"db_create: %s\n",db_strerror(ret));
    return FLT_EXIT;
  }

  if((ret = NamesDB->open(NamesDB,NULL,AuthNames,NULL,DB_HASH,DB_CREATE,0644)) != 0) {
    cf_log(LOG_ERR,__FILE__,__LINE__,"DB->open(%s): %s\n",AuthNames,db_strerror(ret));
    return FLT_EXIT;
  }

  cf_register_protocol_handler("AUTH",flt_registerednames_handler);

  return FLT_OK;
}

int flt_registerednames_handle_command(t_configfile *cf,t_conf_opt *opt,const u_char *context,u_char **args,size_t argnum) {
  if(argnum == 1) {
    if(AuthNames) free(AuthNames);
    AuthNames = strdup(args[0]);
  }
  else {
    cf_log(LOG_ERR,__FILE__,__LINE__,"flt_registerednames: expecting one argument for directive AuthNames!\n");
  }

  return 0;
}

void flt_registerednames_cleanup(void) {
  if(AuthNames) free(AuthNames);
  if(NamesDB)   NamesDB->close(NamesDB,0);
}

t_conf_opt flt_registerednames_config[] = {
  { "AuthNames", flt_registerednames_handle_command, CFG_OPT_CONFIG|CFG_OPT_NEEDED, NULL },
  { NULL, NULL, 0, NULL }
};

t_handler_config flt_registerednames_handlers[] = {
  { INIT_HANDLER,            flt_registerednames_init_module   },
  { 0, NULL }
};

t_module_config flt_registerednames = {
  flt_registerednames_config,
  flt_registerednames_handlers,
  NULL,
  NULL,
  NULL,
  flt_registerednames_cleanup
};

/* eof */
