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

static int flt_link_set_links    = 0;
static int flt_link_no_visited   = 0;

t_message *flt_link_get_previous(t_message *msg) {
  t_mod_api is_visited;
  t_message *tmp;

  if(msg->prev) {
    is_visited = cf_get_mod_api_ent("is_visited");
    if(is_visited && flt_link_no_visited) {
      for(tmp=msg->prev;tmp && (tmp->invisible == 1 || tmp->may_show == 0 || is_visited(&(tmp->mid)) != NULL);tmp=tmp->prev);
    }

    /* either user wants also unvisited or API failure or no unvisited message could be found */
    if(!is_visited || !flt_link_no_visited || !tmp) {
      for(tmp=msg->prev;tmp && (tmp->invisible == 1 || tmp->may_show == 0);tmp=tmp->prev);
    }

    return tmp;
  }

  return NULL;
}


t_message *flt_link_get_next(t_message *msg) {
  t_message *tmp;
  t_mod_api is_visited = cf_get_mod_api_ent("is_visited");

  if(msg->next) {
    if(is_visited && flt_link_no_visited) {
      for(tmp=msg->next;tmp && (tmp->invisible == 1 || tmp->may_show == 0 || is_visited(&(tmp->mid)) != NULL);tmp=tmp->next);
      if(tmp) return tmp;
    }

    for(tmp=msg->next;tmp && (tmp->invisible == 1 || tmp->may_show == 0);tmp=tmp->next);
    return tmp;
  }

  return NULL;
}

t_message *flt_link_get_last(t_cl_thread *thread) {
  t_message *msg = NULL;
  thread->messages->prev = NULL;

  for(msg=thread->last;msg && (msg->invisible == 1 || msg->may_show == 0);msg=msg->prev);
  return msg;
}

void my_getlink(t_string *str,u_int64_t tid,u_int64_t mid,const u_char *aaf,const u_char aafval[]) {
  str->content  = get_link(NULL,tid,mid);
  str->reserved = str->len = strlen(str->content);
  str->reserved += 1;

  if(aaf) {
    str_chars_append(str,aafval,5);
    str_char_append(str,*aaf);
  }
}

int flt_link_set_links_post(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_cl_thread *thread,t_cf_template *tpl) {
  u_char *aaf = cf_cgi_get(head,"aaf");
  t_message *msg;
  size_t n;
  u_char *buff;
  t_string str;
  t_name_value *qtype = cfg_get_first_value(&fo_view_conf,NULL,"ParamType");
  u_char *aafval = strdup(*qtype->values[0] == 'Q' ? "&aaf=" : "?aaf=");

  /* user doesn't want <link> tags */
  if(flt_link_set_links == 0) return FLT_DECLINE;

  str_init(&str);

  /* ok, we have to find the previous message */
  if((msg = flt_link_get_previous(thread->threadmsg)) != NULL) {
    my_getlink(&str,thread->tid,msg->mid,aaf,aafval);
    tpl_cf_setvar(tpl,"prev",str.content,str.len,1);
    str_cleanup(&str);
  }

  /* next message... */
  if((msg = flt_link_get_next(thread->threadmsg)) != NULL) {
    my_getlink(&str,thread->tid,msg->mid,aaf,aafval);
    tpl_cf_setvar(tpl,"next",str.content,str.len,1);
    str_cleanup(&str);
  }

  my_getlink(&str,thread->tid,thread->messages->mid,aaf,aafval);
  tpl_cf_setvar(tpl,"first",str.content,str.len,1);
  str_cleanup(&str);

  /* last message... */
  if((msg = flt_link_get_last(thread)) != NULL) {
    my_getlink(&str,thread->tid,msg->mid,aaf,aafval);
    tpl_cf_setvar(tpl,"last",str.content,str.len,1);
    str_cleanup(&str);
  }
  
  return FLT_OK;
}

int flt_link_handle_conf(t_configfile *cfg,t_conf_opt *entry,const u_char *context,u_char **args,size_t argnum) {
  if(argnum == 1) {
    if(*entry->name == 'S')                              flt_link_set_links    = cf_strcmp(args[0],"yes") == 0;
    else if(cf_strcmp(entry->name,"LinkNoVisited") == 0) flt_link_no_visited   = cf_strcmp(args[0],"yes") == 0;
  }
  else {
    fprintf(stderr,"Error: expecting 1 argument for directive SetLinkTags!\n");
    return 1;
  }

  return 0;
}

t_conf_opt flt_link_config[] = {
  { "SetLinkTags",     flt_link_handle_conf,  CFG_OPT_CONFIG|CFG_OPT_USER,  NULL },
  { "LinkNoVisited",   flt_link_handle_conf,  CFG_OPT_CONFIG|CFG_OPT_USER,  NULL },
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
