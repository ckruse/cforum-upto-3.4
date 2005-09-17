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
  array_t directives;
} uconf_userconfig_t;

typedef struct {
  u_char *name;
  u_char *access;

  int flags;
  int argnum;

  array_t arguments;
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
void cf_uconf_to_html(string_t *str);

uconf_userconfig_t *cf_uconf_merge_config(cf_hash_t *head,configuration_t *config,array_t *errormessages,int touch_committed);
u_char *cf_write_uconf(const u_char *filename,uconf_userconfig_t *merged);

#endif

/* eof */
