/**
 * \file flt_xmlstorage.c
 * \author Christian Kruse, <cjk@wwwtech.de>
 * \brief XML storage plugin
 *
 * This file contains a storage plugin implementation and
 * uses XML to save all data
 */

/* {{{ initial comments */
/*
 * $LastChangedDate$
 * $LastChangedRevision$
 * $LastChangedBy$
 *
 */
/* }}} */

/* {{{ includes */
#include "cfconfig.h"
#include "defines.h"

#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>

#include <gdome.h>

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <inttypes.h>

struct sockaddr_un;

#include "cf_pthread.h"

#include "hashlib.h"
#include "utils.h"
#include "cfgcomp.h"
#include "readline.h"

#include "serverutils.h"
#include "serverlib.h"
#include "fo_server.h"

#include "xml_handling.h"
/* }}} */

/** The URL of the selfforum DTD */
#define FORUM_DTD "http://wwwtech.de/cforum/download/cforum-3.dtd"

static int sort_threads  = 0;
static int sort_messages = 0;
static int archive_ip    = 0;

void flt_xmlstorage_create_threadtree(forum_t *forum,thread_t *thread,posting_t *post,GdomeNode *msg_elem_index,GdomeNode *msg_elem_thread,short level);
void flt_xmlstorage_handle_header(posting_t *p,GdomeNode *n);

/* {{{ flt_xmlstorage_cmp_thread */
int flt_xmlstorage_cmp_thread(const void *a,const void *b) {
  thread_t *ta = *((thread_t **)a);
  thread_t *tb = *((thread_t **)b);

  if(ta->tid > tb->tid) return sort_threads == CF_SORT_ASCENDING ? 1 : -1;
  else return sort_threads == CF_SORT_ASCENDING ? -1 : 1;
}
/* }}} */

/* {{{ struct s_h_p */
/** This struct is used to sort the thread list. It contains a hierarchical structure. */
typedef struct s_h_p {
  posting_t *node; /**< The pointer to the posting */
  long len; /**< The number of postings in this hierarchy level */
  struct s_h_p *childs; /**< The answers to this posting */
} h_p_t;
/* }}} */

/* {{{ flt_xmlstorage_hirarchy_them */
/**
 * Insert the postings to a hierarchy level. This function works recursively.
 * \param parent The parent h_p_t structure
 * \param pst The posting pointer
 * \return The pointer to the next posting in a hierarchy level smaller or equal to ours.
 */
posting_t *flt_xmlstorage_hirarchy_them(h_p_t *parent,posting_t *pst) {
  posting_t *p;
  int lvl;

  p   = pst;
  lvl = p->level;

  while(p) {
    if(p->level == lvl) {
      parent->childs = cf_alloc(parent->childs,parent->len+1, sizeof(h_p_t),CF_ALLOC_REALLOC);
      memset(&parent->childs[parent->len],0,sizeof(h_p_t));

      parent->childs[parent->len++].node = p;
      p = p->next;
    }
    else if(p->level > lvl) p = flt_xmlstorage_hirarchy_them(&parent->childs[parent->len-1],p);
    else return p;
  }

  return NULL;
}
/* }}} */

/* {{{ flt_xmlstorage_quicksort_posts */
/**
 * Quicksort-function for the postings
 * \param list The hierarchical list of postings
 * \param low The lower frontier of the field
 * \param high The higher frontier of the field
 */
void flt_xmlstorage_quicksort_posts(h_p_t *list,long low,long high) {
  long i = low,j = high;
  h_p_t *pivot,tmp;

  pivot = &list[(low+high)/2];

  do {
    while(i<high && list[i].node->date < pivot->node->date) i++;
    while(j>low && list[j].node->date > pivot->node->date) j--;

    if(i <= j) {
      tmp     = list[i];
      list[i] = list[j];
      list[j] = tmp;
      i++; j--;
    }
  } while(i <= j);

  if(low<j) flt_xmlstorage_quicksort_posts(list,low,j);
  if(i<high) flt_xmlstorage_quicksort_posts(list,i,high);
}
/* }}} */

/* {{{ flt_xmlstorage_sort_them */
/**
 * Sorting function.
 * \param node The h_p_t structure node
 */
void flt_xmlstorage_sort_them(h_p_t *node) {
  long i;

  if(node->len) {
    flt_xmlstorage_quicksort_posts(node->childs,0,node->len-1);

    for(i=0;i<node->len;i++) {
     flt_xmlstorage_sort_them(&node->childs[i]);
    }
  }

}
/* }}} */

/* {{{ flt_xmlstorage_free_structs */
/**
 * This function cleans up a hierarchical structure
 * \param node The h_p_t node
 */
void flt_xmlstorage_free_structs(h_p_t *node) {
  long i;

  if(node->len) {
    for(i=0;i<node->len;i++) {
     flt_xmlstorage_free_structs(&node->childs[i]);
    }

    free(node->childs);
  }
}
/* }}} */

/* {{{ flt_xmlstorage_serialize_them */
/**
 * This function serializes a hierarchical structure into a flat chain
 * \param node The h_p_t node
 * \param sort The sorting direction
 * \return A posting in this or lower than this hierarchy level
 */
