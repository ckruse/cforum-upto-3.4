/**
 * \file flt_link.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 *
 * This plugin sets <link> elements in the postings and the threadlist
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
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/types.h>

#include "readline.h"
#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "cfcgi.h"
#include "template.h"
#include "clientlib.h"
/* }}} */

#define LINK_OLDEST_FIRST 0
#define LINK_NEWEST_FIRST 1

static int SetLinks    = 0;
static int NoVisited   = 0;
static int LinkOrder   = LINK_OLDEST_FIRST;

t_message *flt_link_get_previous(t_message *msg) {
  t_mod_api is_visited;
  t_message *tmp;

  if(msg->prev) {
    is_visited = cf_get_mod_api_ent("is_visited");
    if(is_visited && NoVisited) {
      for(tmp=msg->prev;tmp && (tmp->invisible == 1 || tmp->may_show == 0 || is_visited(&(tmp->mid)) != NULL);tmp=tmp->prev);
    }

    /* either user wants also unvisited or API failure or no unvisited message could be found */
    if(!is_visited || !NoVisited || !tmp) {
      for(tmp=msg->prev;tmp && (tmp->invisible == 1 || tmp->may_show == 0);tmp=tmp->prev);
    }

    return tmp;
  }

  return NULL;
}


t_message *flt_link_get_next(t_cl_thread *thr,t_message *msg) {
  t_message *tmp,*tmp1;
  t_mod_api is_visited = cf_get_mod_api_ent("is_visited");
  int lvl,first;

  if(LinkOrder == LINK_NEWEST_FIRST) {
    if(msg->next) {
      if(is_visited && NoVisited) {
        for(;msg && (msg->invisible == 1 || msg->may_show == 0 || is_visited(&(msg->mid)) != NULL);msg=msg->next);

        /* there could be no unvisited message be found */
        if(msg == NULL) return msg->next;

        /* ok, unvisited message could be found */
        return msg;
      }
      else {
        return msg->next;
      }
    }
  }
  else {
    if(is_visited && NoVisited) {
      /* go to the last subtree */
      if(msg == thr->messages) msg = thr->messages->next;
      if(msg == NULL) return NULL;

      for(tmp=msg;(tmp1 = next_subtree(tmp)) != NULL;tmp=tmp1);
      printf("we went to the last subtree (%llu)\n",tmp->mid);

      /* there wa are. now, check for each subtree if there is an unread message */
      for(tmp1=tmp;(tmp1 = prev_subtree(tmp)) != NULL;tmp=tmp1) {
        for(lvl = tmp->level,first=1;tmp && (tmp->level > lvl || first);tmp=tmp->next) {
          first = 0;
          if(is_visited(&(tmp->mid)) == NULL) return tmp;
        }
      }

      /* out of the loop: no unvisited message could be found */
      printf("we are out of it\n");
      /** \todo check for last message in the thread (we have to jump to the previous subtree) */
      //for(tmp=msg;(tmp1 = next_subtree(tmp)) != NULL;tmp=tmp1);
      tmp = prev_subtree(msg);

      /* ok, we are at the next subtree -- are we? */
      if(tmp == msg) return msg->next;
      return tmp;
    }
    else {
      for(tmp=msg;(tmp1 = next_subtree(tmp)) != NULL;tmp=tmp1);

      /* ok, we are at the next subtree -- are we? */
      if(tmp == msg) return msg->next;
      return msg;
    }
  }

  return NULL;
}

t_message *flt_link_get_last(t_cl_thread *thread) {
  t_message *msg;

  for(msg=thread->last;msg && (msg->invisible == 1 || msg->may_show == 0);msg=msg->prev);
  return msg;
}

int flt_link_set_links_post(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_cl_thread *thread,t_cf_template *tpl) {
  u_char *aaf = cf_cgi_get(head,"aaf");
  t_message *msg;
  size_t n;
  u_char buff[256];

  /* use doesn't want <link> tags */
  if(SetLinks == 0) return FLT_DECLINE;

  /* ok, we have to find the previous message */
  if((msg = flt_link_get_previous(thread->threadmsg)) != NULL) {
    if(aaf) n = snprintf(buff,256,"?t=%llu&m=%llu&aaf=%s",thread->tid,msg->mid,aaf);
    else    n = snprintf(buff,256,"?t=%llu&m=%llu",thread->tid,msg->mid);

    tpl_cf_setvar(tpl,"prev",buff,n,1);
  }

  /* next message... */
  if((msg = flt_link_get_next(thread,thread->threadmsg)) != NULL) {
    if(aaf) n = snprintf(buff,256,"?t=%lld&m=%lld&aaf=%s",thread->tid,msg->mid,aaf);
    else    n = snprintf(buff,256,"?t=%lld&m=%lld",thread->tid,msg->mid);

    tpl_cf_setvar(tpl,"next",buff,n,1);
  }

  if(aaf) n = snprintf(buff,256,"?t=%lld&m=%lld&aaf=%s",thread->tid,thread->messages->mid,aaf);
  else    n = snprintf(buff,256,"?t=%lld&m=%lld",thread->tid,thread->messages->mid);
  tpl_cf_setvar(tpl,"first",buff,n,1);

  /* last message... */
  if((msg = flt_link_get_last(thread)) != NULL) {
    if(aaf) n = snprintf(buff,256,"?t=%lld&m=%lld&aaf=%s",thread->tid,msg->mid,aaf);
    else    n = snprintf(buff,256,"?t=%lld&m=%lld",thread->tid,msg->mid);

    tpl_cf_setvar(tpl,"last",buff,n,1);
  }
  
  return FLT_OK;
}

int flt_link_handle_conf(t_configfile *cfg,t_conf_opt *entry,u_char **args,int argnum) {
  if(argnum == 1) {
    if(*entry->name == 'S')                              SetLinks    = cf_strcmp(args[0],"yes") == 0;
    else if(cf_strcmp(entry->name,"LinkNoVisited") == 0) NoVisited   = cf_strcmp(args[0],"yes") == 0;
    else {
      if(cf_strcmp(args[0],"OldestFirst") == 0) LinkOrder  = LINK_OLDEST_FIRST;
      else                                      LinkOrder  = LINK_NEWEST_FIRST;
    }
  }
  else {
    fprintf(stderr,"Error: expecting 1 argument for directive SetLinkTags!\n");
    return 1;
  }

  return 0;
}

t_conf_opt flt_link_config[] = {
  { "SetLinkTags",     flt_link_handle_conf,   NULL },
  { "LinkNoVisited",   flt_link_handle_conf,   NULL },
  { "LinkOrder",       flt_link_handle_conf,   NULL },
  { NULL, NULL, NULL }
};

t_handler_config flt_link_handlers[] = {
  { POSTING_HANDLER,   flt_link_set_links_post },
  { 0, NULL }
};

t_module_config flt_link = {
  flt_link_config,
  flt_link_handlers,
  NULL,
  NULL,
  NULL,
  NULL
};

/* eof */
