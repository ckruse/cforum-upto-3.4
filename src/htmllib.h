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

typedef int (*directive_filter_t)(configuration_t *dc,configuration_t *vc,cl_thread_t *thr,const u_char *directive,const u_char **parameters,size_t plen,string_t *bcnt,string_t *bcite,string_t *content,string_t *cite,const u_char *qchars,int sig);
typedef int (*content_filter_t)(configuration_t *dc,configuration_t *vc,cl_thread_t *thr,string_t *content,string_t *cite,const u_char *qchars);

typedef int (*directive_validator_t)(configuration_t *dc,configuration_t *vc,const u_char *directive,const u_char **parameters,size_t plen,cf_tpl_variable_t *var);

#define CF_HTML_DIR_TYPE_NOARG  1
#define CF_HTML_DIR_TYPE_ARG    2
#define CF_HTML_DIR_TYPE_INLINE 4
#define CF_HTML_DIR_TYPE_BLOCK  8

void cf_htmllib_init(void);

int cf_html_register_directive(const u_char *name,directive_filter_t filter,int type);
int cf_html_register_validator(const u_char *name,directive_validator_t filter,int type);
int cf_html_register_textfilter(const u_char *text,directive_filter_t filter);
void msg_to_html(cl_thread_t *thread,const u_char *msg,string_t *content,string_t *cite,u_char *quote_chars,int max_sig_lines,int show_sig);
int cf_validate_msg(cl_thread_t *thread,const u_char *msg,cf_tpl_variable_t *var);

int cf_gen_threadlist(cl_thread_t *thread,cf_hash_t *head,string_t *threadlist,const u_char *tplname,const u_char *type,const u_char *linktpl,int mode);

#endif

/* eof */
