/**
 * \file xml_handling.h
 * \author Christian Kruse, <ckruse@wwwtech.de>
 * \brief XML handling function definitions
 *
 * This file contains functions making it easier to
 * handle the gdome library
 */

/* {{{ Initial headers */
/*
 * $LastChangedDate$
 * $LastChangedRevision$
 * $LastChangedBy$
 *
 */
/* }}} */

#ifndef __XML_HANDLING_ROUTINES_H
#define __XML_HANDLING_ROUTINES_H

/**
 * This function tries to get the node content. The returned charset is UTF-8.
 * \param n The node value
 * \return NULL on error, the content as a string on success.
 * \attention You have to free() the returned string!
 */
u_char *xml_get_node_value(GdomeNode *n);

/**
 * This function creates a new DOM document.
 * \param impl The DOM implementation
 * \param root The name of the root element
 * \param dtd_uri The URI to the DTD
 * \return NULL on error, the document on success
 * \attention You have to destroy the GdomeDocument!
 */
GdomeDocument *xml_create_doc(GdomeDOMImplementation *impl,const u_char *root,const u_char *dtd_uri);

/**
 * This function creates a new XML element.
 * \param doc The DOM document
 * \param name The element name
 * \return NULL on error, the element object on success
 * \attention You have to destroy the GdomeElement!
 */
GdomeElement *xml_create_element(GdomeDocument *doc,const u_char *name);

/**
 * This function sets a XML attribute to the element
 * \param el The DOM XML element object
 * \param name The attribute name
 * \param val The attribute value
 */
void xml_set_attribute(GdomeElement *el,const u_char *name,const u_char *val);

/**
 * This function gets the value of an attribute of a node
 * \param n The node object
 * \param name The name of the attribute
 * \return The value of the attribute or NULL if attribute is not present
 */
u_char *xml_get_attribute(GdomeNode *n,const u_char *name);

/**
 * This function sets a value to a GdomeElement.
 * \param doc The Gdome DOM document object
 * \param el The Gdome DOM XML element
 * \param value The value to set to the element
 */
void xml_set_value(GdomeDocument *doc,GdomeElement *el,const u_char *value);

/**
 * This function gets the first occurence of the give tag name
 * and returns it
 * \param node The node which child nodes should be searched
 * \param name The name of the tag
 * \return NULL if element could not be found or first occurence of element
 */
GdomeNode *xml_get_first_element_by_name(GdomeNode *node,const u_char *name);

#endif

/* eof */
