/**
 * \file messageutils.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 * \brief message handling utilities
 *
 * This file contains utility functions for handling messages
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <sys/types.h>

#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "template.h"
#include "readline.h"
#include "charconvert.h"
#include "clientlib.h"
/* }}} */


/* {{{ cf_msg_get_first_visible */
message_t *cf_msg_get_first_visible(message_t *msg) {
  register message_t *msg1;

  if(msg->may_show && msg->invisible == 0) return msg;
  for(msg1=msg;msg1;msg1=msg1->next) {
    if(msg1->may_show && msg1->invisible == 0) return msg1;
  }

  return msg;
}
/* }}} */

/* {{{ cf_msg_ht_get_first_visible */
hierarchical_node_t *cf_msg_ht_get_first_visible(hierarchical_node_t *msg) {
  size_t i;
  hierarchical_node_t *val;

  for(i=0;i<msg->childs.elements;++i) {
    val = array_element_at(&msg->childs,i);

    if(val->msg->invisible == 0 && val->msg->may_show) return val;

    if(val->childs.elements && (val = cf_msg_ht_get_first_visible(val)) != NULL) return val;
  }

  return NULL;
}
/* }}} */

/* {{{ cf_msg_filter_invisible */
void _cf_msg_filter_invisible(hierarchical_node_t *ht,hierarchical_node_t *ary,int si) {
  size_t i;
  hierarchical_node_t *ht1,ary1;

  for(i=0;i<ht->childs.elements;++i) {
    ht1 = array_element_at(&ht->childs,i);

    if(si || (ht1->msg->invisible == 0 && ht1->msg->may_show)) {
      array_init(&ary1.childs,sizeof(ary1),NULL);
      ary1.msg = ht1->msg;
      _cf_msg_filter_invisible(ht1,&ary1,si);
      array_push(&ary->childs,&ary1);
    }
    else _cf_msg_filter_invisible(ht1,ary,si);
  }
}
/* }}} */

void cf_msg_filter_invisible(hierarchical_node_t *ht,hierarchical_node_t *ary,int si) {
  array_init(&ary->childs,sizeof(*ary),NULL);

  if(si || (ht->msg->invisible == 0 && ht->msg->may_show)) ary->msg = ht->msg;
  else ary->msg = NULL;

  _cf_msg_filter_invisible(ht,ary,si);
}

/* {{{ cf_msg_delete_subtree */
message_t *cf_msg_delete_subtree(message_t *msg) {
  int lvl = msg->level;

  for(msg=msg->next;msg && msg->level > lvl;msg=msg->next) msg->may_show = 0;

  return msg;
}
/* }}} */

/* {{{ cf_msg_next_subtree */
message_t *cf_msg_next_subtree(message_t *msg) {
  int lvl = msg->level;

  for(msg=msg->next;msg && msg->level > lvl;msg=msg->next);

  return msg;
}
/* }}} */

/* {{{ cf_msg_prev_subtree */
message_t *cf_msg_prev_subtree(message_t *msg) {
  int lvl = msg->level;

  for(msg=msg->prev;msg && msg->level > lvl;msg=msg->prev);

  return msg;
}
/* }}} */

/* {{{ cf_msg_get_parent */
message_t *cf_msg_get_parent(message_t *tmsg) {
  message_t *msg = NULL;

  for(msg=tmsg;msg;msg=msg->prev) {
    if(msg->level == tmsg->level - 1) return msg;
  }

  return NULL;
}
/* }}} */

/* {{{ cf_msg_has_answers */
int cf_msg_has_answers(message_t *msg) {
  int lvl = msg->level;
  int ShowInvisible = cf_hash_get(GlobalValues,"ShowInvisible",13) == NULL ? 0 : 1;

  if(msg->next) {
    if((msg->next->may_show && msg->next->invisible == 0) || ShowInvisible == 1) {
      if(msg->next->level > msg->level) {
        return 1;
      }
      return 0;
    }
    else {
      for(msg=msg->next;msg && (msg->may_show == 0 || msg->invisible == 1) && ShowInvisible == 0;msg=msg->next);

      if(!msg) {
        return 0;
      }
      else {
        if((msg->invisible == 1 || msg->may_show == 0) && ShowInvisible == 0) return 0;

        if(msg->level > lvl) {
          return 1;
        }

        return 0;
      }
    }
  }
  else return 0;
}
/* }}} */

