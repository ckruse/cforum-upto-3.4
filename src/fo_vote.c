/**
 * \file fo_vote.c
 * \author Christian Kruse, <cjk@wwwtech.de>
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
#include "cfconfig.h"
#include "defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <errno.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/file.h>
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
#include "cfgcomp.h"
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

/* {{{ send_ok_output */
void send_ok_output(cf_cfg_config_t *cfg,cf_hash_t *head,cf_cfg_config_value_t *cs) {
  cf_cfg_config_value_t *fbase,*cf_cfg_tpl,*mode,*lang;
  u_char tpl_name[256];
  cf_template_t tpl;
  int uname = cf_hash_get(GlobalValues,"UserName",8) != NULL;
  u_char *link;

  cf_string_t *ctid = cf_cgi_get(head,"t"),
    *cmid = cf_cgi_get(head,"m");

  u_int64_t tid,mid;

  mode = cf_cfg_get_value(cfg,"TemplateMode");
  lang = cf_cfg_get_value(cfg,"Language");
  cf_cfg_tpl = cf_cfg_get_value(cfg,"OkTemplate");

  cf_gen_tpl_name(tpl_name,256,mode->sval,lang->sval,cf_cfg_tpl->sval);

  tid   = cf_str_to_uint64(ctid->content);
  mid   = cf_str_to_uint64(cmid->content);
  link  = cf_get_link(NULL,tid,mid);

  if(cf_tpl_init(&tpl,tpl_name) != 0) {
    printf("500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->sval);
    cf_error_message(cfg,"E_TPL_NOT_FOUND",NULL);
    return;
  }

  fbase = cf_cfg_get_value(cfg,"BaseURL");

  cf_set_variable(&tpl,cs->sval,"backlink",link,strlen(link),0); //TODO: forum-backlink
  cf_set_variable(&tpl,cs->sval,"forumbase",fbase->avals[uname].sval,strlen(fbase->avals[uname].sval),1); //TODO: forum-base-uri
  cf_set_variable(&tpl,cs->sval,"charset",cs->sval,strlen(cs->sval),1);

  free(link);

  printf("Content-Type: text/html; charset=%s\015\012\015\012",cs->sval);
  cf_tpl_parse(&tpl);
  cf_tpl_finish(&tpl);
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
    "fo_default", "fo_view", "fo_vote"
  };

  int sock,ret;
  u_char buff[512],*uname,*ucfg,*line;
  cf_hash_t *head;
  size_t len;
  DB *db;
  DBT key,data;
  cf_cfg_config_value_t *dbname,*cs,*send204,*cfgpath;
  int fd;
  u_char *forum_name;
  rline_t rsd;

  cf_readmode_t rm_infos;

  cf_string_t *ctid,*cmid,*a,*mode = NULL;

  cf_cfg_config_t cfg;

  /* set signal handler for SIGSEGV (for error reporting) */
  signal(SIGSEGV,sighandler);
  signal(SIGILL,sighandler);
  signal(SIGFPE,sighandler);
  signal(SIGBUS,sighandler);

  cf_init();

  forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  sock = 0;

  if(cf_cfg_get_conf(&cfg,wanted,3) != 0) {
    fprintf(stderr,"Config file error\n");
    return EXIT_FAILURE;
  }

  head   = cf_cgi_new();
  dbname = cf_cfg_get_value(&cfg,"VotingDatabase");
  cs     = cf_cfg_get_value(&cfg,"ExternCharset");
  cfgpath= cf_cfg_get_value(&cfg,"ConfigDirectory");

  ret = cf_run_auth_handlers(&cfg,head);

  if((uname = cf_hash_get(GlobalValues,"UserName",8)) == NULL) {
    printf("Status: 403 Forbidden\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->sval);
    cf_error_message(&cfg,"E_VOTE_AUTH",NULL);
    return EXIT_SUCCESS;
  }

  if(head && uname) {
    ctid = cf_cgi_get(head,"t");
    cmid = cf_cgi_get(head,"m");
    a    = cf_cgi_get(head,"a");
    mode = cf_cgi_get(head,"mode");

    /* {{{ read user config */
    if((ucfg = cf_get_uconf_name(cfgpath->sval,uname)) != NULL) {
      if(cf_cfg_read_conffile(&cfg,ucfg) != 0) {
        fprintf(stderr,"config file error!\n");

        printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->sval);
        cf_error_message(&cfg,"E_VOTE_INTERNAL",NULL);

        return EXIT_FAILURE;
      }
    }
    /* }}} */

    /* {{{ get readmode information */
    memset(&rm_infos,0,sizeof(rm_infos));
    if((ret = cf_run_readmode_collectors(&cfg,head,&rm_infos)) != FLT_OK) {
      printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->sval);
      fprintf(stderr,"cf_run_readmode_collectors() returned %d!\n",ret);
      cf_error_message(&cfg,"E_CONFIG_ERR",NULL);
      ret = FLT_EXIT;
    }
    else cf_hash_set(GlobalValues,"RM",2,&rm_infos,sizeof(rm_infos));
    /* }}} */

    send204 = cf_cfg_get_value(&cfg,"Send204");


    if(cmid && ctid && a && is_id(cmid->content) && is_id(ctid->content)) {
      cfgpath = cf_cfg_get_value(&cfg,"SocketName");

      if((sock = cf_socket_setup(cfgpath->sval)) != -1) {
        /* {{{ open database and lock it */
        if((ret = db_create(&db,NULL,0)) != 0) {
          printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->sval);
          cf_error_message(&cfg,"E_FO_500",NULL);
          fprintf(stderr,"fo_vote: db_create() error: %s\n",db_strerror(ret));
          return EXIT_FAILURE;
        }

        if((ret = db->open(db,NULL,dbname->sval,NULL,DB_BTREE,DB_CREATE,0644)) != 0) {
          printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->sval);
          cf_error_message(&cfg,"E_FO_500",NULL);
          fprintf(stderr,"fo_vote: db->open() error: %s\n",db_strerror(ret));
          return EXIT_FAILURE;
        }

        if((ret = db->fd(db,&fd)) != 0) {
          printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->sval);
          cf_error_message(&cfg,"E_FO_500",NULL);
          fprintf(stderr,"fo_vote: db->fd() error: %s\n",db_strerror(ret));
          return EXIT_FAILURE;
        }

        if((ret = flock(fd,LOCK_EX)) != 0) {
          printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->sval);
          cf_error_message(&cfg,"E_FO_500",NULL);
          fprintf(stderr,"fo_vote: db->fd() error: %s\n",strerror(errno));
          return EXIT_FAILURE;
        }
        /* }}} */

        memset(&key,0,sizeof(key));
        memset(&data,0,sizeof(data));

        len = snprintf(buff,512,"%s_%s",uname,cmid->content);

        key.data = buff;
        key.size = len;

        if((ret = db->get(db,NULL,&key,&data,0)) == 0) {
          printf("Status: 403 Forbidden\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->sval);
          if(mode && cf_strcmp(mode->content,"xmlhttp") == 0) printf("0\n");
          else cf_error_message(&cfg,"E_VOTE_MULTIPLE",NULL);
          return EXIT_FAILURE;
        }

        if(ret != DB_NOTFOUND) {
          printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->sval);
          if(mode && cf_strcmp(mode->content,"xmlhttp") == 0) printf("0\n");
          else cf_error_message(&cfg,"E_VOTE_INTERNAL",NULL);
          fprintf(stderr,"fo_vote: db->get() error: %s\n",db_strerror(ret));
          return EXIT_FAILURE;
        }

        data.data = "1";
        data.size = 1;

        if((ret = db->put(db,NULL,&key,&data,0)) != 0) {
          printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->sval);
          if(mode && cf_strcmp(mode->content,"xmlhttp") == 0) printf("0\n");
          else cf_error_message(&cfg,"E_VOTE_INTERNAL",NULL);
          fprintf(stderr,"fo_vote: db->put() error: %s\n",db_strerror(ret));
          return EXIT_FAILURE;
        }

        flock(fd,LOCK_UN);
        db->close(db,0);

        len = snprintf(buff,512,"SELECT %s\nVOTE %s\nTid: %s\nMid: %s\n\nQUIT\n",forum_name,*a->content=='g'?"GOOD":"BAD",ctid->content,cmid->content);
        writen(sock,buff,len);

        if(mode && cf_strcmp(mode->content,"xmlhttp") == 0) {
          memset(&rsd,0,sizeof(rsd));

          printf("Content-Type: text/html\015\012\015\012");

          if((line = readline(sock,&rsd)) != NULL) {
            if(cf_strncmp(line,"200 Ok",6) == 0) {
              free(line);
              if((line = readline(sock,&rsd)) != NULL) {
                if(cf_strncmp(line,"200 Ok",6) == 0) {
                  free(line);

                  if((line = readline(sock,&rsd)) != NULL) {
                    printf("%d",atoi(line+5));
                    free(line);
                  }
                  else printf("0");
                }
                else {
                  free(line);
                  printf("0");
                }
              }
              else printf("0");
            }
            else {
              free(line);
              printf("0");
            }
          }
          else printf("0");

          close(sock);
        }
        else if(send204 && cf_strcmp(send204->sval,"yes") == 0) {
          close(sock);
          printf("Status: 204 No Content\015\012\015\012");
        }
        else {
          close(sock);
          send_ok_output(&cfg,head,cs);
        }
      }
      else {
        printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->sval);
        fprintf(stderr,"fo_vote: could not socket: %s\n",strerror(errno));
        if(mode && cf_strcmp(mode->content,"xmlhttp") == 0) printf("0\n");
        else cf_error_message(&cfg,"E_VOTE_INTERNAL",NULL);
      }
    }
    else {
      printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->sval);
      if(mode && cf_strcmp(mode->content,"xmlhttp") == 0) printf("0\n");
      else cf_error_message(&cfg,"E_VOTE_INTERNAL",NULL);
    }
  }
  else {
    printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->sval);
    if(mode && cf_strcmp(mode->content,"xmlhttp") == 0) printf("0\n");
    else cf_error_message(&cfg,"E_VOTE_INTERNAL",NULL);
  }


  /* {{{ cleanup */
  if(head) cf_hash_destroy(head);

  /* cleanup source */
  cf_fini();
  cf_cfg_config_destroy(&cfg);

  return EXIT_SUCCESS;
  /* }}} */
}

/* eof */
