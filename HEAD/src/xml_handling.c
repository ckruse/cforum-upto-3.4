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
#include "config.h"
#include "defines.h"

#include <stdlib.h>
#include <gdome.h>
#include <pthread.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "cf_pthread.h"

#include "charconvert.h"
#include "utils.h"
#include "hashlib.h"
#include "readline.h"
#include "fo_server.h"
#include "configparser.h"
#include "xml_handling.h"
#include "serverlib.h"
/* }}} */

/* {{{ xml_create_doc */
GdomeDocument *xml_create_doc(GdomeDOMImplementation *impl,const u_char *dtd_uri) {
  GdomeException e;
  GdomeDOMString *qname  = gdome_str_mkref("Forum");
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

  if(val == NULL) {
    cf_log(LOG_ERR,__FILE__,__LINE__,"val is null! (name: %s)\n",name);
  }

  gdome_el_setAttribute(el,nm,vl,&e);

  gdome_str_unref(nm);
  gdome_str_unref(vl);
}
/* }}} */

/* {{{ xml_set_value */
void xml_set_value(GdomeDocument *doc,GdomeElement *el,const u_char *value) {
  GdomeException e;
  GdomeDOMString *str = gdome_str_mkref_dup(value);
  GdomeText *txt      = gdome_doc_createTextNode(doc,str,&e);

  gdome_el_appendChild(el,(GdomeNode *)txt,&e);

  gdome_t_unref(txt,&e);
  gdome_str_unref(str);
}
/* }}} */

/* {{{ stringify_posting */
t_posting *stringify_posting(GdomeDocument *doc1,GdomeElement *t1,GdomeDocument *doc2,GdomeElement *t2,t_posting *p) {
  int lvl = p->level;
  u_char buff[50];
  GdomeException e;
  GdomeElement *elem1,*elem2;
  GdomeElement *m1 = xml_create_element(doc1,"Message");
  GdomeElement *m2 = xml_create_element(doc2,"Message");

  GdomeElement *header1 = xml_create_element(doc1,"Header");
  GdomeElement *header2 = xml_create_element(doc2,"Header");

  GdomeElement *author1 = xml_create_element(doc1,"Author");
  GdomeElement *author2 = xml_create_element(doc2,"Author");

  gdome_el_appendChild(m1,(GdomeNode *)header1,&e);
  gdome_el_appendChild(m2,(GdomeNode *)header2,&e);

  gdome_el_appendChild(header1,(GdomeNode *)author1,&e);
  gdome_el_appendChild(header2,(GdomeNode *)author2,&e);


  /* the invisible flag */
  xml_set_attribute(m1,"invisible",p->invisible ? "1" : "0");
  xml_set_attribute(m2,"invisible",p->invisible ? "1" : "0");

  /* the name */
  elem1 = xml_create_element(doc1,"Name");
  elem2 = xml_create_element(doc2,"Name");

  xml_set_value(doc1,elem1,p->user.name);
  xml_set_value(doc2,elem2,p->user.name);

  gdome_el_appendChild(author1,(GdomeNode *)elem1,&e);
  gdome_el_appendChild(author2,(GdomeNode *)elem2,&e);

  gdome_el_unref(elem1,&e);
  gdome_el_unref(elem2,&e);


  /* the email address */
  elem1 = xml_create_element(doc1,"Email");
  elem2 = xml_create_element(doc2,"Email");

  if(p->user.email) {
    xml_set_value(doc1,elem1,p->user.email);
    xml_set_value(doc2,elem2,p->user.email);
  }

  gdome_el_appendChild(author1,(GdomeNode *)elem1,&e);
  gdome_el_appendChild(author2,(GdomeNode *)elem2,&e);

  gdome_el_unref(elem1,&e);
  gdome_el_unref(elem2,&e);

  /* the homepage url */
  elem1 = xml_create_element(doc1,"HomepageUrl");
  elem2 = xml_create_element(doc2,"HomepageUrl");

  if(p->user.hp) {
    xml_set_value(doc1,elem1,p->user.hp);
    xml_set_value(doc2,elem2,p->user.hp);
  }

  gdome_el_appendChild(author1,(GdomeNode *)elem1,&e);
  gdome_el_appendChild(author2,(GdomeNode *)elem2,&e);

  gdome_el_unref(elem1,&e);
  gdome_el_unref(elem2,&e);

  /* the image url */
  elem1 = xml_create_element(doc1,"ImageUrl");
  elem2 = xml_create_element(doc2,"ImageUrl");

  if(p->user.img) {
    xml_set_value(doc1,elem1,p->user.img);
    xml_set_value(doc2,elem2,p->user.img);
  }

  gdome_el_appendChild(author1,(GdomeNode *)elem1,&e);
  gdome_el_appendChild(author2,(GdomeNode *)elem2,&e);

  gdome_el_unref(elem1,&e);
  gdome_el_unref(elem2,&e);

  /* category */
  elem1 = xml_create_element(doc1,"Category");
  elem2 = xml_create_element(doc2,"Category");

  if(p->category) {
    xml_set_value(doc1,elem1,p->category);
    xml_set_value(doc2,elem2,p->category);
  }

  gdome_el_appendChild(header1,(GdomeNode *)elem1,&e);
  gdome_el_appendChild(header2,(GdomeNode *)elem2,&e);

  gdome_el_unref(elem1,&e);
  gdome_el_unref(elem2,&e);

  /* subject */
  elem1 = xml_create_element(doc1,"Subject");
  elem2 = xml_create_element(doc2,"Subject");

  xml_set_value(doc1,elem1,p->subject);
  xml_set_value(doc2,elem2,p->subject);

  gdome_el_appendChild(header1,(GdomeNode *)elem1,&e);
  gdome_el_appendChild(header2,(GdomeNode *)elem2,&e);

  gdome_el_unref(elem1,&e);
  gdome_el_unref(elem2,&e);

  /* date */
  elem1 = xml_create_element(doc1,"Date");
  elem2 = xml_create_element(doc2,"Date");

  sprintf(buff,"%ld",p->date);

  xml_set_attribute(elem1,"longSec",buff);
  xml_set_attribute(elem2,"longSec",buff);

  gdome_el_appendChild(header1,(GdomeNode *)elem1,&e);
  gdome_el_appendChild(header2,(GdomeNode *)elem2,&e);

  gdome_el_unref(elem1,&e);
  gdome_el_unref(elem2,&e);


  /* set the id and the ip... */
  sprintf(buff,"m%lld",p->mid);
  xml_set_attribute(m1,"id",buff);

  if(p->unid) {
    xml_set_attribute(m1,"unid",p->unid);
  }

  xml_set_attribute(m2,"id",buff);
  xml_set_attribute(m2,"ip",p->user.ip);

  gdome_el_appendChild(t1,(GdomeNode *)m1,&e);
  gdome_el_appendChild(t2,(GdomeNode *)m2,&e);
  
  for(p=p->next;p;) {
    if(p->level > lvl) {
      p = stringify_posting(doc1,m1,doc2,m2,p);
    }
    else { /* smaller or equal */
      gdome_el_unref(m1,&e);
      gdome_el_unref(m2,&e);

      gdome_el_unref(header1,&e);
      gdome_el_unref(header2,&e);

      gdome_el_unref(author1,&e);
      gdome_el_unref(author2,&e);

      return p;
    }
  }

  gdome_el_unref(m1,&e);
  gdome_el_unref(m2,&e);

  gdome_el_unref(header1,&e);
  gdome_el_unref(header2,&e);

  gdome_el_unref(author1,&e);
  gdome_el_unref(author2,&e);

  return NULL;
}
/* }}} */

