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
u_char *get_node_value(GdomeNode *n);

/**
 * This function creates a new DOM document.
 * \param impl The DOM implementation
 * \param dtd_uri The URI to the DTD
 * \return NULL on error, the document on success
 * \attention You have to destroy the GdomeDocument!
 */
GdomeDocument *xml_create_doc(GdomeDOMImplementation *impl,const u_char *dtd_uri);

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
 * This function sets a value to a GdomeElement.
 * \param doc The Gdome DOM document object
 * \param el The Gdome DOM XML element
 * \param value The value to set to the element
 */
void xml_set_value(GdomeDocument *doc,GdomeElement *el,const u_char *value);

/**
 * This function stringifies the postings of a thread.
 * \param doc1 The Gdome DOM document of the index file
 * \param t1 The thread element of the index file
 * \param doc2 The Gdome DOM document of the thread file
 * \param t2 The thread element of the thread file
 * \param p The posting element
 * \return The next posting in this hierarchy or lower this hierarchy level.
 */
t_posting *stringify_posting(GdomeDocument *doc1,GdomeElement *t1,GdomeDocument *doc2,GdomeElement *t2,t_posting *p);

/**
 * This function stringifies the thread and its postings
 * \param doc1 The index file document
 * \param t The thread structure
 */
void stringify_thread_and_write_to_disk(GdomeDocument *doc1,t_thread *t);

#endif

/* eof */
