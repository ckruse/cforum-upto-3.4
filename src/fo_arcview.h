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

extern t_cf_hash *ArcviewHandlers;

typedef int (*t_is_valid_year)(const u_char *);
typedef int (*t_is_valid_month)(const u_char *,const u_char *);
typedef int (*t_is_valid_thread)(const u_char *,const u_char *,const u_char *);

typedef t_array *(*t_get_years)(void);
typedef t_array *(*t_get_monthlist)(const u_char *);
typedef t_array *(*t_get_threadlist)(const u_char *,const u_char *);
typedef t_cl_thread *(*t_get_thread)(const u_char *,const u_char *,const u_char *);

typedef time_t (*t_month_last_modified)(const u_char *,const u_char *);
typedef time_t (*t_thread_last_modified)(const u_char *,const u_char *,const u_char *);

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
} t_arc_tl_ent;


#endif
