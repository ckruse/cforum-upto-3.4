/**
 * \file htmllib.c
 * \author Christian Kruse
 *
 * This library contains some functions to display messages in HTML
 */

/* {{{ Initial comments */
/*
 * $LastChangedDate$
 * $LastChangedRevision$
 * $LastChangedBy$
 *
 */
/* }}} */


#ifndef _CF_HTMLLIB_H
#define _CF_HTMLLIB_H

typedef int (*cf_directive_filter_t)(cf_cfg_config_t *cfg,cf_cl_thread_t *thr,const u_char *directive,const u_char **parameters,size_t plen,cf_string_t *bcnt,cf_string_t *bcite,cf_string_t *content,cf_string_t *cite,const u_char *qchars,int sig);
typedef int (*cf_content_filter_t)(cf_cfg_config_t *cfg,cf_cl_thread_t *thr,cf_string_t *content,cf_string_t *cite,const u_char *qchars);

typedef int (*cf_directive_validator_t)(cf_cfg_config_t *cfg,const u_char *directive,const u_char **parameters,size_t plen,cf_tpl_variable_t *var);

#define CF_HTML_DIR_TYPE_NOARG  1
#define CF_HTML_DIR_TYPE_ARG    2
#define CF_HTML_DIR_TYPE_INLINE 4
#define CF_HTML_DIR_TYPE_BLOCK  8

void cf_htmllib_init(void);

int cf_html_register_directive(const u_char *name,cf_directive_filter_t filter,int type);
int cf_html_register_validator(const u_char *name,cf_directive_validator_t filter,int type);
int cf_html_register_textfilter(const u_char *text,cf_directive_filter_t filter);
void cf_msg_to_html(cf_cfg_config_t *cfg,cf_cl_thread_t *thread,const u_char *msg,cf_string_t *content,cf_string_t *cite,u_char *quote_chars,int max_sig_lines,int show_sig);
int cf_validate_msg(cf_cfg_config_t *cfg,cf_cl_thread_t *thread,const u_char *msg,cf_tpl_variable_t *var);

int cf_gen_threadlist(cf_cfg_config_t *cfg,cf_cl_thread_t *thread,cf_hash_t *head,cf_string_t *threadlist,const u_char *tplname,const u_char *type,const u_char *linktpl,int mode);

#endif

/* eof */
