/**
 * \file template.h
 * \author Christian Kruse, <ckruse@wwwtech.de>,
 *         Christian Seiler, <self@christian-seiler.de>
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

#define TPL_VARIABLE_INVALID      0
#define TPL_VARIABLE_STRING       1
#define TPL_VARIABLE_INT          2
#define TPL_VARIABLE_ARRAY        3
#define TPL_VARIABLE_HASH         4

/**
 * This struct contains the necessary information about the template file.
 */
typedef struct s_cf_template {
  void *tpl; /**< A pointer to the template file (by dlopen) */
  u_char *filename; /**< The filename of the template */

  t_string parsed; /**< A string structure containing the parsed content */
  t_cf_hash *varlist; /**< A hash containing the variables */

} t_cf_template;

/**
 * This strcut contains a template variable
 */
typedef struct s_cf_tpl_variable {
  unsigned short type; /**< The type of the variable (string, int, array) */
  union {
    t_string d_string; /**< String data */
    signed long d_int; /**< Integer data */
    t_array d_array; /**< Array data */
    t_cf_hash *d_hash; /**< Hash data */
  } data; /**< The actual template data */
  
  unsigned short temporary; /**< Internal flag: Is this a temporary variable? */
  unsigned short arrayref; /**< Internal flag: Is this a variable created in a foreach loop? */
} t_cf_tpl_variable ;

typedef void (*t_parse)(t_cf_template *); /**< This is the function type called when parsing (tpl_parse) a template file */
typedef void (*t_parse_mem)(t_cf_template *); /**< This /is the function type called when parsing (tpl_parse_to_mem) a template file */


/**
 * This function loads a template file and initializes the template structure.
 * \param tpl The template structure
 * \param fname The filename of the template file. It has to contain the full path!
 * \return 0 on success, -1 on error
 */
int cf_tpl_init(t_cf_template *tpl,const u_char *fname);

/**
 * This function adds a template variable to the template
 * \param tpl The template structure
 * \param vname The name of the variable that is to be added
 * \param var The variable structure
 */
void cf_tpl_setvar(t_cf_template *tpl,const u_char *vname,t_cf_tpl_variable *var);

/**
 * This function adds a template variable to the template by its value. Only string and integer variables are supported.
 * \param tpl The template structure
 * \param vname The name of the variable that is to be added
 * \param type The type of the variable (integer, string)
 */
void cf_tpl_setvalue(t_cf_template *tpl,const u_char *vname,unsigned short type,...);

/**
 * This function appends to a string variable
 * \param tpl The template structure
 * \param vname The name of the variable
 * \param value The value to append
 * \param len The length of the value
 * \return 0 on success, -1 on error (e.g. not a string variable)
 */
int cf_tpl_appendvalue(t_cf_template *tpl,const u_char *vname,const u_char *value,int len);

/**
 * This function frees a template variable
 * \param tpl The template file structure
 * \param vname The variable name
 */
void cf_tpl_freevar(t_cf_template *tpl,const u_char *vname);

/**
 * This function initializes a template varialbe structure
 * \param var The variable structure
 * \param type The type of the variable
 */
void cf_tpl_var_init(t_cf_tpl_variable *var,unsigned short type);

/**
 * This function destroyes a template varialbe structure
 * \param var The variable structure
 */
void cf_tpl_var_destroy(t_cf_tpl_variable *var);

/**
 * This function sets the value of a string or integer variable. The type is determined from the variable structure itself.
 * \param var The variable structure
 */
void cf_tpl_var_setvalue(t_cf_tpl_variable *var,...);

/**
 * This function converts a variable from one type to another. It returns the pointer to the converted variable.
 * \param dest The destination variable structure. This is also returned by the function. If it is NULL, a temporary variable will be created and returned.
 * \param src The source variable structure.
 * \param new_type The new type of the variable.
 * \returns The dest parameter, a new temporary variable structure or NULL if an error occcurred.
 */
t_cf_tpl_variable *cf_tpl_var_convert(t_cf_tpl_variable *dest,t_cf_tpl_variable *src,unsigned short new_type);

/**
 * This function clones a template variable with all subvariables if this is an array.
 * \param var The variable that is to be cloned
 * \returns The cloned variable.
 */
t_cf_tpl_variable *cf_tpl_var_clone(t_cf_tpl_variable *var);

/**
 * This function adds an element to an array
 * \param var The array variable structure
 * \param element The new element that is to be added
 */
void cf_tpl_var_add(t_cf_tpl_variable *var,t_cf_tpl_variable *element);

/**
 * This function adds an element to an array by its value. Only string and integer values are supported
 * \param var The array variable structure
 * \param type The type of the element to add
 */
void cf_tpl_var_addvalue(t_cf_tpl_variable *array_var,unsigned short type,...);

/**
 * This function sets an element of a hash
 * \param var The hash variable structure
 * \param key The key of the element
 * \param element The element that is to be set
 */
void cf_tpl_hashvar_set(t_cf_tpl_variable *var,const u_char *key,t_cf_tpl_variable *element);

/**
 * This function sets a value of a hash
 * \param var The hash variable structure
 * \param key The key of the element
 * \param type The type of the element to set
 */
void cf_tpl_hashvar_setvalue(t_cf_tpl_variable *array_var,const u_char *key,unsigned short type,...);

/**
 * This function will parse a template file and print out the parsed content. No memory is allocated, this
 * should be very fast.
 * \param tpl The template file structure
 */
void cf_tpl_parse(t_cf_template *tpl);

/**
 * This function parses a template file and store the parsed content into the string structure parsed in the
 * template file structure.
 * \param tpl The template file structure
 */

void cf_tpl_parse_to_mem(t_cf_template *tpl);
/**
 * This function destroys a template file structure. It closes the template file, frees the parsed content
 * and destroys the variables.
 * \param tpl The template file structure
 */
void cf_tpl_finish(t_cf_template *tpl);

/**
 * This function returns the value of a template variable.
 * \param tpl The template file structure
 * \param vname The variable name
 * \return The value of the variable if the variable could be found or NULL
 * \attention You may not free this value! It's done internally by the template engine!
 */
const t_cf_tpl_variable *cf_tpl_getvar(t_cf_template *tpl,const u_char *vname);


#endif

/* eof */
