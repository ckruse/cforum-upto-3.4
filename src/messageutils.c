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

#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "template.h"
#include "readline.h"
#include "charconvert.h"
#include "clientlib.h"
/* }}} */


/* {{{ cf_msg_get_first_visible */
t_message *cf_msg_get_first_visible(t_message *msg) {
  register t_message *msg1;

  if(msg->may_show && msg->invisible == 0) return msg;
  for(msg1=msg;msg1;msg1=msg1->next) {
    if(msg1->may_show && msg1->invisible == 0) return msg1;
  }

  return msg;
}
/* }}} */

/* {{{ cf_msg_delete_subtree */
t_message *cf_msg_delete_subtree(t_message *msg) {
  int lvl = msg->level;

  for(msg=msg->next;msg && msg->level > lvl;msg=msg->next) msg->may_show = 0;

  return msg;
}
/* }}} */

/* {{{ cf_msg_next_subtree */
t_message *cf_msg_next_subtree(t_message *msg) {
  int lvl = msg->level;

  for(msg=msg->next;msg && msg->level > lvl;msg=msg->next);

  return msg;
}
/* }}} */

/* {{{ cf_msg_prev_subtree */
t_message *cf_msg_prev_subtree(t_message *msg) {
  int lvl = msg->level;

  for(msg=msg->prev;msg && msg->level > lvl;msg=msg->prev);

  return msg;
}
/* }}} */

/* {{{ cf_msg_get_parent */
t_message *cf_msg_get_parent(t_message *tmsg) {
  t_message *msg = NULL;

  for(msg=tmsg;msg;msg=msg->prev) {
    if(msg->level == tmsg->level - 1) return msg;
  }

  return NULL;
}
/* }}} */

/* {{{ cf_msg_has_answers */
int cf_msg_has_answers(t_message *msg) {
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
  t_cf_post_flag *flag = (t_cf_post_flag *)data;

  free(flag->name);
  free(flag->val);
}
/* }}} */

/* {{{ cf_cleanup_message */
void cf_cleanup_message(t_message *msg) {
  str_cleanup(&msg->author);
  str_cleanup(&msg->subject);

  if(msg->category.len) str_cleanup(&msg->category);
  if(msg->content.len) str_cleanup(&msg->content);
  if(msg->email.len) str_cleanup(&msg->content);
  if(msg->hp.len) str_cleanup(&msg->hp);
  if(msg->img.len) str_cleanup(&msg->img);

  if(msg->tpl.tpl) tpl_cf_finish(&msg->tpl);

  if(msg->flags.elements) cf_list_destroy(&msg->flags,cf_destroy_flag);
}
/* }}} */

/* {{{ cf_cleanup_thread */
void cf_cleanup_thread(t_cl_thread *thr) {
  t_message *msg = thr->messages,*last = thr->messages;

  for(;msg;msg=last) {
    last = msg->next;

    cf_cleanup_message(msg);
    free(msg);
  }

}
/* }}} */


/* eof */
