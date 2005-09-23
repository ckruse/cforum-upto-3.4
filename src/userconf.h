/**
 * \file userconf.h
 * \author Christian Kruse, <ckruse@wwwtech.de>
 *
 * Userconfig utilities library
 */

#ifndef _CF_USERCONF_H_
#define _CF_USERCONF_H_

#define CF_UCONF_FLAG_INVISIBLE 0x01

/** This structure contains the user configuration */
typedef struct {
  array_t directives; /**< This is the array of directives */
} uconf_userconfig_t;

/** This structure contains a directive with all information of the modules.de.xml */
typedef struct {
  u_char *name; /**< The name of the directive */
  u_char *access; /**< */

  int flags; /**< This variable contains the flags for this directive, i.e. CF_UCONF_FLAG_INVISIBLE */
  int argnum; /**< The number of arguments of this directive */

  array_t arguments; /**< The arguments of this directive */
} uconf_directive_t;

/** This struct defines the arguments */
typedef struct {
  u_char *param; /**< The name of the CGI param */
  u_char *ifnotcommitted; /**< The value which should be set if it was not committed */
  u_char *deflt; /**< The default value */
  u_char *parse; /**< The parse type (i.e. date) */
  u_char *val; /**< The value */

  int validation_type; /**< The validation type; if != 0, should be validated by type */
  u_char *validation; /**< The validation regex or the validation type */

  u_char *error; /**< The error message string */
} uconf_argument_t;

typedef struct {
  u_char *directive;
  u_char *param;
  u_char *error;
} uconf_error_t;

/**
 * This function reads the modules XML file, parses all information and saves it in a uconf_userconfig_t structure
 * \return NULL if failure, pointer to uconf_userconfig_t on success
 */
uconf_userconfig_t *cf_uconf_read_modxml();

void cf_uconf_destroy_argument(uconf_argument_t *argument);
void cf_uconf_destroy_directive(uconf_directive_t *dir);
void cf_uconf_cleanup_modxml(uconf_userconfig_t *modxml);
void cf_uconf_to_html(string_t *str);

uconf_userconfig_t *cf_uconf_merge_config(cf_hash_t *head,configuration_t *config,array_t *errormessages,int touch_committed);
u_char *cf_write_uconf(const u_char *filename,uconf_userconfig_t *merged);

int cf_run_uconf_write_handlers(cf_hash_t *cgi,configuration_t *dc,configuration_t *uc,configuration_t *oldconf,uconf_userconfig_t *newconf);
void cf_run_uconf_display_handlers(cf_hash_t *cgi,configuration_t *dc,configuration_t *uc,cf_template_t *tpl,configuration_t *user);

typedef int (*uconf_write_filter_t)(cf_hash_t *cgi,configuration_t *dc,configuration_t *uc,configuration_t *oldconf,uconf_userconfig_t *newconf);
typedef int (*uconf_display_filter_t)(cf_hash_t *cgi,configuration_t *dc,configuration_t *uc,cf_template_t *tpl,configuration_t *uconf);

typedef void (*uconf_action_handler_t)(cf_hash_t *cgi,configuration_t *dc,configuration_t *uc);

int uconf_register_action_handler(u_char *name,uconf_action_handler_t action);
uconf_action_handler_t uconf_get_action_handler(u_char *name);

#endif

/* eof */
