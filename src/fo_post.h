/*
 * \file fo_post.h
 * \author Christian Kruse, <ckruse@wwwtech.de>
 * \brief Header defitions of fo_post
 *
 * This file defines some things for fo_post
 */

#ifndef _FO_POST_H
#define _FO_POST_H

void display_posting_form(t_cf_hash *head,t_message *p,t_cf_tpl_variable *var);
t_string *body_plain2coded(const u_char *text);

#endif

/* eof */
