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

#include "cf_pthread.h"

#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "readline.h"
#include "fo_server.h"
#include "initfinish.h"
#include "charconvert.h"
#include "xml_handling.h"
#include "serverlib.h"
/* }}} */


/** This struct is used to sort the thread list. It contains a hierarchical structure. */
typedef struct s_h_p {
  t_posting *node; /**< The pointer to the posting */
  long len; /**< The number of postings in this hierarchy level */
  struct s_h_p *childs; /**< The answers to this posting */
} t_h_p;

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
voidflt_xmlstorage_sort_them(t_h_p *node) {
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
voidflt_xmlstorage_free_structs(t_h_p *node) {
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
t_posting *serialize_them(t_h_p *node,int sort) {
  long i;
  t_posting *p;

  if(sort == 1) {
    if(node->len) {
      node->node->next = node->childs[0].node;

      for(i=0;i<node->len;i++) {
        if(node->childs[i].len) {
          p = flt_xmlstorage_serialize_them(&node->childs[i],sort);

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
          p = flt_xmlstorage_serialize_them(&node->childs[i],sort);

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

/* {{{ flt_xmlstorage_sort_postings */
/**
 * This function sorts the postings
 * \param posts The pointer to the first posting in the chain
 * \param sort The sorting direction
 */
void flt_xmlstorage_sort_postings(t_posting *posts,int sort) {
  t_h_p *first = fo_alloc(NULL,1,sizeof(t_h_p),FO_ALLOC_CALLOC);

  if(posts->next) {
    first->node = posts;
    flt_xmlstorage_hirarchy_them(first,posts);
    /*return;*/
  }
  else {
    free(first);
    return;
  }

 flt_xmlstorage_sort_them(first);
  flt_xmlstorage_serialize_them(first,sort);

 flt_xmlstorage_free_structs(first);
  free(first);
}
/* }}} */

/* {{{ flt_xmlstorage_quicksort_threads */
/**
 * This function is a quicksort function for the threads.
 * \param list The thread list
 * \param low The lower frontier of the field
 * \param high The higher frontier of the field
 */
void flt_xmlstorage_quicksort_threads(t_thread **list,long low,long high) {
  long i = low,j = high;
  t_thread *pivot,*tmp;

  pivot = list[(low+high)/2];

  do {
    while(i<high && list[i]->postings->date < pivot->postings->date) i++;
    while(j>low && list[j]->postings->date > pivot->postings->date) j--;

    if(i <= j) {
      tmp     = list[i];
      list[i] = list[j];
      list[j] = tmp;
      i++; j--;
    }
  } while(i <= j);

  if(low<j) flt_xmlstorage_quicksort_threads(list,low,j);
  if(i<high) flt_xmlstorage_quicksort_threads(list,i,high);
}
/* }}} */

/* {{{ flt_xmlstorage_sort_threadlist */
/**
 * This function does all the sorting work
 * \param head The head variable
 */
void flt_xmlstorage_sort_threadlist(t_head *head) {
  t_name_value *sort_thr_v = cfg_get_first_value(&fo_server_conf,NULL,"SortThreads");
  t_name_value *sort_msg_v = cfg_get_first_value(&fo_server_conf,NULL,"SortMessages");
  int threadnum = 0,reser = 0,i,sort_thr,sort_msg;
  t_thread **list = NULL,*tmp;
  t_posting *p,*p1;

  sort_thr = atoi(sort_thr_v->values[0]);
  sort_msg = atoi(sort_msg_v->values[0]);

  for(tmp=head->thread;tmp;tmp=tmp->next,threadnum++) {
    if(threadnum >= reser) {
      reser += 5;
      list   = fo_alloc(list,reser,sizeof(t_thread *),FO_ALLOC_REALLOC);
    }

    list[threadnum] = tmp;
  }

  flt_xmlstorage_quicksort_threads(list,0,threadnum-1);

  if(sort_thr == 1) {
    for(i=0;i<threadnum;i++) {
      flt_xmlstorage_sort_postings(list[i]->postings,sort_msg);

      /* reset prev-pointers */
      for(p=list[i]->postings,p1=NULL;p;p=p->next) {
        p->prev = p1;
        p1      = p;
      }

      if(i < threadnum-1) {
        list[i]->next = list[i+1];
        if(i==0) list[i]->prev = NULL;
        else     list[i]->prev = list[i-1];
      }
      else {
        list[i]->next = NULL;
        if(i==0) list[i]->prev = NULL;
        else     list[i]->prev = list[i-1];
      }
    }
  }
  else {
    for(i=threadnum-1;i>=0;i--) {
      flt_xmlstorage_sort_postings(list[i]->postings,sort_msg);

      if(i > 0) {
        list[i]->next = list[i-1];
        if(i<threadnum-1) list[i]->prev = list[i+1];
        else              list[i]->prev = NULL;
      }
      else {
        list[i]->next = NULL;
        if(i<threadnum-1) list[i]->prev = list[i+1];
        else              list[i]->prev = NULL;
      }
    }

    head->thread = list[threadnum-1];
  }

  free(list);
}
/* }}} */


/* {{{ flt_xmlstorage_make_forumtree
 * Returns:
 * Parameters:
 *   - t_configuration *cfg   the default configuration
 *
 * This function creates the forum tree
 *
 */
int flt_xmlstorage_make_forumtree(t_forum *forum) {
  int i = 0,len = 0,len1 = 0;
	t_string fname,tmp;
  GdomeException          exc;
  GdomeDocument          *doc;
  GdomeElement           *el;
  GdomeNodeList          *nl;
  t_name_value           *msgpath        = cfg_get_first_value(&fo_server_conf,NULL,"MessagePath");
  GdomeDOMImplementation *impl           = gdome_di_mkref();
  GdomeDOMString *thread_str             = gdome_str_mkref("Thread");
  GdomeDOMString         *tmpstr,*tmpstr1;
  u_char buff[50];
  t_thread *thread;
  GdomeDocument     *tmpdoc;
  GdomeNodeList     *nl_threads,*nl_msgcnt;
  GdomeNode         *nl_thread,*nl_message;
  GdomeNode         *thread_el;
  GdomeNamedNodeMap *atts;
  GdomeDOMString    *id_str;
  GdomeDOMString    *msgcnt_str;
  GdomeNode         *id_n;
  GdomeDOMString    *id;
  GdomeNode         *posting;

  str_init(&fname);
	str_init(&tmp);

  str_char_set(&fname,msgpath->values[0],strlen(msgpath->values[0]));
  str_char_set(&tmp,msgpath->values[0],strlen(msgpath->values[0]));
  if(fname->content[fname->len-1] != '/') {
	  str_char_append(&fname,'/');
		str_char_append(&tmp,'/');
  }

  len1 = tmp.len;

  str_chars_append(&fname,"forum.xml",9);

  doc  = gdome_di_createDocFromURI(impl,fname,GDOME_LOAD_PARSING,&exc);

  if(!doc) {
    cf_log(CF_ERR,__FILE__,__LINE__,"coult not load file %s!\n",fname);
    exit(-1);
  }

  el   = gdome_doc_documentElement(doc,&exc);
  nl   = gdome_doc_getElementsByTagName(doc,thread_str,&exc);

  /* get the last mid */
  tmpstr    = gdome_str_mkref("lastMessage");
  tmpstr1   = gdome_el_getAttribute(el,tmpstr,&exc);
  head->mid = str_to_u_int64(tmpstr1->str+1);
  gdome_str_unref(tmpstr1);
  gdome_str_unref(tmpstr);

  tmpstr    = gdome_str_mkref("lastThread");
  tmpstr1   = gdome_el_getAttribute(el,tmpstr,&exc);
  head->tid = str_to_u_int64(tmpstr1->str+1);
  gdome_str_unref(tmpstr);
  gdome_str_unref(tmpstr1);
  gdome_el_unref(el,&exc);

  len = gdome_nl_length(nl,&exc);

  for(i=0;i<len;i++) {
    thread = fo_alloc(NULL,1,sizeof(*thread),FO_ALLOC_CALLOC);

    thread_el  = gdome_nl_item(nl,i,&exc);
    atts       = gdome_n_attributes(thread_el,&exc);
    id_str     = gdome_str_mkref("id");
    msgcnt_str = gdome_str_mkref("MessageContent");
    id_n       = gdome_nnm_getNamedItem(atts,id_str,&exc);
    id         = gdome_n_nodeValue(id_n,&exc);
    posting    = gdome_n_firstChild(thread_el,&exc);

    tmp.len = len1;
		str_chars_append(&tmp,id->str,strlen(id->str));
		str_chars_append(&tmp,".xml",4);

    tmpdoc = gdome_di_createDocFromURI(impl,tmp,GDOME_LOAD_PARSING,&exc);

    nl_threads               = gdome_doc_getElementsByTagName(tmpdoc,thread_str,&exc);
    nl_thread                = gdome_nl_item(nl_threads,0,&exc);
    nl_message               = gdome_n_firstChild(nl_thread,&exc);

    nl_msgcnt                = gdome_doc_getElementsByTagName(tmpdoc,msgcnt_str,&exc);
    
    thread->tid              = str_to_u_int64(id->str+1);
    thread->next             = head->thread;
    thread->postings         = fo_alloc(NULL,1,sizeof(*thread->postings),FO_ALLOC_CALLOC);
    thread->last             = thread->postings;
    if(head->thread) head->thread->prev = thread;
    head->thread             = thread;

    /* registering threads is necessary */
    cf_register_thread(thread);

    /* initialize thread lock */
    snprintf(buff,50,"t%llu",thread->tid);
    cf_rwlock_init(buff,&thread->lock);

    flt_xmlstorage_make_thread_tree(forum,nl_message,posting,nl_msgcnt,thread,thread->postings,0,0);

    gdome_nl_unref(nl_msgcnt,&exc);
    gdome_nl_unref(nl_threads,&exc);
    gdome_n_unref(nl_thread,&exc);
    gdome_n_unref(nl_message,&exc);
    gdome_di_freeDoc(impl,tmpdoc,&exc);

    gdome_str_unref(msgcnt_str);
    gdome_str_unref(id);
    gdome_str_unref(id_str);
    gdome_n_unref(id_n,&exc);
    gdome_n_unref(thread_el,&exc);
    gdome_n_unref(posting,&exc);
    gdome_nnm_unref(atts,&exc);
  }

  gdome_nl_unref(nl,&exc);
  gdome_di_freeDoc(impl,doc,&exc);
  gdome_di_unref(impl,&exc);

  str_cleanup(&tmp);
  str_cleanup(&fname);

  if(head->thread) {
    flt_xmlstorage_sort_threadlist(head);

    for(thread=head->thread;thread->next;thread=thread->next);
    head->last = thread;
  }

	return FLT_OK;
}
/* }}} */

/* {{{ flt_xmlstorage_get_posting_content */
/**
 * This function searches for the MessageContent entry of the node
 * \param contents A node list of the contents ('MessageContent')
 * \param mid The message id of the actual message
 * \return The MessageContent node if found, NULL if not found
 */
GdomeNode *get_posting_content(GdomeNodeList *contents,u_int64_t mid) {
  GdomeException exc;
  GdomeDOMString *str_mid;
  GdomeNamedNodeMap *atts;
  GdomeNode *n;
  GdomeNode *nmid;
  GdomeDOMString *val;
  int len = gdome_nl_length(contents,&exc);
  int i;
  u_int64_t m;

  for(i=0;i<len;i++) {
    n       = gdome_nl_item(contents,i,&exc);
    str_mid = gdome_str_mkref("mid");
    atts    = gdome_n_attributes(n,&exc);
    nmid    = gdome_nnm_getNamedItem(atts,str_mid,&exc);

    if(!nmid) {
      cf_log(LOG_ERR,__FILE__,__LINE__,"Could not get mid in message m%lld\n",mid);
    }
    else {
      val = gdome_n_nodeValue(nmid,&exc);

      if(val) {
        m = str_to_u_int64(val->str+1);

        if(m == mid) {
          gdome_str_unref(val);

          gdome_nnm_unref(atts,&exc);
          gdome_n_unref(nmid,&exc);
          gdome_str_unref(str_mid);

          return n;
        }

        gdome_str_unref(val);
      }

      gdome_n_unref(nmid,&exc);
    }
    

    gdome_nnm_unref(atts,&exc);
    gdome_n_unref(n,&exc);
    gdome_str_unref(str_mid);
  }

  return NULL;
}
/* }}} */

/* {{{ flt_xmlstorage_make_thread_tree
 * Returns:
 * Parameters:
 *   - GdomeNode *posting_newdoc    a node to the posting ('Message')
 *   - GdomeNodeList *contents      a node list of the contents ('MessageContent')
 *   - t_thread *thread             a pointer to the thread object
 *   - t_posting *post              a pointer to the actual thread
 *   - int level                    the level of the posting
 *   - int pos                      the position in the content list
 *
 * this function creates recursively the thread structure
 *
 */
void flt_xmlstorage_make_thread_tree(t_forum *forum,GdomeNode *posting_newdoc, GdomeNode *posting_index, GdomeNodeList *contents, t_thread *thread, t_posting *post, int level,int pos) {
  int i,len,z,one = 1;
  GdomeException     exc;
  GdomeNodeList     *childs_nd = gdome_n_childNodes(posting_newdoc,&exc);
  GdomeNodeList     *childs_in = gdome_n_childNodes(posting_index,&exc);
  GdomeNamedNodeMap *atts      = gdome_n_attributes(posting_newdoc,&exc);
  GdomeNamedNodeMap *atts_ind  = gdome_n_attributes(posting_index,&exc);
  GdomeDOMString    *str_id    = gdome_str_mkref("id");
  GdomeDOMString    *str_ip    = gdome_str_mkref("ip");
  GdomeDOMString    *str_invi  = gdome_str_mkref("invisible");
  GdomeDOMString    *str_gv    = gdome_str_mkref("votingGood");
  GdomeDOMString    *str_bv    = gdome_str_mkref("votingBad");
  GdomeDOMString    *str_unid  = gdome_str_mkref("unid");
  GdomeNode         *invi      = gdome_nnm_getNamedItem(atts,str_invi,&exc);
  GdomeNode         *id        = gdome_nnm_getNamedItem(atts,str_id,&exc);
  GdomeNode         *ip        = gdome_nnm_getNamedItem(atts,str_ip,&exc);
  GdomeNode         *unid      = gdome_nnm_getNamedItem(atts_ind,str_unid,&exc);
  GdomeNode         *good_v    = gdome_nnm_getNamedItem(atts,str_gv,&exc);
  GdomeNode         *bad_v     = gdome_nnm_getNamedItem(atts,str_bv,&exc);
  GdomeNode         *fcnt      = NULL;
  GdomeDOMString    *tmp       = NULL;
  GdomeNode *n;
  GdomeNode *element_nd;
  GdomeNode *element_in;
  GdomeDOMString *name_nd;

  t_posting *p;

  post->level    = level;
  thread->posts += 1;

  /* {{{ get message id */
  if(id) {
    tmp       = gdome_n_nodeValue(id,&exc);
    post->mid = str_to_u_int64(tmp->str+1);
    gdome_str_unref(tmp);
  }
  else {
    cf_log(LOG_ERR,__FILE__,__LINE__,"could not get posting id for thread t%lld, posting %d!\n",thread->tid,pos);
    exit(0);
  }
	/* }}} */

  /* {{{ get votes */
  if(good_v) {
    tmp = gdome_n_nodeValue(good_v,&exc);
    post->votes_good = atoi(tmp->str);
    gdome_str_unref(tmp);
  }

  if(bad_v) {
    tmp = gdome_n_nodeValue(bad_v,&exc);
    post->votes_bad = atoi(tmp->str);
    gdome_str_unref(tmp);
  }
	/* }}} */

  /* {{{ get unique id */
  if(unid) {
    tmp = gdome_n_nodeValue(unid,&exc);
		str_char_set(&tmp->unid,tmp->str,strlen(tmp->str));
    gdome_str_unref(tmp);

    /* register unique id globaly */
    cf_hash_set(forum->uniques.ids,post->unid.content,post->unid.len,&one,sizeof(one));
  }
  else {
    cf_log(LOG_ERR,__FILE__,__LINE__,"could not get unique id for thread t%lld, posting m%lld!\n",thread->tid,post->mid);
  }

  gdome_nnm_unref(atts_ind,&exc);
  gdome_str_unref(str_unid);
  gdome_n_unref(unid,&exc);
	/* }}} */

  /* {{{ get ip */
  if(ip) {
    tmp = gdome_n_nodeValue(ip,&exc);
		str_char_set(&post->user.ip,tmp->str,strlen(tmp->str));
    gdome_str_unref(tmp);
  }
  else {
    cf_log(LOG_ERR,__FILE__,__LINE__,"could not get user ip for thread t%lld, posting m%lld!\n",thread->tid,post->mid);
    exit(0);
  }
	/* }}} */

  /* {{{ get visibility */
  if(invi) {
    tmp = gdome_n_nodeValue(invi,&exc);
    post->invisible = atoi(tmp->str);
    gdome_str_unref(tmp);
  }
  else {
    post->invisible     = 0;
  }
	/* }}} */

  /* {{{ get posting content */
  fcnt = flt_xmlstorage_get_posting_content(contents,post->mid);
  if(fcnt) {
    n = gdome_n_firstChild(fcnt,&exc);
    tmp = gdome_n_nodeValue(n,&exc);
    gdome_n_unref(n,&exc);
  }
  else {
    cf_log(LOG_ERR,__FILE__,__LINE__,"error: could not get content for thread t%lld, message m%lld\n",thread->tid,post->mid);
    exit(0);
  }

  if(tmp) {
	  str_char_set(&post->content,tmp->str,strlen(tmp->str));
    gdome_str_unref(tmp);
  }
  else {
    cf_log(LOG_ERR,__FILE__,__LINE__,"could not get posting content for thread t%lld, message m%lld\n",thread->tid,post->mid);
    exit(0);
  }
	/* }}} */

  len = gdome_nl_length(childs_nd,&exc);

  for(z=0,i=0;i<len;i++) {
    element_nd = gdome_nl_item(childs_nd,i,&exc);
    element_in = gdome_nl_item(childs_in,i,&exc);
    name_nd    = gdome_n_nodeName(element_nd,&exc);

    if(cf_strcmp(name_nd->str,"Header") == 0) {
      flt_xmlstorage_handle_header(post,element_nd,thread->tid);

      if(!thread->newest || post->date > thread->newest->date) thread->newest = post;
      if(!thread->oldest || post->date < thread->oldest->date) thread->oldest = post;
    }
    else if(cf_strcmp(name_nd->str,"Message") == 0) {
      p = fo_alloc(NULL,1,sizeof(*p),FO_ALLOC_CALLOC);
      z++;

      if(thread->last != NULL) thread->last->next = p;

      p->prev          = thread->last;
      thread->last     = p;

      flt_xmlstorage_make_thread_tree(element_nd,element_in,contents,thread,p,level+1,z+pos);
    }

    gdome_str_unref(name_nd);
    gdome_n_unref(element_nd,&exc);
    gdome_n_unref(element_in,&exc);
  }

  if(fcnt) gdome_n_unref(fcnt,&exc);

  gdome_n_unref(invi,&exc);
  gdome_str_unref(str_ip);
  gdome_n_unref(ip,&exc);
  gdome_nnm_unref(atts,&exc);
  gdome_str_unref(str_id);
  gdome_str_unref(str_invi);
  gdome_str_unref(str_gv);
  gdome_str_unref(str_bv);
  gdome_n_unref(id,&exc);
  gdome_nl_unref(childs_nd,&exc);
  gdome_nl_unref(childs_in,&exc);
}
/* }}} */

/* {{{ flt_xmlstorage_handle_header
 * Returns:
 * Parameters:
 *    - t_posting *p   a pointer to the posting
 *    - GdomeNode *n   the 'Header' node
 *
 * this function reads the data from the xml structure
 *
 */
void flt_xmlstorage_handle_header(t_posting *p,GdomeNode *n,u_int64_t tid) {
  GdomeException     exc;
	u_char *tmp;

  GdomeNodeList     *nl       = gdome_n_childNodes(n,&exc);
  GdomeNode         *author   = gdome_nl_item(nl,0,&exc);
  GdomeNode         *category = gdome_nl_item(nl,1,&exc);
  GdomeNode         *subject  = gdome_nl_item(nl,2,&exc);
  GdomeNode         *date     = gdome_nl_item(nl,3,&exc);
  GdomeDOMString    *ls_str   = gdome_str_mkref("longSec");
  GdomeNamedNodeMap *atts     = gdome_n_attributes(date,&exc);
  GdomeNode         *longSec  = gdome_nnm_getNamedItem(atts,ls_str,&exc);
  GdomeDOMString    *ls_val   = gdome_n_nodeValue(longSec,&exc);

  GdomeNodeList     *a_nl     = gdome_n_childNodes(author,&exc);
  GdomeNode         *a_name   = gdome_nl_item(a_nl,0,&exc);
  GdomeNode         *a_email  = gdome_nl_item(a_nl,1,&exc);
  GdomeNode         *a_hp     = gdome_nl_item(a_nl,2,&exc);
  GdomeNode         *a_img    = gdome_nl_item(a_nl,3,&exc);

  p->date          = strtol(ls_val->str,NULL,10);

  tmp = get_node_value(a_name);
	str_char_set(&p->user.name,tmp,strlen(tmp));
	free(tmp);

  tmp = get_node_value(subject);
	str_char_set(&p->subject,tmp,strlen(tmp));
	free(tmp);

  tmp = get_node_value(category);
	if(tmp) {
	  str_char_set(&p->category,tmp,strlen(tmp));
		free(tmp);
	}

	tmp = get_node_value(a_email);
	if(tmp) {
	  str_char_set(&p->user.email,tmp,strlen(tmp));
		free(tmp);
	}

  tmp = get_node_value(a_hp);
	if(tmp) {
	  str_char_set(&p->user.hp,tmp,strlen(tmp));
		free(tmp);
	}

  tmp = get_node_value(a_img);
	if(tmp) {
	  str_char_set(&p->img,tmp,strlen(tmp));
		free(tmp);
	}

  gdome_str_unref(ls_val);
  gdome_n_unref(a_name,&exc);
  gdome_n_unref(a_email,&exc);
  gdome_n_unref(a_hp,&exc);
  gdome_n_unref(a_img,&exc);
  gdome_n_unref(longSec,&exc);
  gdome_nl_unref(a_nl,&exc);
  gdome_str_unref(ls_str);
  gdome_nnm_unref(atts,&exc);
  gdome_n_unref(author,&exc);
  gdome_n_unref(category,&exc);
  gdome_n_unref(subject,&exc);
  gdome_n_unref(date,&exc);
  gdome_nl_unref(nl,&exc);
}
/* }}} */

t_conf_opt flt_xmlstorage_config[] = {
  { NULL, NULL, 0, NULL }
};

t_handler_config flt_xmlstorage_handlers[] = {
  { DATA_LOADING_HANDLER, flt_xmlstorage_make_forumtree },
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