posting_t *flt_xmlstorage_serialize_them(h_p_t *node) {
  long i;
  posting_t *p;

  if(sort_messages == CF_SORT_ASCENDING) {
    if(node->len) {
      node->node->next = node->childs[0].node;

      for(i=0;i<node->len;i++) {
        if(node->childs[i].len) {
          p = flt_xmlstorage_serialize_them(&node->childs[i]);

          if(i < node->len-1) {
            p->next = node->childs[i+1].node;
          }
          else {
            p->next = NULL;
            return p;
          }
        }
        else {
          if(i < node->len-1) {
            node->childs[i].node->next = node->childs[i+1].node;
          }
          else {
            node->childs[i].node->next = NULL;
            return node->childs[i].node;
          }
        }
      }
    }
    else {
      node->node->next = NULL;
      return node->node;
    }
  }
  else {
    if(node->len) {
      node->node->next = node->childs[node->len-1].node;

      for(i=node->len-1;i>=0;i--) {
        if(node->childs[i].len) {
          p = flt_xmlstorage_serialize_them(&node->childs[i]);

          if(i > 0) {
            p->next = node->childs[i-1].node;
          }
          else {
            p->next = NULL;
            return p;
          }
        }
        else {
          if(i > 0) {
            node->childs[i].node->next = node->childs[i-1].node;
          }
          else {
            node->childs[i].node->next = NULL;
            return node->childs[i].node;
          }
        }
      }
    }
    else {
      node->node->next = NULL;
      return node->node;
    }
  }

  return NULL;
}
/* }}} */

/* {{{ flt_xmlstorage_sormessage_ts */
void flt_xmlstorage_sormessage_ts(thread_t *thread) {
  h_p_t *first = cf_alloc(NULL,1,sizeof(*first),CF_ALLOC_CALLOC);
  posting_t *p,*p1;

  if(thread->postings->next) {
    first->node = thread->postings;
    flt_xmlstorage_hirarchy_them(first,thread->postings);
    /*return;*/
  }
  else {
    free(first);
    return;
  }

  flt_xmlstorage_sort_them(first);
  flt_xmlstorage_serialize_them(first);
  flt_xmlstorage_free_structs(first);

  free(first);

  /* reset prev-pointers */
  for(p=thread->postings,p1=NULL;p;p=p->next) {
    p->prev = p1;
    p1      = p;
  }
}
/* }}} */

