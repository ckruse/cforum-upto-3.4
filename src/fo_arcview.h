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

#ifndef _CF_ARCVIEW_H
#define _CF_ARCVIEW_H

extern cf_hash_t *ArcviewHandlers;

typedef int (*is_valid_year_t)(const u_char *);
typedef int (*is_valid_month_t)(const u_char *,const u_char *);
typedef int (*is_valid_thread_t)(const u_char *,const u_char *,const u_char *);

typedef cf_array_t *(*get_years_t)(void);
typedef cf_array_t *(*get_monthlist_t)(const u_char *);
typedef cf_array_t *(*get_threadlist_t)(const u_char *,const u_char *);
typedef cl_thread_t *(*get_thread_t)(const u_char *,const u_char *,const u_char *);

typedef time_t (*month_last_modified_t)(const u_char *,const u_char *);
typedef time_t (*thread_last_modified_t)(const u_char *,const u_char *,const u_char *);

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
} arc_tl_ent_t;


#endif
