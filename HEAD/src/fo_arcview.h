/**
 * \file fo_arcview.h
 * \author Christian Kruse, <ckruse@wwwtech.de>
 * \brief The forum archive viewer program header file
 */

/* {{{ Initial comment */
/*
 * $LastChangedDate$
 * $LastChangedRevision$
 * $LastChangedBy$
 *
 */
/* }}} */

#ifndef _FO_ARCVIEW_H
#define _FO_ARCVIEW_H

/**
 * Struct for message representation of the archive viewer. It's hierarchical
 */
typedef struct s_arc_message {
  u_int64_t mid;     /**< message id */
  t_string author;   /**< author name */
  t_string subject;  /**< subject of the post */
  t_string category; /**< category of the post */
  t_string content;  /**< content of the post */
  t_string email;    /**< email address of the author */
  t_string hp;       /**< homepage address of the author */
  t_string img;      /**< image url of the author */

  time_t date;       /**< date of the posting */
  int may_show;      /**< boolean for plugins (if 0, the message will be deleted) */
  int invisible;     /**< boolean, if 1 the message will be deleted */

  t_cf_template tpl; /**< template of this posting */

  t_array childs;    /**< array of answers to this posting */
} t_arc_message;

/**
 * Struct for thread representation of the archive viewer
 */
typedef struct s_arc_thread {
  u_int64_t tid;        /**< thread id */
  long msg_len;         /**< number of messages */

  t_arc_message *msgs;  /**< pointer to the thread message */
} t_arc_thread;


#endif