/* {{{ flt_xmlstorage_make_forumtree */
int flt_xmlstorage_make_forumtree(forum_t *forum) {
  cf_name_value_t *p = cf_cfg_get_first_value(&fo_default_conf,forum->name,"FS:MessagePath");
  cf_name_value_t *sort_t = cf_cfg_get_first_value(&fo_server_conf,forum->name,"FS:SortThreads");
  cf_name_value_t *sort_m = cf_cfg_get_first_value(&fo_server_conf,forum->name,"FS:SortMessages");
  cf_name_value_t *arch_ip = cf_cfg_get_first_value(&fo_server_conf,forum->name,"FS:ArchiveIp");
  cf_string_t path;

  u_char *ctid;
  unsigned long length = 0,i;
  thread_t *thread;
  cf_array_t ary;
  u_char buff[50];

  GdomeException e;
  GdomeDOMImplementation *di;
  GdomeDocument *doc_index,*doc_thread;
  GdomeDOMString *thread_str;
  GdomeNode *n,*n1,*n2,*root;
  GdomeNodeList *nl;
  GdomeElement *root_element;

  di = gdome_di_mkref();
  thread_str = gdome_str_mkref("Thread");

  cf_array_init(&ary,sizeof(thread),NULL);

  sort_threads  = cf_strcmp(sort_t->values[0],"ascending") == 0 ? CF_SORT_ASCENDING : CF_SORT_DESCENDING;
  sort_messages = cf_strcmp(sort_m->values[0],"ascending") == 0 ? CF_SORT_ASCENDING : CF_SORT_DESCENDING;
  archive_ip    = arch_ip && cf_strcmp(arch_ip->values[0],"yes") == 0;

  cf_str_init(&path);
  cf_str_char_set(&path,p->values[0],strlen(p->values[0]));

  if(path.content[path.len-1] != '/') cf_str_char_append(&path,'/');
  cf_str_chars_append(&path,"forum.xml",9);

  if((doc_index = gdome_di_createDocFromURI(di,path.content,0,&e)) == NULL) {
    cf_log(CF_ERR|CF_FLSH,__FILE__,__LINE__,"error loading file (%s)!\n",path.content);
    return FLT_OK;
  }

  path.len -= 9;

  root_element = gdome_doc_documentElement(doc_index,&e);

  if((ctid = xml_get_attribute((GdomeNode *)root_element,"lastThread")) != NULL) {
    forum->threads.last_tid = cf_str_to_uint64(ctid+1);
    free(ctid);
  }

  if((ctid = xml_get_attribute((GdomeNode *)root_element,"lastMessage")) != NULL) {
    forum->threads.last_mid = cf_str_to_uint64(ctid+1);
    free(ctid);
  }

  gdome_el_unref(root_element,&e);

  nl = gdome_doc_getElementsByTagName(doc_index,thread_str,&e);
  if(nl) length = gdome_nl_length(nl,&e);

  for(i=0;i<length;i++) {
    n    = gdome_nl_item(nl,i,&e);
    ctid = xml_get_attribute(n,"id");

    cf_str_chars_append(&path,ctid,strlen(ctid));
    cf_str_chars_append(&path,".xml",4);
    if((doc_thread = gdome_di_createDocFromURI(di,path.content,0,&e)) == NULL) {
      cf_log(CF_ERR|CF_FLSH,__FILE__,__LINE__,"error loading file (%s)!\n",path.content);
      exit(-1);
    }
    path.len -= strlen(ctid) + 4;

    /* document parsed, now we have to get the Thread element in the thread file */
    root = (GdomeNode *)gdome_doc_documentElement(doc_thread,&e);
    n1   = xml_get_first_element_by_name(root,"Message");
    n2   = xml_get_first_element_by_name(n,"Message");

    thread           = cf_alloc(NULL,1,sizeof(*thread),CF_ALLOC_CALLOC);
    thread->tid      = cf_str_to_uint64(ctid+1);
    thread->postings = cf_alloc(NULL,1,sizeof(*thread->postings),CF_ALLOC_CALLOC);
    thread->last     = thread->postings;

    flt_xmlstorage_create_threadtree(forum,thread,thread->postings,n2,n1,0);
    cf_array_push(&ary,&thread);

    cf_register_thread(forum,thread);

    /* initialize thread lock */
    snprintf(buff,50,"t%"PRIu64,thread->tid);
    cf_rwlock_init(buff,&thread->lock);

    free(ctid);
    gdome_n_unref(n1,&e);
    gdome_n_unref(n2,&e);
    gdome_n_unref(n,&e);
    gdome_n_unref(root,&e);
    gdome_doc_unref(doc_thread,&e);
  }

  gdome_str_unref(thread_str);
  if(nl) gdome_nl_unref(nl,&e);
  gdome_doc_unref(doc_index,&e);
  gdome_di_unref(di,&e);

  cf_str_cleanup(&path);

  cf_array_sort(&ary,flt_xmlstorage_cmp_thread);

  for(i=0;i<ary.elements;i++) {
    thread = *((thread_t **)cf_array_element_at(&ary,i));

    if(forum->threads.last == NULL) {
      forum->threads.last = thread;
      forum->threads.list = thread;
      thread->prev = NULL;
    }
    else {
      forum->threads.last->next = thread;
      thread->prev = forum->threads.last;
      forum->threads.last = thread;
    }

    /* sort messages */
    flt_xmlstorage_sormessage_ts(thread);
  }

  cf_array_destroy(&ary);

  return FLT_OK;
}
/* }}} */

/* {{{ flt_xmlstorage_gemessage_t_node */
GdomeNode *flt_xmlstorage_gemessage_t_node(GdomeNode *parent,const u_char *mid) {
  GdomeException e;
  GdomeDOMString *str = gdome_str_mkref("Message");
  GdomeNodeList *nl_ind = gdome_el_getElementsByTagName((GdomeElement *)parent,str,&e);
  GdomeNode *n = NULL;
  unsigned long len,i;
  u_char *tmp;

  len = gdome_nl_length(nl_ind,&e);
  for(i=0;i<len;i++) {
    n   = gdome_nl_item(nl_ind,i,&e);
    tmp = xml_get_attribute(n,"id");

    if(cf_strcmp(tmp,mid) == 0) {
      free(tmp);
      break;
    }

    free(tmp);
    gdome_n_unref(n,&e);
    n = NULL;
  }

  gdome_nl_unref(nl_ind,&e);
  gdome_str_unref(str);

  return n;
}
/* }}} */

