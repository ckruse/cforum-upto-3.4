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

static u_char *flt_link_fn = NULL;

/* {{{ flt_link_get_previous */
t_message *flt_link_get_previous(t_message *msg) {
  t_mod_api is_visited;
  t_message *tmp = NULL;

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
/* }}} */

/* {{{ flt_link_get_next */
t_message *flt_link_get_next(t_message *msg) {
  t_message *tmp;
  t_mod_api is_visited = cf_get_mod_api_ent("is_visited");

  if(msg->next) {
    if(is_visited && NoVisited) {
      for(tmp=msg->next;tmp && (tmp->invisible == 1 || tmp->may_show == 0 || is_visited(&(tmp->mid)) != NULL);tmp=tmp->next);
      if(tmp) return tmp;
    }

    for(tmp=msg->next;tmp && (tmp->invisible == 1 || tmp->may_show == 0);tmp=tmp->next);
    return tmp;
  }

  return NULL;
}
/* }}} */

/* {{{ flt_link_get_last */
t_message *flt_link_get_last(t_cl_thread *thread) {
  t_message *msg = NULL;
  thread->messages->prev = NULL;

  for(msg=thread->last;msg && (msg->invisible == 1 || msg->may_show == 0);msg=msg->prev);
  return msg;
}
/* }}} */

/* {{{ flt_link_getlink */
void flt_link_getlink(t_string *str,u_int64_t tid,u_int64_t mid,u_char *forum_name) {
  str->content  = cf_get_link(NULL,forum_name,tid,mid);
  str->reserved = str->len = strlen(str->content);
  str->reserved += 1;
}
/* }}} */

/* {{{ flt_link_set_links_post */
int flt_link_set_links_post(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_cl_thread *thread,t_cf_template *tpl) {
  u_char *forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  t_message *msg;
  size_t n;
  u_char *buff;
  t_string str;
  t_name_value *qtype = cfg_get_first_value(&fo_view_conf,forum_name,"ParamType"),
    *cs = cfg_get_first_value(&fo_default_conf,forum_name,"ExternCharset"),
    *rm = cfg_get_first_value(vc,forum_name,"ReadMode");

  /* user doesn't want <link> tags */
  if(SetLinks == 0 || cf_strcmp(rm->values[0],"thread") != 0) return FLT_DECLINE;

  str_init(&str);

  /* ok, we have to find the previous message */
  if((msg = flt_link_get_previous(thread->threadmsg)) != NULL) {
    flt_link_getlink(&str,thread->tid,msg->mid,forum_name);
    cf_set_variable(tpl,cs,"prev",str.content,str.len,1);
    str_cleanup(&str);
  }

  /* next message... */
  if((msg = flt_link_get_next(thread->threadmsg)) != NULL) {
    flt_link_getlink(&str,thread->tid,msg->mid,forum_name);
    cf_set_variable(tpl,cs,"next",str.content,str.len,1);
    str_cleanup(&str);
  }

  flt_link_getlink(&str,thread->tid,thread->messages->mid,forum_name);
  cf_set_variable(tpl,cs,"first",str.content,str.len,1);
  str_cleanup(&str);

  /* link rel="up" */
  for(msg=thread->threadmsg;msg && msg->level >= thread->threadmsg->level;msg=msg->prev);
  if(msg) {
    flt_link_getlink(&str,thread->tid,msg->mid,forum_name);
    cf_set_variable(tpl,cs,"up",str.content,str.len,1);
    str_cleanup(&str);
  }

  /* last message... */
  if((msg = flt_link_get_last(thread)) != NULL) {
    flt_link_getlink(&str,thread->tid,msg->mid,forum_name);
    cf_set_variable(tpl,cs,"last",str.content,str.len,1);
    str_cleanup(&str);
  }
  
  return FLT_OK;
}
/* }}} */

/* {{{ flt_link_handle_conf */
int flt_link_handle_conf(t_configfile *cfg,t_conf_opt *entry,const u_char *context,u_char **args,size_t argnum) {
  if(flt_link_fn == NULL) flt_link_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(flt_link_fn,context) != 0) return 0;

  if(argnum == 1) {
    if(*entry->name == 'S')                              SetLinks    = cf_strcmp(args[0],"yes") == 0;
    else if(cf_strcmp(entry->name,"LinkNoVisited") == 0) NoVisited   = cf_strcmp(args[0],"yes") == 0;
  }
  else {
    fprintf(stderr,"Error: expecting 1 argument for directive SetLinkTags!\n");
    return 1;
  }

  return 0;
}
/* }}} */

t_conf_opt flt_link_config[] = {
  { "SetLinkTags",     flt_link_handle_conf,  CFG_OPT_CONFIG|CFG_OPT_USER|CFG_OPT_LOCAL,  NULL },
  { "LinkNoVisited",   flt_link_handle_conf,  CFG_OPT_CONFIG|CFG_OPT_USER|CFG_OPT_LOCAL,  NULL },
  { NULL, NULL, 0, NULL }
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
