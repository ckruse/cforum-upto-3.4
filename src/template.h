/**
 * \file template.h
 * \author Christian Kruse, <ckruse@wwwtech.de>
 * \brief the template function prototypes
 *
 * This template library supports recursive ifs and variables. The template files will be transformed to C code
 * and compiled to shared library files. These files will be bound to the program at runtime by dlopen(). This
 * is *very* fast.
 */

/* {{{ Initial headers */
/*
 * $LastChangedDate$
 * $LastChangedRevision$
 * $LastChangedBy$
 *
 */
/* }}} */

#ifndef __TEMPLATE_H
#define __TEMPLATE_H

/**
 * This struct contains the necessary information about the template file.
 */
typedef struct s_cf_template {
  void *tpl; /**< A pointer to the template file (by dlopen) */

  t_string parsed; /**< A string structure containing the parsed content */
  t_cf_hash *varlist; /**< A hash containing the variables */

} t_cf_template;

/**
 * This structure defines a variable value
 */
typedef struct s_cf_tpl_variable {
  t_string *data; /**< The value of the variable */
  int escape_html; /**< Shall this variable be html-escaped? */
} t_cf_tpl_variable;


typedef void (*t_parse)(t_cf_template *); /**< This is the function type called when parsing (tpl_parse) a template file */
typedef void (*t_parse_mem)(t_cf_template *); /**< This is the function type called when parsing (tpl_pars_to_mem) a template file */


/**
 * This function loads a template file and initializes the template structure.
 * \param tpl The template structure
 * \param fname The filename of the template file. It has to contain the full path!
 * \return 0 on success, -1 on error
 */
int tpl_cf_init(t_cf_template *tpl,const u_char *fname);

/**
 * This function sets a template variable to a specified value.
 * \param tpl The template structure
 * \param vname The variable name
 * \param value The value of the variable
 * \param len The length of the value
 * \param escapehtml Should the characters in the string converted to HTML entities if neccessary?
 */
void tpl_cf_setvar(t_cf_template *tpl,u_char *vname,const u_char *value,int len,int escapehtml);

/**
 * This function appends a string to the value of the specified variable.
 * \param tpl The template structure
 * \param vname The variable name
 * \param value The value to append
 * \param len The length of the value
 * \return 0 on success, -1 on error
 */
int  tpl_cf_appendvar(t_cf_template *tpl,u_char *vname,const u_char *value,int len);

/**
 * This function unsets a template variable.
 * \param tpl The template file structure
 * \param vname The variable name
 */
void tpl_cf_freevar(t_cf_template *tpl,u_char *vname);

/**
 * This function will parse a template file and print out the parsed content. No memory is allocated, this
 * should be very fast.
 * \param tpl The template file structure
 */
void tpl_cf_parse(t_cf_template *tpl);

/**
 * This function parses a template file and store the parsed content into the string structure parsed in the
 * template file structure.
 * \param tpl The template file structure
 */
void tpl_cf_parse_to_mem(t_cf_template *tpl);

/**
 * This function destroys a template file structure. It closes the template file, frees the parsed content
 * and destroys the variables.
 * \param tpl The template file structure
 */
void tpl_cf_finish(t_cf_template *tpl);

/**
 * This function returns the value of a template variable.
 * \param tpl The template file structure
 * \param vname The variable name
 * \return The value of the variable if the variable could be found or NULL
 * \attention You may not free this value! It's done internally by the template engine!
 */
const t_cf_tpl_variable *tpl_cf_getvar(t_cf_template *tpl,u_char *vname);


#endif

/* eof */
