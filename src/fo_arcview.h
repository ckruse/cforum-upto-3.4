/**
 * \file fo_arcview.h
 * \author Christian Kruse, <cjk@wwwtech.de>
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

#ifndef _CF_ARCVIEW_H
#define _CF_ARCVIEW_H

extern cf_hash_t *ArcviewHandlers;

typedef int (*cf_is_valid_year_t)(cf_cfg_config_t *,const u_char *);
typedef int (*cf_is_valid_month_t)(cf_cfg_config_t *,const u_char *,const u_char *);
typedef int (*cf_is_valid_thread_t)(cf_cfg_config_t *,const u_char *,const u_char *,const u_char *);

typedef cf_array_t *(*cf_get_years_t)(cf_cfg_config_t *);
typedef cf_array_t *(*cf_get_monthlist_t)(cf_cfg_config_t *,const u_char *);
typedef cf_array_t *(*cf_get_threadlist_t)(cf_cfg_config_t *,const u_char *,const u_char *);
typedef cf_cl_thread_t *(*cf_get_thread_t)(cf_cfg_config_t *,const u_char *,const u_char *,const u_char *);

typedef time_t (*cf_month_last_modified_t)(cf_cfg_config_t *,const u_char *,const u_char *);
typedef time_t (*cf_thread_last_modified_t)(cf_cfg_config_t *,const u_char *,const u_char *,const u_char *);

typedef struct {
  int invisible;

  u_char *author;
  size_t alen;

  u_char *cat;
  size_t clen;

  u_char *subject;
  size_t slen;

  u_char *tid;
  size_t tlen;

  time_t date;
} cf_arc_tl_ent_t;


#endif
