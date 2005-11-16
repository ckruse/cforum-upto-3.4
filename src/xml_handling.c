/**
 * \file xml_handling.c
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

/* {{{ Includes */
#include "cfconfig.h"
#include "defines.h"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include <gdome.h>

#include <sys/types.h>

#include "charconvert.h"
#include "hashlib.h"
#include "utils.h"

#include "xml_handling.h"
/* }}} */

/* {{{ xml_create_doc */
GdomeDocument *xml_create_doc(GdomeDOMImplementation *impl,const u_char *root,const u_char *dtd_uri) {
  GdomeException e;
  GdomeDOMString *qname  = gdome_str_mkref_dup(root);
  GdomeDOMString *sysid  = gdome_str_mkref_dup(dtd_uri);
  GdomeDocumentType *dtd = gdome_di_createDocumentType(impl,qname,NULL,sysid,&e);
  GdomeDocument *doc     = gdome_di_createDocument(impl,NULL,qname,dtd,&e);

  gdome_str_unref(qname);
  gdome_str_unref(sysid);
  gdome_dt_unref(dtd,&e);

  return doc;
}
/* }}} */

/* {{{ xml_create_element */
GdomeElement *xml_create_element(GdomeDocument *doc,const u_char *name) {
  GdomeException e;
  GdomeDOMString *tmp = gdome_str_mkref_dup(name);
  GdomeElement *el    = gdome_doc_createElement(doc,tmp,&e);
  gdome_str_unref(tmp);

  return el;
}
/* }}} */

/* {{{ xml_set_attribute */
void xml_set_attribute(GdomeElement *el,const u_char *name,const u_char *val) {
  GdomeException e;
  GdomeDOMString *nm = gdome_str_mkref_dup(name);
  GdomeDOMString *vl = gdome_str_mkref_dup(val);

  gdome_el_setAttribute(el,nm,vl,&e);

  gdome_str_unref(nm);
  gdome_str_unref(vl);
}
/* }}} */

/* {{{ xml_get_attribute */
u_char *xml_get_attribute(GdomeNode *n,const u_char *name) {
  GdomeException e;
  GdomeNamedNodeMap *nnm = gdome_n_attributes(n,&e);
  GdomeDOMString *ds_name = gdome_str_mkref_dup(name);

  GdomeNode *n1 = gdome_nnm_getNamedItem(nnm,ds_name,&e);
  u_char *val = NULL;

  if(n1) {
    val = xml_get_node_value(n1);
    gdome_n_unref(n1,&e);
  }

  gdome_nnm_unref(nnm,&e);
  gdome_str_unref(ds_name);

  return val;
}
/* }}} */

/* {{{ xml_set_value */
void xml_set_value(GdomeDocument *doc,GdomeElement *el,const u_char *value) {
  GdomeException e;
  GdomeDOMString *str = gdome_str_mkref_dup(value);
  GdomeText      *txt = gdome_doc_createTextNode(doc,str,&e);

  gdome_el_appendChild(el,(GdomeNode *)txt,&e);

  gdome_t_unref(txt,&e);
  gdome_str_unref(str);
}
/* }}} */

/* {{{ xml_get_node_value */
u_char *xml_get_node_value(GdomeNode *n) {
  GdomeException  exc;
  GdomeNode      *x   = gdome_n_firstChild(n,&exc);

  if(x) {
    GdomeDOMString *y = gdome_n_nodeValue(x,&exc);

    if(y) {
      u_char *z = strdup(y->str);

      gdome_n_unref(x,&exc);
      gdome_str_unref(y);

      return z;
    }
    else {
      GdomeDOMString *y = gdome_n_nodeValue(n,&exc);

      if(y) {
        u_char *z = strdup(y->str);

        gdome_n_unref(x,&exc);
        gdome_str_unref(y);

        return z;
      }
    }

    gdome_n_unref(x,&exc);
  }

  return NULL;
}
/* }}} */

/* {{{ xml_get_first_element_by_name */
GdomeNode *xml_get_first_element_by_name(GdomeNode *node,const u_char *name) {
  GdomeException e;
  GdomeDOMString *gdname = gdome_str_mkref_dup(name);
  GdomeNodeList *nl = gdome_el_getElementsByTagName((GdomeElement *)node,gdname,&e);
  GdomeNode *n = NULL;

  if(nl) {
    n = gdome_nl_item(nl,0,&e);
    gdome_nl_unref(nl,&e);
  }

  gdome_str_unref(gdname);
  return n;
}
/* }}} */

/* eof */

