/**
 * \file flt_xmlstorage.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 * \brief XML storage plugin
 *
 * This file contains a storage plugin implementation and
 * uses XML to save all data
 */

/* {{{ Initial comments */
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

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>

#include <gdome.h>

struct sockaddr_un;

#include "cf_pthread.h"

#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "readline.h"

#include "serverlib.h"
#include "fo_server.h"

#include "xml_handling.h"
/* }}} */

#define CF_SORT_ASCENDING 1
#define CF_SORT_DESCENDING 2

static int sort_threads  = 0;
static int sort_messages = 0;

void flt_xmlstorage_create_threadtree(t_forum *forum,t_thread *thread,t_posting *post,GdomeNode *msg_elem_index,GdomeNode *msg_elem_thread,short level);
void flt_xmlstorage_handle_header(t_posting *p,GdomeNode *n);

/* {{{ flt_xmlstorage_cmp_thread */
int flt_xmlstorage_cmp_thread(const void *a,const void *b) {
  t_thread *ta = *((t_thread **)a);
  t_thread *tb = *((t_thread **)b);

  if(ta->tid > tb->tid) return sort_threads == CF_SORT_ASCENDING ? 1 : -1;
  else return sort_threads == CF_SORT_ASCENDING ? -1 : 1;
}
/* }}} */

/* {{{ t_h_p */
/** This struct is used to sort the thread list. It contains a hierarchical structure. */
typedef struct s_h_p {
  t_posting *node; /**< The pointer to the posting */
  long len; /**< The number of postings in this hierarchy level */
  struct s_h_p *childs; /**< The answers to this posting */
} t_h_p;
/* }}} */

/* {{{ flt_xmlstorage_hirarchy_them */
/**
 * Insert the postings to a hierarchy level. This function works recursively.
 * \param parent The parent t_h_p structure
 * \param pst The posting pointer
 * \return The pointer to the next posting in a hierarchy level smaller or equal to ours.
 */
