/**
 * \file flt_voting.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 *
 * Implementation of the voting protocol stack
 */

/* {{{ Initial comments */
/*
 * $LastChangedDate: 2004-06-09 15:55:53 +0200 (Wed, 09 Jun 2004) $
 * $LastChangedRevision: 106 $
 * $LastChangedBy: cseiler $
 *
 */
/* }}} */

/* {{{ Includes */
#include "config.h"
#include "defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include <pthread.h>

#include <sys/stat.h>
#include <sys/types.h>

#include "cf_pthread.h"

#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "readline.h"
#include "fo_server.h"
#include "serverlib.h"
/* }}} */

int flt_voting_handler(int sockfd,const u_char **tokens,int tnum,rline_t *tsd) {
  t_cf_hash *infos;
  u_char *ln = NULL,*tmp,*ctid,*cmid;
  u_int64_t tid,mid;
  int err = 0;
  t_thread *t;
  t_posting *p;
  long llen;

  if(tnum != 2) return FLT_DECLINE;

  infos = cf_hash_new(NULL);

  do {
    ln  = readline(sockfd,tsd);

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
        writen(sockfd,"503 Bad request\n",16);
        err = 1;
        break;
      }

      free(ln);
    }
  } while(ln);

  if(err == 0) {
    if(cf_strcmp(tokens[1],"GOOD") == 0) {
      ctid = cf_hash_get(infos,"Tid",3);
      cmid = cf_hash_get(infos,"Mid",3);

      if(!ctid || !cmid) {
        writen(sockfd,"503 Bad request\n",16);
        cf_hash_destroy(infos);
        return FLT_OK;
      }

      tid = strtoull(ctid,NULL,10);
      mid = strtoull(cmid,NULL,10);

      cf_hash_destroy(infos);

      /* {{{ get thread and posting */
      if((t = cf_get_thread(tid)) == NULL) {
        writen(sockfd,"404 Thread Not Found\n",21);
        return FLT_OK;
      }

      if((p = cf_get_posting(t,mid)) == NULL) {
        writen(sockfd,"404 Posting Not Found\n",22);
        return FLT_OK;
      }
      /* }}} */

      CF_RW_WR(&t->lock);
      p->votes_good++;
      CF_RW_UN(&t->lock);

      writen(sockfd,"200 Ok\n",7);
    }
    else if(cf_strcmp(tokens[1],"BAD") == 0) {
      ctid = cf_hash_get(infos,"Tid",3);
      cmid = cf_hash_get(infos,"Mid",3);

      if(!ctid || !cmid) {
        writen(sockfd,"503 Bad request\n",16);
        cf_hash_destroy(infos);
        return FLT_OK;
      }

      tid = strtoull(ctid,NULL,10);
      mid = strtoull(cmid,NULL,10);

      cf_hash_destroy(infos);

      /* {{{ get thread and posting */
      if((t = cf_get_thread(tid)) == NULL) {
        writen(sockfd,"404 Thread Not Found\n",21);
        return FLT_OK;
      }

      if((p = cf_get_posting(t,mid)) == NULL) {
        writen(sockfd,"404 Posting Not Found\n",22);
        return FLT_OK;
      }
      /* }}} */

      CF_RW_WR(&t->lock);
      p->votes_bad++;
      CF_RW_UN(&t->lock);

      writen(sockfd,"200 Ok\n",7);
    }
    else {
      cf_hash_destroy(infos);
      return FLT_DECLINE;
    }

    return FLT_OK;
  }

  cf_hash_destroy(infos);
  return FLT_OK;
}

int flt_voting_register_handlers(int sock) {
  cf_register_protocol_handler("VOTE",flt_voting_handler);
  return FLT_OK;
}

t_conf_opt flt_voting_config[] = {
  { NULL, NULL, 0, NULL }
};

t_handler_config flt_voting_handlers[] = {
  { INIT_HANDLER,            flt_voting_register_handlers   },
  { 0, NULL }
};

t_module_config flt_voting = {
  flt_voting_config,
  flt_voting_handlers,
  NULL,
  NULL,
  NULL,
  NULL
};

/* eof */