/* {{{ flt_xmlstorage_create_threadtree */
void flt_xmlstorage_create_threadtree(forum_t *forum,thread_t *thread,posting_t *post,GdomeNode *msg_elem_index,GdomeNode *msg_elem_thread,short level) {
  u_char *unid  = xml_get_attribute(msg_elem_index,"unid");
  u_char *cmid  = xml_get_attribute(msg_elem_thread,"id");
  u_char *invi  = xml_get_attribute(msg_elem_thread,"invisible");
  u_char *vgood = xml_get_attribute(msg_elem_thread,"votingGood");
  u_char *vbad  = xml_get_attribute(msg_elem_thread,"votingBad");
  u_char *ip    = xml_get_attribute(msg_elem_thread,"ip");
  u_char *cnt;

  unsigned long len,i,z = 0;

  GdomeException e;
  GdomeNodeList *nl_thr = gdome_n_childNodes(msg_elem_thread,&e);

  GdomeNode *n,*msg_ind;
  GdomeDOMString *name;

  posting_t *p;

  ++thread->posts;

  post->unid.content = unid;
  post->unid.len   = post->unid.reserved = strlen(unid);
  post->mid        = cf_str_to_uint64(cmid+1);
  post->invisible  = atoi(invi);
  post->votes_good = strtoul(vgood,NULL,10);
  post->votes_bad  = strtoul(vbad,NULL,10);
  post->user.ip.content = ip;
  post->user.ip.len = post->user.ip.reserved = strlen(ip);
  post->level      = level;

  free(cmid);
  free(invi);
  free(vgood);
  free(vbad);

  cf_list_init(&post->flags);

  len = gdome_nl_length(nl_thr,&e);
  for(i=0,z=0;i<len;++i) {
    n = gdome_nl_item(nl_thr,i,&e);
    name = gdome_n_nodeName(n,&e);

    if(cf_strcmp(name->str,"Header") == 0) {
      flt_xmlstorage_handle_header(post,n);

      if(!thread->newest || post->date > thread->newest->date) thread->newest = post;
      if(!thread->oldest || post->date < thread->oldest->date) thread->oldest = post;
    }
    else if(cf_strcmp(name->str,"MessageContent") == 0) {
      cnt = xml_get_node_value(n);

      if(cnt) {
        post->content.content  = cnt;
        post->content.len      = strlen(cnt);
        post->content.reserved = post->content.len;
      }
      else {
        cf_log(CF_ERR|CF_FLSH,__FILE__,__LINE__,"error in thread file of thread %llu: could not get content!\n",thread->tid);
        exit(-1);
      }
    }
    else if(cf_strcmp(name->str,"Message") == 0) {
      cmid = xml_get_attribute(n,"id");
      if((msg_ind = flt_xmlstorage_gemessage_t_node(msg_elem_index,cmid)) == NULL) {
        cf_log(CF_ERR|CF_FLSH,__FILE__,__LINE__,"error getting index element in thread %llu, message %llu\n",thread->tid,post->mid);
        exit(-1);
      }
      free(cmid);

      p = cf_alloc(NULL,1,sizeof(*p),CF_ALLOC_CALLOC);
      if(thread->last != NULL) thread->last->next = p;

      p->prev      = thread->last;
      thread->last = p;

      flt_xmlstorage_create_threadtree(forum,thread,p,msg_ind,n,level+1);
      gdome_n_unref(msg_ind,&e);
    }

    gdome_str_unref(name);
    gdome_n_unref(n,&e);
  }

  gdome_nl_unref(nl_thr,&e);
}
/* }}} */

/* {{{ flt_xmlstorage_handle_header */
void flt_xmlstorage_handle_header(posting_t *p,GdomeNode *n) {
  GdomeException exc;
  u_char *tmp,*ls;

  GdomeNodeList     *nl       = gdome_n_childNodes(n,&exc);
  GdomeNode         *author   = gdome_nl_item(nl,0,&exc);
  GdomeNode         *category = gdome_nl_item(nl,1,&exc);
  GdomeNode         *subject  = gdome_nl_item(nl,2,&exc);
  GdomeNode         *date     = gdome_nl_item(nl,3,&exc);
  GdomeNode         *flags    = gdome_nl_item(nl,4,&exc);

  GdomeNodeList     *a_nl     = gdome_n_childNodes(author,&exc);
  GdomeNode         *a_name   = gdome_nl_item(a_nl,0,&exc);
  GdomeNode         *a_email  = gdome_nl_item(a_nl,1,&exc);
  GdomeNode         *a_hp     = gdome_nl_item(a_nl,2,&exc);
  GdomeNode         *a_img    = gdome_nl_item(a_nl,3,&exc);

  GdomeNodeList *flags_nl = gdome_n_childNodes(flags,&exc);
  GdomeNode *n1;

  unsigned long len,i;

  posting_flag_t flag;

  ls = xml_get_attribute(date,"longSec");
  p->date          = strtol(ls,NULL,10);
  free(ls);

  tmp = xml_get_node_value(a_name);
  cf_str_char_set(&p->user.name,tmp,strlen(tmp));
  free(tmp);

  tmp = xml_get_node_value(subject);
  cf_str_char_set(&p->subject,tmp,strlen(tmp));
  free(tmp);

  tmp = xml_get_node_value(category);
  if(tmp) {
    cf_str_char_set(&p->category,tmp,strlen(tmp));
    free(tmp);
  }

  tmp = xml_get_node_value(a_email);
  if(tmp) {
    cf_str_char_set(&p->user.email,tmp,strlen(tmp));
    free(tmp);
  }

  tmp = xml_get_node_value(a_hp);
  if(tmp) {
    cf_str_char_set(&p->user.hp,tmp,strlen(tmp));
    free(tmp);
  }

  tmp = xml_get_node_value(a_img);
  if(tmp) {
    cf_str_char_set(&p->user.img,tmp,strlen(tmp));
    free(tmp);
  }

  len = gdome_nl_length(flags_nl,&exc);
  for(i=0;i<len;i++) {
    n1 = gdome_nl_item(flags_nl,i,&exc);
    memset(&flag,0,sizeof(flag));

    flag.name = xml_get_attribute(n1,"name");
    flag.val  = xml_get_node_value(n1);

    cf_list_append(&p->flags,&flag,sizeof(flag));

    gdome_n_unref(n1,&exc);
  }

  gdome_n_unref(flags,&exc);
  gdome_nl_unref(flags_nl,&exc);
  gdome_n_unref(a_name,&exc);
  gdome_n_unref(a_email,&exc);
  gdome_n_unref(a_hp,&exc);
  gdome_n_unref(a_img,&exc);
  gdome_nl_unref(a_nl,&exc);
  gdome_n_unref(author,&exc);
  gdome_n_unref(category,&exc);
  gdome_n_unref(subject,&exc);
  gdome_n_unref(date,&exc);
  gdome_nl_unref(nl,&exc);
}
/* }}} */

