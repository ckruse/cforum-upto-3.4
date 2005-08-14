/**
 * \file userconf.h
 * \author Christian Kruse, <ckruse@wwwtech.de>
 *
 * Userconfig utilities library
 */

#ifndef _CF_USERCONF_H_
#define _CF_USERCONF_H_

#define CF_UCONF_FLAG_INVISIBLE 0x01

typedef struct {
  t_array directives;
} uconf_userconfig_t;

typedef struct {
  u_char *name;
  u_char *access;

  int flags;
  int argnum;

  t_array arguments;
} uconf_directive_t;

typedef struct {
  u_char *param;
  u_char *ifnotcommitted;
  u_char *deflt;
  u_char *parse;
  u_char *val;

  int validation_type;
  u_char *validation;

  u_char *error;
} uconf_argument_t;

uconf_userconfig_t *cf_uconf_read_modxml();

void cf_uconf_destroy_argument(uconf_argument_t *argument);
void cf_uconf_destroy_directive(uconf_directive_t *dir);
void cf_uconf_cleanup_modxml(uconf_userconfig_t *modxml);
void cf_uconf_to_html(t_string *str);

#endif

/* eof */
