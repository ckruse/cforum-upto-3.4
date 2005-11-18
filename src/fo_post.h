/*
 * \file fo_post.h
 * \author Christian Kruse, <ckruse@wwwtech.de>
 * \brief Header defitions of fo_post
 *
 * This file defines some things for fo_post
 */

#ifndef _CF_POST_H
#define _CF_POST_H

void display_posting_form(cf_cfg_config_t *cfg,cf_hash_t *head,cf_message_t *p,cf_tpl_variable_t *var);
cf_string_t *body_plain2coded(const u_char *qchars,const u_char *text);

#endif

/* eof */