/* {{{ flt_xmlstorage_stringify_posting */
posting_t *flt_xmlstorage_stringify_posting(GdomeDocument *doc1,GdomeElement *t1,GdomeDocument *doc2,GdomeElement *t2,posting_t *p) {
  int lvl = p->level;
  GdomeException e;
  cf_string_t mstr;

  cf_list_element_t *elem;
  posting_flag_t *flag;

  GdomeDOMString *str;
  GdomeCDATASection *cd;

  GdomeElement *elem1,*elem2;
  GdomeElement *m1 = xml_create_element(doc1,"Message");
  GdomeElement *m2 = xml_create_element(doc2,"Message");

  GdomeElement *header1 = xml_create_element(doc1,"Header");
  GdomeElement *header2 = xml_create_element(doc2,"Header");

  GdomeElement *author1 = xml_create_element(doc1,"Author");
  GdomeElement *author2 = xml_create_element(doc2,"Author");

  GdomeElement *flags1 = xml_create_element(doc1,"Flags");
  GdomeElement *flags2 = xml_create_element(doc2,"Flags");

  GdomeElement *cnt = xml_create_element(doc2,"MessageContent");
  
  gdome_el_appendChild(m1,(GdomeNode *)header1,&e);
  gdome_el_appendChild(m2,(GdomeNode *)header2,&e);

  gdome_el_appendChild(header1,(GdomeNode *)author1,&e);
  gdome_el_appendChild(header2,(GdomeNode *)author2,&e);

  gdome_el_appendChild(m2,(GdomeNode *)cnt,&e);

  /* {{{ attributes */
  /* invisible flag */
  xml_set_attribute(m1,"invisible",p->invisible ? "1" : "0");
  xml_set_attribute(m2,"invisible",p->invisible ? "1" : "0");

  cf_str_init_growth(&mstr,20);
  cf_str_char_append(&mstr,'m');
  cf_uint64_to_str(&mstr,p->mid);

  xml_set_attribute(m1,"id",mstr.content);

  /* unique id, id and ip */
  if(p->unid.len) xml_set_attribute(m1,"unid",p->unid.content);

  xml_set_attribute(m2,"id",mstr.content);
  if(!for_archive || archive_ip) xml_set_attribute(m2,"ip",p->user.ip.content);

  cf_str_cleanup(&mstr);

  /* voting attributes */
  cf_uint32_to_str(&mstr,p->votes_good);
  xml_set_attribute(m1,"votingGood",mstr.content);
  xml_set_attribute(m2,"votingGood",mstr.content);
  cf_str_cleanup(&mstr);

  cf_uint32_to_str(&mstr,p->votes_bad);
  xml_set_attribute(m1,"votingBad",mstr.content);
  xml_set_attribute(m2,"votingBad",mstr.content);
  cf_str_cleanup(&mstr);
  /* }}} */

  /* {{{ name */
  elem1 = xml_create_element(doc1,"Name");
  elem2 = xml_create_element(doc2,"Name");

  xml_set_value(doc1,elem1,p->user.name.content);
  xml_set_value(doc2,elem2,p->user.name.content);

  gdome_el_appendChild(author1,(GdomeNode *)elem1,&e);
  gdome_el_appendChild(author2,(GdomeNode *)elem2,&e);

  gdome_el_unref(elem1,&e);
  gdome_el_unref(elem2,&e);
  /* }}} */

  /* {{{ email address */
  elem1 = xml_create_element(doc1,"Email");
  elem2 = xml_create_element(doc2,"Email");

  if(p->user.email.len) {
    xml_set_value(doc1,elem1,p->user.email.content);
    xml_set_value(doc2,elem2,p->user.email.content);
  }

  gdome_el_appendChild(author1,(GdomeNode *)elem1,&e);
  gdome_el_appendChild(author2,(GdomeNode *)elem2,&e);

  gdome_el_unref(elem1,&e);
  gdome_el_unref(elem2,&e);
  /* }}} */

  /* {{{ homepage url */
  elem1 = xml_create_element(doc1,"HomepageUrl");
  elem2 = xml_create_element(doc2,"HomepageUrl");

  if(p->user.hp.len) {
    xml_set_value(doc1,elem1,p->user.hp.content);
    xml_set_value(doc2,elem2,p->user.hp.content);
  }

  gdome_el_appendChild(author1,(GdomeNode *)elem1,&e);
  gdome_el_appendChild(author2,(GdomeNode *)elem2,&e);

  gdome_el_unref(elem1,&e);
  gdome_el_unref(elem2,&e);
  /* }}} */

  /* {{{ image url */
  elem1 = xml_create_element(doc1,"ImageUrl");
  elem2 = xml_create_element(doc2,"ImageUrl");

  if(p->user.img.len) {
    xml_set_value(doc1,elem1,p->user.img.content);
    xml_set_value(doc2,elem2,p->user.img.content);
  }

  gdome_el_appendChild(author1,(GdomeNode *)elem1,&e);
  gdome_el_appendChild(author2,(GdomeNode *)elem2,&e);

  gdome_el_unref(elem1,&e);
  gdome_el_unref(elem2,&e);
  /* }}} */

  /* {{{ category */
  elem1 = xml_create_element(doc1,"Category");
  elem2 = xml_create_element(doc2,"Category");

  if(p->category.len) {
    xml_set_value(doc1,elem1,p->category.content);
    xml_set_value(doc2,elem2,p->category.content);
  }

  gdome_el_appendChild(header1,(GdomeNode *)elem1,&e);
  gdome_el_appendChild(header2,(GdomeNode *)elem2,&e);

  gdome_el_unref(elem1,&e);
  gdome_el_unref(elem2,&e);
  /* }}} */

  /* {{{ subject */
  elem1 = xml_create_element(doc1,"Subject");
  elem2 = xml_create_element(doc2,"Subject");

  xml_set_value(doc1,elem1,p->subject.content);
  xml_set_value(doc2,elem2,p->subject.content);

  gdome_el_appendChild(header1,(GdomeNode *)elem1,&e);
  gdome_el_appendChild(header2,(GdomeNode *)elem2,&e);

  gdome_el_unref(elem1,&e);
  gdome_el_unref(elem2,&e);
  /* }}} */

  /* {{{ date */
  elem1 = xml_create_element(doc1,"Date");
  elem2 = xml_create_element(doc2,"Date");

  cf_uint32_to_str(&mstr,(u_int32_t)p->date);
  xml_set_attribute(elem1,"longSec",mstr.content);
  xml_set_attribute(elem2,"longSec",mstr.content);
  cf_str_cleanup(&mstr);

  gdome_el_appendChild(header1,(GdomeNode *)elem1,&e);
  gdome_el_appendChild(header2,(GdomeNode *)elem2,&e);

  gdome_el_unref(elem1,&e);
  gdome_el_unref(elem2,&e);
  /* }}} */

  /* {{{ create <Flag> elements */
  gdome_el_appendChild(header1,(GdomeNode *)flags1,&e);
  gdome_el_appendChild(header2,(GdomeNode *)flags2,&e);

  for(elem=p->flags.elements;elem;elem=elem->next) {
    flag = (posting_flag_t *)elem->data;

    elem1 = xml_create_element(doc1,"Flag");
    xml_set_attribute(elem1,"name",flag->name);
    xml_set_value(doc1,elem1,flag->val);

    elem2 = xml_create_element(doc2,"Flag");
    xml_set_attribute(elem2,"name",flag->name);
    xml_set_value(doc2,elem2,flag->val);

    gdome_el_appendChild(flags1,(GdomeNode *)elem1,&e);
    gdome_el_appendChild(flags2,(GdomeNode *)elem2,&e);

    gdome_el_unref(elem1,&e);
    gdome_el_unref(elem2,&e);
  }
  /* }}} */

  /* {{{ content */
  str = gdome_str_mkref_dup(p->content.content);

  cd = gdome_doc_createCDATASection(doc2,str,&e);
  gdome_el_appendChild(cnt,(GdomeNode *)cd,&e);

  gdome_cds_unref(cd,&e);
  gdome_str_unref(str);

  gdome_el_appendChild(t1,(GdomeNode *)m1,&e);
  gdome_el_appendChild(t2,(GdomeNode *)m2,&e);
  /* }}} */

  /* {{{ cleanup */
  gdome_el_unref(header1,&e);
  gdome_el_unref(header2,&e);

  gdome_el_unref(author1,&e);
  gdome_el_unref(author2,&e);

  gdome_el_unref(cnt,&e);

  gdome_el_unref(flags1,&e);
  gdome_el_unref(flags2,&e);
  /* }}} */

  for(p=p->next;p;) {
    if(p->level > lvl) p = flt_xmlstorage_stringify_posting(doc1,m1,doc2,m2,p);
    else {
      gdome_el_unref(m1,&e);
      gdome_el_unref(m2,&e);

      return p;
    }
  }

  gdome_el_unref(m1,&e);
  gdome_el_unref(m2,&e);

  return NULL;
}
/* }}} */

