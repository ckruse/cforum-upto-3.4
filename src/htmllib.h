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

typedef int (*t_directive_filter)(t_configuration *dc,t_configuration *vc,const u_char *directive,const u_char *parameter,t_string *cnt,t_string *cite,const u_char *qchars,int sig);
typedef int (*t_content_filter)(t_configuration *dc,t_configuration *vc,t_cl_thread *thr,t_string *content,t_string *cite,const u_char *qchars);

void msg_to_html(t_cl_thread *thread,const u_char *msg,t_string *content,t_string *cite,u_char *quote_chars,int max_sig_lines,int show_sig);

#endif

/* eof */