t_posting *flt_xmlstorage_hirarchy_them(t_h_p *parent,t_posting *pst) {
  t_posting *p;
  int lvl;

  p   = pst;
  lvl = p->level;

  while(p) {
    if(p->level == lvl) {
      parent->childs = fo_alloc(parent->childs,parent->len+1, sizeof(t_h_p),FO_ALLOC_REALLOC);
      memset(&parent->childs[parent->len],0,sizeof(t_h_p));

      parent->childs[parent->len++].node = p;
      p = p->next;
    }
    else if(p->level > lvl) {
      p = flt_xmlstorage_hirarchy_them(&parent->childs[parent->len-1],p);
    }
    else {
      return p;
    }
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
void flt_xmlstorage_quicksort_posts(t_h_p *list,long low,long high) {
  long i = low,j = high;
  t_h_p *pivot,tmp;

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

/* {{{flt_xmlstorage_sort_them */
/**
 * Sorting function.
 * \param node The t_h_p structure node
 */
void flt_xmlstorage_sort_them(t_h_p *node) {
  long i;

  if(node->len) {
    flt_xmlstorage_quicksort_posts(node->childs,0,node->len-1);

    for(i=0;i<node->len;i++) {
     flt_xmlstorage_sort_them(&node->childs[i]);
    }
  }

}
/* }}} */

/* {{{flt_xmlstorage_free_structs */
/**
 * This function cleans up a hierarchical structure
 * \param node The t_h_p node
 */
void flt_xmlstorage_free_structs(t_h_p *node) {
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
 * \param node The t_h_p node
 * \param sort The sorting direction
 * \return A posting in this or lower than this hierarchy level
 */
t_posting *flt_xmlstorage_serialize_them(t_h_p *node) {
  long i;
  t_posting *p;

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

/* {{{ flt_xmlstorage_sort_messages */
void flt_xmlstorage_sort_messages(t_thread *thread) {
  t_h_p *first = fo_alloc(NULL,1,sizeof(*first),FO_ALLOC_CALLOC);
  t_posting *p,*p1;

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
int flt_xmlstorage_make_forumtree(t_forum *forum) {
  t_name_value *p = cfg_get_first_value(&fo_default_conf,forum->name,"MessagePath");
  t_name_value *sort_t = cfg_get_first_value(&fo_server_conf,forum->name,"SortThreads");
  t_name_value *sort_m = cfg_get_first_value(&fo_server_conf,forum->name,"SortMessages");
  t_string path;

  u_char *ctid;
  unsigned long length,i;
  t_thread *thread;
  t_array ary;
  u_char buff[50];

  /* {{{ xml definitions */
  GdomeException e;
  GdomeDOMImplementation *di = gdome_di_mkref();
  GdomeDocument *doc_index,*doc_thread;
  GdomeDOMString *thread_str = gdome_str_mkref("Thread");
  GdomeNode *n,*n1,*n2,*root;
  GdomeNodeList *nl;
  /* }}} */

  array_init(&ary,sizeof(thread),NULL);

  sort_threads  = cf_strcmp(sort_t->values[0],"ascending") == 0 ? CF_SORT_ASCENDING : CF_SORT_DESCENDING;
  sort_messages = cf_strcmp(sort_m->values[0],"ascending") == 0 ? CF_SORT_ASCENDING : CF_SORT_DESCENDING;

  str_init(&path);
  str_char_set(&path,p->values[0],strlen(p->values[0]));

  if(path.content[path.len-1] != '/') str_char_append(&path,'/');
  str_chars_append(&path,"forum.xml",9);

  if((doc_index = gdome_di_createDocFromURI(di,path.content,GDOME_LOAD_VALIDATING,&e)) == NULL) {
    cf_log(CF_ERR|CF_FLSH,__FILE__,__LINE__,"error loading file (%s)!\n",path.content);
    exit(-1);
  }

  path.len -= 9;

  nl = gdome_doc_getElementsByTagName(doc_index,thread_str,&e);
  length = gdome_nl_length(nl,&e);

  for(i=0;i<length;i++) {
    n    = gdome_nl_item(nl,i,&e);
    ctid = xml_get_attribute(n,"id");

    str_chars_append(&path,ctid,strlen(ctid));
    str_chars_append(&path,".xml",4);
    if((doc_thread = gdome_di_createDocFromURI(di,path.content,GDOME_LOAD_VALIDATING,&e)) == NULL) {
      cf_log(CF_ERR|CF_FLSH,__FILE__,__LINE__,"error loading file (%s)!\n",path.content);
      exit(-1);
    }
    path.len -= strlen(ctid) + 4;

    /* document parsed, now we have to get the Thread element in the thread file */
    root = (GdomeNode *)gdome_doc_documentElement(doc_thread,&e);
    n1   = xml_get_first_element_by_name(root,"Message");
    n2   = xml_get_first_element_by_name(n,"Message");

    thread           = fo_alloc(NULL,1,sizeof(*thread),FO_ALLOC_CALLOC);
    thread->tid      = str_to_u_int64(ctid+1);
    thread->postings = fo_alloc(NULL,1,sizeof(*thread->postings),FO_ALLOC_CALLOC);
    thread->last     = thread->postings;

    flt_xmlstorage_create_threadtree(forum,thread,thread->postings,n2,n1,0);
    array_push(&ary,&thread);

    cf_register_thread(forum,thread);

    /* initialize thread lock */
    snprintf(buff,50,"t%llu",thread->tid);
    cf_rwlock_init(buff,&thread->lock);

    gdome_n_unref(n1,&e);
    gdome_n_unref(n2,&e);
    gdome_n_unref(n,&e);
    gdome_n_unref(root,&e);
    gdome_doc_unref(doc_thread,&e);
  }

  /* {{{ xml cleanup */
  gdome_str_unref(thread_str);
  gdome_nl_unref(nl,&e);
  gdome_doc_unref(doc_index,&e);
  /* }}} */

  str_cleanup(&path);

  array_sort(&ary,flt_xmlstorage_cmp_thread);

  for(i=0;i<ary.elements;i++) {
    thread = *((t_thread **)array_element_at(&ary,i));

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
    flt_xmlstorage_sort_messages(thread);
  }

  return FLT_OK;
}
/* }}} */

/* {{{ flt_xmlstorage_get_message_node */
GdomeNode *flt_xmlstorage_get_message_node(GdomeNode *parent,const u_char *mid) {
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
void flt_xmlstorage_create_threadtree(t_forum *forum,t_thread *thread,t_posting *post,GdomeNode *msg_elem_index,GdomeNode *msg_elem_thread,short level) {
  u_char *unid  = xml_get_attribute(msg_elem_index,"unid");
  u_char *cmid  = xml_get_attribute(msg_elem_thread,"id");
  u_char *invi  = xml_get_attribute(msg_elem_thread,"invisible");
  u_char *vgood = xml_get_attribute(msg_elem_thread,"votingGood");
  u_char *vbad  = xml_get_attribute(msg_elem_thread,"votingBad");
  u_char *ip    = xml_get_attribute(msg_elem_thread,"ip");
  u_char *cnt;

  unsigned long len,i,z = 0;

  /* {{{ xml elements */
  GdomeException e;
  GdomeNodeList *nl_thr = gdome_n_childNodes(msg_elem_thread,&e);

  GdomeNode *n,*msg_ind;
  GdomeDOMString *name;
  /* }}} */

  t_posting *p;

  post->unid.content = unid;
  post->unid.len   = post->unid.reserved = strlen(unid);
  post->mid        = str_to_u_int64(cmid+1);
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

  /* {{{ handle header, message content and child messages */
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
      if((msg_ind = flt_xmlstorage_get_message_node(msg_elem_index,cmid)) == NULL) {
        cf_log(CF_ERR|CF_FLSH,__FILE__,__LINE__,"error getting index element in thread %llu, message %llu\n",thread->tid,post->mid);
        exit(-1);
      }
      free(cmid);

      p = fo_alloc(NULL,1,sizeof(*p),FO_ALLOC_CALLOC);
      if(thread->last != NULL) thread->last->next = p;

      p->prev      = thread->last;
      thread->last = p;

      flt_xmlstorage_create_threadtree(forum,thread,p,msg_ind,n,level+1);
      gdome_n_unref(msg_ind,&e);
    }

    gdome_str_unref(name);
    gdome_n_unref(n,&e);
  }
  /* }}} */

  /* {{{ xml destruction */
  gdome_nl_unref(nl_thr,&e);
  /* }}} */
}
/* }}} */

/* {{{ flt_xmlstorage_handle_header */
void flt_xmlstorage_handle_header(t_posting *p,GdomeNode *n) {
  GdomeException     exc;
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

  t_posting_flag flag;

  /* {{{ get date */
  ls = xml_get_attribute(date,"longSec");
  p->date          = strtol(ls,NULL,10);
  /* }}} */

  /* {{{ get author name */
  tmp = xml_get_node_value(a_name);
        str_char_set(&p->user.name,tmp,strlen(tmp));
        free(tmp);
  /* }}} */

  /* {{{ get subject */
  tmp = xml_get_node_value(subject);
        str_char_set(&p->subject,tmp,strlen(tmp));
        free(tmp);
  /* }}} */

  /* {{{ get category */
  tmp = xml_get_node_value(category);
        if(tmp) {
          str_char_set(&p->category,tmp,strlen(tmp));
                free(tmp);
        }
  /* }}} */

  /* {{{ get email */
        tmp = xml_get_node_value(a_email);
        if(tmp) {
          str_char_set(&p->user.email,tmp,strlen(tmp));
                free(tmp);
        }
  /* }}} */

  /* {{{ get homepage */
  tmp = xml_get_node_value(a_hp);
        if(tmp) {
          str_char_set(&p->user.hp,tmp,strlen(tmp));
                free(tmp);
        }
  /* }}} */

  /* {{{ get image */
  tmp = xml_get_node_value(a_img);
        if(tmp) {
          str_char_set(&p->user.img,tmp,strlen(tmp));
                free(tmp);
        }
  /* }}} */

  /* {{{ get flags */
  len = gdome_nl_length(flags_nl,&exc);
  for(i=0;i<len;i++) {
    n1 = gdome_nl_item(flags_nl,i,&exc);
    memset(&flag,0,sizeof(flag));

    flag.name = xml_get_attribute(n1,"name");
    flag.val  = xml_get_node_value(n1);

    cf_list_append(&p->flags,&flag,sizeof(flag));

    gdome_n_unref(n1,&exc);
  }
  /* }}} */

  /* {{{ xml destruction */
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
  /* }}} */
}
/* }}} */

/* {{{ flt_xmlstorage_threadlist_writer */
int flt_xmlstorage_threadlist_writer(t_forum *forum) {
  return FLT_OK;
}
/* }}} */

/* {{{ flt_xmlstorage_archive_thread */
int flt_xmlstorage_archive_thread(t_forum *forum,t_thread *thread) {
  return FLT_OK;
}
/* }}} */


t_conf_opt flt_xmlstorage_config[] = {
  { "MessagePath", handle_command, CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_LOCAL, &fo_default_conf },
  { "ArchivePath", handle_command, CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_LOCAL, &fo_default_conf },
  { NULL, NULL, 0, NULL }
};

t_handler_config flt_xmlstorage_handlers[] = {
  { DATA_LOADING_HANDLER,   flt_xmlstorage_make_forumtree },
  { THRDLST_WRITE_HANDLER,  flt_xmlstorage_threadlist_writer },
  { ARCHIVE_THREAD_HANDLER, flt_xmlstorage_archive_thread },
  { 0, NULL }
};

t_module_config flt_xmlstorage = {
  flt_xmlstorage_config,
  flt_xmlstorage_handlers,
  NULL,
  NULL,
  NULL,
  NULL
};

/* eof */