/* {{{ flt_xmlstorage_thread2xml */
GdomeDocument *flt_xmlstorage_thread2xml(GdomeDOMImplementation *impl,GdomeDocument *doc1,thread_t *t,int for_archive) {
  GdomeException e;
  GdomeDocument *doc2   = xml_create_doc(impl,"Forum",FORUM_DTD);
  GdomeElement *thread1 = xml_create_element(doc1,"Thread");
  GdomeElement *thread2 = xml_create_element(doc2,"Thread");
  GdomeElement *root;
  cf_string_t str;

  cf_str_init_growth(&str,10);
  cf_str_char_append(&str,'t');
  cf_uint64_to_str(&str,t->tid);

  xml_set_attribute(thread1,"id",str.content);
  xml_set_attribute(thread2,"id",str.content);
  cf_str_cleanup(&str);

  flt_xmlstorage_stringify_posting(doc1,thread1,doc2,thread2,t->postings);

  root = gdome_doc_documentElement(doc1,&e);
  gdome_el_appendChild(root,(GdomeNode *)thread1,&e);
  gdome_el_unref(root,&e);

  root = gdome_doc_documentElement(doc2,&e);
  gdome_el_appendChild(root,(GdomeNode *)thread2,&e);
  gdome_el_unref(root,&e);

  gdome_el_unref(thread1,&e);
  gdome_el_unref(thread2,&e);

  return doc2;
}
/* }}} */