/* {{{ cf_destroy_flag */
void cf_destroy_flag(void *data) {
  cf_post_flag_t *flag = (cf_post_flag_t *)data;

  free(flag->name);
  free(flag->val);
}
/* }}} */

/* {{{ _cf_cleanup_hierarchical */
void _cf_cleanup_hierarchical(hierarchical_node_t *n) {
  size_t i;
  hierarchical_node_t *tmp;

  for(i=0;i<n->childs.elements;++i) {
    tmp = array_element_at(&n->childs,i);
    _cf_cleanup_hierarchical(tmp);
  }

  array_destroy(&n->childs);
}
/* }}} */

/* {{{ cf_cleanup_message */
void cf_cleanup_message(message_t *msg) {
  str_cleanup(&msg->author);
  str_cleanup(&msg->subject);

  if(msg->category.len) str_cleanup(&msg->category);
  if(msg->content.len) str_cleanup(&msg->content);
  if(msg->email.len) str_cleanup(&msg->email);
  if(msg->hp.len) str_cleanup(&msg->hp);
  if(msg->img.len) str_cleanup(&msg->img);

  if(msg->flags.elements) cf_list_destroy(&msg->flags,cf_destroy_flag);
}
/* }}} */

/* {{{ cf_cleanup_thread */
void cf_cleanup_thread(cl_thread_t *thr) {
  message_t *msg = thr->messages,*last = thr->messages;

  _cf_cleanup_hierarchical(thr->ht);
  free(thr->ht);

  for(;msg;msg=last) {
    last = msg->next;

    cf_cleanup_message(msg);
    free(msg);
  }
}
/* }}} */

/* {{{ cf_msg_build_hierarchical_structure */
message_t *cf_msg_build_hierarchical_structure(hierarchical_node_t *parent,message_t *msg) {
  message_t *m;
  int lvl;
  hierarchical_node_t h;

  m   = msg;
  lvl = m->level;

  while(m) {
    if(m->level == lvl) {
      if(!parent->childs.element_size) array_init(&parent->childs,sizeof(*parent),NULL);

      memset(&h,0,sizeof(h));

      h.msg = m;
      array_push(&parent->childs,&h);
      m     = m->next;
    }
    else if(m->level > lvl) m = cf_msg_build_hierarchical_structure(array_element_at(&parent->childs,parent->childs.elements-1),m);
    else return m;
  }

  return NULL;
}
/* }}} */

/* {{{ cf_msg_do_linearize */
message_t *cf_msg_do_linearize(hierarchical_node_t *node) {
  size_t i;
  message_t *p;
  hierarchical_node_t *tmp,*tmp1;

  if(node->childs.elements) {
    tmp = array_element_at(&node->childs,0);
    node->msg->next = tmp->msg;

    for(i=0;i<node->childs.elements;++i) {
      tmp = array_element_at(&node->childs,i);

      if(tmp->childs.elements) {
        p = cf_msg_do_linearize(tmp);

        if(i < node->childs.elements-1) {
          tmp1 = array_element_at(&node->childs,i+1);
          p->next = tmp1->msg;
        }
        else {
          p->next = NULL;
          return p;
        }
      }
      else {
        if(i < node->childs.elements-1) {
          tmp1 = array_element_at(&node->childs,i+1);
          tmp->msg->next = tmp1->msg;
        }
        else {
          tmp->msg->next = NULL;
          return tmp->msg;
        }
      }
    }
  }
  else {
    node->msg->next = NULL;
    return node->msg;
  }

  return NULL;
}
/* }}} */

/* {{{ cf_msg_linearize */
void cf_msg_linearize(cl_thread_t *thr,hierarchical_node_t *h) {
  message_t *p,*p1;

  cf_msg_do_linearize(h);

  /* reset prev-pointers */
  for(p=h->msg,p1=NULL;p;p=p->next) {
    p->prev = p1;
    p1      = p;
  }

  thr->last = p1;
}
/* }}} */


/* eof */