/* {{{ stringify_thread_and_write_to_disk */
void stringify_thread_and_write_to_disk(GdomeDocument *doc1,t_thread *t) {
  t_name_value *mpath = cfg_get_first_value(&fo_default_conf,"MessagePath");
  GdomeException e;
  u_char buff[256];
  GdomeDOMImplementation *impl = gdome_di_mkref();
  GdomeDocument *doc2   = xml_create_doc(impl,FORUM_DTD);
  GdomeElement *thread1 = xml_create_element(doc1,"Thread");
  GdomeElement *thread2 = xml_create_element(doc2,"Thread");
  GdomeElement *msgcnt  = xml_create_element(doc2,"ContentList");
  GdomeElement *root    = gdome_doc_documentElement(doc1,&e);
  GdomeElement *el;
  GdomeCDATASection *cd;
  GdomeDOMString *str;
  t_posting *p;

  sprintf(buff,"t%lld",t->tid);

  xml_set_attribute(thread1,"id",buff);
  xml_set_attribute(thread2,"id",buff);

  stringify_posting(doc1,thread1,doc2,thread2,t->postings);

  for(p=t->postings;p;p=p->next) {
    el = xml_create_element(doc2,"MessageContent");
    sprintf(buff,"m%lld",p->mid);
    xml_set_attribute(el,"mid",buff);

    str = gdome_str_mkref_dup(p->content);

    cd = gdome_doc_createCDATASection(doc2,str,&e);
    gdome_el_appendChild(el,(GdomeNode *)cd,&e);
    gdome_el_appendChild(msgcnt,(GdomeNode *)el,&e); /* this fucking silly line causes the memory leek :/ */

    gdome_cds_unref(cd,&e);
    gdome_str_unref(str);
    gdome_el_unref(el,&e);
  }

  gdome_el_appendChild(root,(GdomeNode *)thread1,&e);
  gdome_el_unref(root,&e);

  root = gdome_doc_documentElement(doc2,&e);
  gdome_el_appendChild(root,(GdomeNode *)thread2,&e);
  gdome_el_appendChild(root,(GdomeNode *)msgcnt,&e);

  /* save doc to file... */
  snprintf(buff,256,"%s/t%lld.xml",mpath->values[0],t->tid);
  if(!gdome_di_saveDocToFile(impl,doc2,buff,0,&e)) {
    cf_log(LOG_ERR,__FILE__,__LINE__,"ERROR! COULD NOT WRITE XML FILE!\n");
  }

  gdome_el_unref(thread1,&e);
  gdome_el_unref(thread2,&e);
  gdome_el_unref(msgcnt,&e);
  gdome_el_unref(root,&e);

  gdome_doc_unref(doc2,&e);

  gdome_di_unref(impl,&e);
}
/* }}} */

/* {{{ get_node_value
 * Returns: u_char *  (NULL on failure)
 * Parameters:
 *   - GdomeNode *n
 *
 * this function tries to get the node value
 *
 */
u_char *get_node_value(GdomeNode *n) {
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

/* eof */