/* {{{ flt_xmlstorage_threadlist_writer */
int flt_xmlstorage_threadlist_writer(forum_t *forum) {
  thread_t *t;
  u_int64_t ltid,lmid;
  u_char buff[256];
  pid_t pid;

  cf_name_value_t *mpath = cf_cfg_get_first_value(&fo_default_conf,forum->name,"XmlStorage:MessagePath");

  GdomeException e;
  GdomeDOMImplementation *impl;
  GdomeDocument *doc,*doc_thread;
  GdomeElement *elm;
  cf_string_t str;

  /* we have to fork() because of the fucking memory leaks... */
  pid = fork();
  switch(pid) {
    case -1:
      cf_log(CF_ERR|CF_FLSH,__FILE__,__LINE__,"FORK() ERROR! %s\n",strerror(errno));
      return FLT_EXIT;

    case 0:
      break;

    default:
      waitpid(pid,NULL,0);
      return FLT_OK;
  }


  /*
   * Because of the fork() call above, we don't need
   * locking
   */

  /* first, ensure that the path to forum.xml exists */
  cf_make_path(mpath->values[0],0755);

  t    = forum->threads.list;
  ltid = forum->threads.last_tid;
  lmid = forum->threads.last_mid;

  impl = gdome_di_mkref();
  doc = xml_create_doc(impl,"Forum",FORUM_DTD);
  elm = gdome_doc_documentElement(doc,&e);

  cf_log(CF_DBG,__FILE__,__LINE__,"tid %"PRIu64", mid: %"PRIu64"\n",ltid,lmid);

  cf_str_init_growth(&str,10);

  if(ltid) {
    cf_str_char_append(&str,'t');
    cf_uint64_to_str(&str,ltid);
    xml_set_attribute(elm,"lastThread",str.content);
    cf_str_cleanup(&str);
  }

  if(lmid) {
    cf_str_char_append(&str,'m');
    cf_uint64_to_str(&str,lmid);
    xml_set_attribute(elm,"lastMessage",str.content);
    cf_str_cleanup(&str);
  }

  for(;t;t=t->next) {
    doc_thread = flt_xmlstorage_thread2xml(impl,doc,t,0);

    /* save doc to file... */
    cf_str_cstr_append(&str,mpath->values[0]);
    cf_str_char_append(&str,'t');
    cf_uint64_to_str(&str,t->tid);
    cf_str_chars_append(&str,".xml",4);
    cf_log(CF_DBG,__FILE__,__LINE__,"save file: %s\n",str.content);
    if(!gdome_di_saveDocToFile(impl,doc_thread,str.content,0,&e)) {
      cf_log(CF_ERR|CF_FLSH,__FILE__,__LINE__,"ERROR! COULD NOT WRITE XML FILE! Trying to write it to /tmp/%s/t%llu.xml\n",forum->name,t->tid);

      snprintf(buff,256,"/tmp/%s",forum->name);
      cf_make_path(buff,0755);

      snprintf(buff,256,"/tmp/%s/t%"PRIu64".xml",forum->name,t->tid);
      gdome_di_saveDocToFile(impl,doc_thread,buff,0,&e);
    }

    gdome_doc_unref(doc_thread,&e);
    cf_str_cleanup(&str);
  }

  cf_str_cstr_append(&str,mpath->values[0]);
  cf_str_chars_append(&str,"/forum.xml",10);
  if(!gdome_di_saveDocToFile(impl,doc,str.content,0,&e)) {
    cf_log(CF_ERR|CF_FLSH,__FILE__,__LINE__,"ERROR! COULD NOT WRITE XML FILE! Trying to write it to /tmp/%s/forum.xml\n",forum->name);

    snprintf(buff,256,"/tmp/%s",forum->name);
    cf_make_path(buff,0755);
    snprintf(buff,256,"/tmp/%s/forum.xml",forum->name);
    gdome_di_saveDocToFile(impl,doc,buff,0,&e);
  }

  cf_str_cleanup(&str);

  gdome_el_unref(elm,&e);
  gdome_doc_unref(doc,&e);
  gdome_di_unref(impl,&e);

  exit(0);
}
/* }}} */

