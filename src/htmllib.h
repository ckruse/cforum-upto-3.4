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

typedef int (*t_directive_filter)(t_configuration *dc,t_configuration *vc,t_cl_thread *thr,const u_char *directive,const u_char **parameters,size_t plen,t_string *bcnt,t_string *bcite,t_string *content,t_string *cite,const u_char *qchars,int sig);
typedef int (*t_content_filter)(t_configuration *dc,t_configuration *vc,t_cl_thread *thr,t_string *content,t_string *cite,const u_char *qchars);

typedef int (*t_directive_validator)(t_configuration *dc,t_configuration *vc,const u_char *directive,const u_char **parameters,size_t plen,t_cf_tpl_variable *var);

#define CF_HTML_DIR_TYPE_NOARG  1
#define CF_HTML_DIR_TYPE_ARG    2
#define CF_HTML_DIR_TYPE_INLINE 4
#define CF_HTML_DIR_TYPE_BLOCK  8

int cf_html_register_directive(u_char *name,t_directive_filter filter,int type);
int cf_html_register_validator(u_char *name,t_directive_validator filter,int type);
void msg_to_html(t_cl_thread *thread,const u_char *msg,t_string *content,t_string *cite,u_char *quote_chars,int max_sig_lines,int show_sig);
int cf_validate_msg(t_cl_thread *thread,const u_char *msg,t_cf_tpl_variable *var);

#endif

/* eof */