/* {{{ flt_xmlstorage_archive_threads */
int flt_xmlstorage_archive_threads(forum_t *forum,thread_t **threads,size_t len) {
  pid_t pid;
  size_t i;

  u_char buff[512];
  cf_string_t str;

  GdomeException e;
  GdomeDOMImplementation *impl;
  GdomeDocument *doc,*doc_thread;

  struct tm t;
  struct stat st;

  unsigned long olen = 0;

  cf_name_value_t *path,*mpath;

  pid = fork();
  switch(pid) {
    case -1:
      fprintf(stderr,"flt_xmlstorage: fork(): could not fork: %s\n",strerror(errno));
      exit(-1);
    case 0:
      break;
    default:
      waitpid(pid,NULL,0);
      return FLT_OK;
  }

  path  = cf_cfg_get_first_value(&fo_default_conf,forum->name,"OL:ArchivePath");
  mpath = cf_cfg_get_first_value(&fo_default_conf,forum->name,"XmlStorage:MessagePath");
  impl  = gdome_di_mkref();

  cf_str_init(&str);

  for(i=0;i<len;++i) {
    localtime_r(&threads[i]->postings->date,&t);
    cf_str_cstr_append(&str,path->values[0]);
    cf_str_char_append(&str,'/');
    cf_uint16_to_str(&str,(u_int16_t)(t.tm_year+1900));
    cf_str_char_append(&str,'/');
    cf_uint16_to_str(&str,(u_int16_t)(t.tm_mon+1));

    if(stat(str.content,&st) == -1) {
      if(cf_make_path(str.content,0755) != 0) {
        cf_log(CF_ERR|CF_FLSH,__FILE__,__LINE__,"Error creating path %s! (%s)\n",buff,strerror(errno));
        exit(-1);
      }

      doc = xml_create_doc(impl,"Forum",FORUM_DTD);
    }
    else {
      cf_str_chars_append(&str,"/index.xml",10);
      if((doc = gdome_di_createDocFromURI(impl,str.content,0,&e)) == NULL) {
        cf_log(CF_ERR|CF_FLSH,__FILE__,__LINE__,"ERROR PARSING INDEX DOCUMENT! (%s)\n",buff);
        exit(-1);
      }
      str.len -= 10;
    }

    doc_thread = flt_xmlstorage_thread2xml(impl,doc,threads[i],1);

    olen = str.len;
    cf_str_chars_append(&str,"/t",2);
    cf_uint64_to_str(&str,threads[i]->tid);
    cf_str_chars_append(&str,".xml",4);

    if(!gdome_di_saveDocToFile(impl,doc_thread,str.content,0,&e)) {
      cf_log(CF_ERR|CF_FLSH,__FILE__,__LINE__,"ERROR! COULD NOT WRITE XML FILE! Trying to write it to /tmp/%s/archive/t%llu.xml\n",forum->name,threads[i]->tid);

      snprintf(buff,256,"/tmp/%s/archive",forum->name);
      cf_make_path(buff,0755);

      snprintf(buff,256,"/tmp/%s/archive/t%"PRIu64".xml",forum->name,threads[i]->tid);
      gdome_di_saveDocToFile(impl,doc_thread,buff,0,&e);
    }

    str.len = olen;
    cf_str_chars_append(&str,"/index.xml",10);
    if(!gdome_di_saveDocToFile(impl,doc,str.content,0,&e)) cf_log(CF_ERR|CF_FLSH,__FILE__,__LINE__,"ERROR! COULD NOT WRITE INDEX XML FILE!\n",forum->name,threads[i]->tid);

    cf_str_cleanup(&str);

    gdome_doc_unref(doc_thread,&e);
    gdome_doc_unref(doc,&e);

    snprintf(buff,512,"%s/t%"PRIu64".xml",mpath->values[0],threads[i]->tid);
    unlink(buff);
  }

  gdome_di_unref(impl,&e);
  exit(0);
}
/* }}} */

/* {{{ flt_xmlstorage_remove_thread */
int flt_xmlstorage_remove_thread(forum_t *forum,thread_t *thr) {
  cf_name_value_t *mpath = cf_cfg_get_first_value(&fo_default_conf,forum->name,"XmlStorage:MessagePath");
  u_char buff[512];

  snprintf(buff,512,"%s/t%"PRIu64".xml",mpath->values[0],thr->tid);
  unlink(buff);

  return FLT_OK;
}
/* }}} */

cf_conf_opt_t flt_xmlstorage_config[] = {
  { "XmlStorage:MessagePath", cf_handle_command, CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, &fo_default_conf },
  { "OL:ArchivePath", cf_handle_command, CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, &fo_default_conf },
  { NULL, NULL, 0, NULL }
};

cf_handler_config_t flt_xmlstorage_handlers[] = {
  { DATA_LOADING_HANDLER,   flt_xmlstorage_make_forumtree },
  { THRDLST_WRITE_HANDLER,  flt_xmlstorage_threadlist_writer },
  { ARCHIVE_THREAD_HANDLER, flt_xmlstorage_archive_threads },
  { REMOVE_THREAD_HANDLER,  flt_xmlstorage_remove_thread },
  { 0, NULL }
};

cf_module_config_t flt_xmlstorage = {
  MODULE_MAGIC_COOKIE,
  flt_xmlstorage_config,
  flt_xmlstorage_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};

/* eof */
