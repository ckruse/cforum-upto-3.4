/**
 * \file initfinish.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 * \brief Initialization functions
 *
 * This file contains the server initialization functions
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

/* {{{ hirarchy_them */
/**
 * Insert the postings to a hierarchy level. This function works recursively.
 * \param parent The parent t_h_p structure
 * \param pst The posting pointer
 * \return The pointer to the next posting in a hierarchy level smaller or equal to ours.
 */
t_posting *hirarchy_them(t_h_p *parent,t_posting *pst) {
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
      p = hirarchy_them(&parent->childs[parent->len-1],p);
    }
    else {
      return p;
    }
  }

  return NULL;
}
/* }}} */

/* {{{ quicksort_posts */
/**
 * Quicksort-function for the postings
 * \param list The hierarchical list of postings
 * \param low The lower frontier of the field
 * \param high The higher frontier of the field
 */
void quicksort_posts(t_h_p *list,long low,long high) {
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

  if(low<j) quicksort_posts(list,low,j);
  if(i<high) quicksort_posts(list,i,high);
}
/* }}} */

/* {{{ sort_them */
/**
 * Sorting function.
 * \param node The t_h_p structure node
 */
void sort_them(t_h_p *node) {
  long i;

  if(node->len) {
    quicksort_posts(node->childs,0,node->len-1);

    for(i=0;i<node->len;i++) {
      sort_them(&node->childs[i]);
    }
  }

}
/* }}} */

/* {{{ free_structs */
/**
 * This function cleans up a hierarchical structure
 * \param node The t_h_p node
 */
void free_structs(t_h_p *node) {
  long i;

  if(node->len) {
    for(i=0;i<node->len;i++) {
      free_structs(&node->childs[i]);
    }

    free(node->childs);
  }
}
/* }}} */

/* {{{ serialize_them */
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
          p = serialize_them(&node->childs[i],sort);

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
          p = serialize_them(&node->childs[i],sort);

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

/* {{{ sort_postings */
/**
 * This function sorts the postings
 * \param posts The pointer to the first posting in the chain
 * \param sort The sorting direction
 */
void sort_postings(t_posting *posts,int sort) {
  t_h_p *first = fo_alloc(NULL,1,sizeof(t_h_p),FO_ALLOC_CALLOC);

  if(posts->next) {
    first->node = posts;
    hirarchy_them(first,posts);
    /*return;*/
  }
  else {
    free(first);
    return;
  }

  sort_them(first);
  serialize_them(first,sort);

  free_structs(first);
  free(first);
}
/* }}} */

/* {{{ quicksort_threads */
/**
 * This function is a quicksort function for the threads.
 * \param list The thread list
 * \param low The lower frontier of the field
 * \param high The higher frontier of the field
 */
void quicksort_threads(t_thread **list,long low,long high) {
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

  if(low<j) quicksort_threads(list,low,j);
  if(i<high) quicksort_threads(list,i,high);
}
/* }}} */

/* {{{ sort_threadlist */
/**
 * This function does all the sorting work
 * \param head The head variable
 */
void sort_threadlist(t_head *head) {
  t_name_value *sort_thr_v = cfg_get_first_value(&fo_server_conf,"SortThreads");
  t_name_value *sort_msg_v = cfg_get_first_value(&fo_server_conf,"SortMessages");
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

  quicksort_threads(list,0,threadnum-1);

  if(sort_thr == 1) {
    for(i=0;i<threadnum;i++) {
      sort_postings(list[i]->postings,sort_msg);

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
      sort_postings(list[i]->postings,sort_msg);

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


/* {{{ make_forumtree
 * Returns:
 * Parameters:
 *   - t_configuration *cfg   the default configuration
 *
 * This function creates the forum tree
 *
 */
void make_forumtree(t_configuration *cfg,t_head *head) {
  int i = 0,len = 0,len1 = 0;
  u_char                  *fname          = fo_alloc(NULL,MAXLINE,1,FO_ALLOC_MALLOC);
  u_char                  *tmp            = fo_alloc(NULL,MAXLINE,1,FO_ALLOC_MALLOC);
  GdomeException          exc;
  GdomeDocument          *doc;
  GdomeElement           *el;
  GdomeNodeList          *nl;
  t_name_value           *msgpath        = cfg_get_first_value(cfg,"MessagePath");
  GdomeDOMImplementation *impl           = gdome_di_mkref();
  GdomeDOMString *thread_str             = gdome_str_mkref("Thread");
  GdomeDOMString         *tmpstr,*tmpstr1;
  u_char buff[50];

  (void)strcpy(fname,msgpath->values[0]);
  (void)strcpy(tmp,msgpath->values[0]);
  if(fname[strlen(fname)-1] != '/') {
    (void)strcat(fname,"/");
    (void)strcat(tmp,"/");
  }

  (void)strcat(fname,"forum.xml");

  len1 = strlen(tmp);
  doc  = gdome_di_createDocFromURI(impl,fname,GDOME_LOAD_PARSING,&exc);

  if(!doc) {
    cf_log(LOG_ERR,__FILE__,__LINE__,"coult not load file %s!\n",fname);
    exit(-1);
  }

  el   = gdome_doc_documentElement(doc,&exc);
  nl   = gdome_doc_getElementsByTagName(doc,thread_str,&exc);

  /* get the last mid */
  tmpstr    = gdome_str_mkref("lastMessage");
  tmpstr1   = gdome_el_getAttribute(el,tmpstr,&exc);
  head->mid = strtoull(tmpstr1->str+1,NULL,10);
  gdome_str_unref(tmpstr1);
  gdome_str_unref(tmpstr);

  tmpstr    = gdome_str_mkref("lastThread");
  tmpstr1   = gdome_el_getAttribute(el,tmpstr,&exc);
  head->tid = strtoull(tmpstr1->str+1,NULL,10);
  gdome_str_unref(tmpstr);
  gdome_str_unref(tmpstr1);
  gdome_el_unref(el,&exc);

  len = gdome_nl_length(nl,&exc);

  for(i=0;i<len;i++) {
    GdomeDocument     *tmpdoc;
    GdomeNodeList     *nl_threads,*nl_msgcnt;
    GdomeNode         *nl_thread,*nl_message;
    t_thread          *thread     = fo_alloc(NULL,1,sizeof(t_thread),FO_ALLOC_CALLOC);
    GdomeNode         *thread_el  = gdome_nl_item(nl,i,&exc);
    GdomeNamedNodeMap *atts       = gdome_n_attributes(thread_el,&exc);
    GdomeDOMString    *id_str     = gdome_str_mkref("id");
    GdomeDOMString    *msgcnt_str = gdome_str_mkref("MessageContent");
    GdomeNode         *id_n       = gdome_nnm_getNamedItem(atts,id_str,&exc);
    GdomeDOMString    *id         = gdome_n_nodeValue(id_n,&exc);
    GdomeNode         *posting    = gdome_n_firstChild(thread_el,&exc);

    (void)strcpy(&tmp[len1],id->str);
    strcat(tmp,".xml");
    tmpdoc = gdome_di_createDocFromURI(impl,tmp,GDOME_LOAD_PARSING,&exc);

    nl_threads               = gdome_doc_getElementsByTagName(tmpdoc,thread_str,&exc);
    nl_thread                = gdome_nl_item(nl_threads,0,&exc);
    nl_message               = gdome_n_firstChild(nl_thread,&exc);

    nl_msgcnt                = gdome_doc_getElementsByTagName(tmpdoc,msgcnt_str,&exc);
    
    thread->tid              = strtoull(&id->str[1],NULL,10);
    thread->next             = head->thread;
    thread->postings         = fo_alloc(NULL,1,sizeof(t_posting),FO_ALLOC_CALLOC);
    thread->last             = thread->postings;
    if(head->thread) head->thread->prev = thread;
    head->thread             = thread;

    /* registering threads is necessary */
    cf_register_thread(thread);

    // initialize thread lock
    snprintf(buff,50,"t%lld",thread->tid);
    cf_rwlock_init(buff,&thread->lock);

    make_thread_tree(nl_message,posting,nl_msgcnt,thread,thread->postings,0,0);

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

  free(tmp);
  free(fname);

  if(head->thread) {
    t_thread *thread;

    sort_threadlist(head);

    for(thread=head->thread;thread->next;thread=thread->next);
    head->last = thread;
  }
}
/* }}} */

/* {{{ get_posting_content */
/**
 * This function searches for the MessageContent entry of the node
 * \param contents A node list of the contents ('MessageContent')
 * \param mid The message id of the actual message
 * \return The MessageContent node if found, NULL if not found
 */
GdomeNode *get_posting_content(GdomeNodeList *contents,u_int64_t mid) {
  GdomeException exc;
  int len = gdome_nl_length(contents,&exc);
  int i;

  for(i=0;i<len;i++) {
    GdomeNode      *n       = gdome_nl_item(contents,i,&exc);
    GdomeDOMString *str_mid = gdome_str_mkref("mid");
    GdomeNamedNodeMap *atts = gdome_n_attributes(n,&exc);
    GdomeNode         *nmid = gdome_nnm_getNamedItem(atts,str_mid,&exc);

    if(!nmid) {
      cf_log(LOG_ERR,__FILE__,__LINE__,"Could not get mid in message m%lld\n",mid);
    }
    else {
      GdomeDOMString *val = gdome_n_nodeValue(nmid,&exc);

      if(val) {
        u_int64_t m = strtoull(val->str+1,NULL,10);

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

/* {{{ make_thread_tree
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
void make_thread_tree(GdomeNode *posting_newdoc, GdomeNode *posting_index, GdomeNodeList *contents, t_thread *thread, t_posting *post, int level,int pos) {
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

  post->level    = level;
  thread->posts += 1;

  if(id) {
    GdomeDOMString *tmp = gdome_n_nodeValue(id,&exc);
    post->mid           = strtoull(&tmp->str[1],NULL,10);

    gdome_str_unref(tmp);
  }
  else {
    cf_log(LOG_ERR,__FILE__,__LINE__,"could not get posting id for thread t%lld, posting %d!\n",thread->tid,pos);
    exit(0);
  }

  if(good_v) {
    GdomeDOMString *tmp = gdome_n_nodeValue(good_v,&exc);
    post->votes_good = atoi(tmp->str);
    gdome_str_unref(tmp);
  }

  if(bad_v) {
    GdomeDOMString *tmp = gdome_n_nodeValue(bad_v,&exc);
    post->votes_bad = atoi(tmp->str);
    gdome_str_unref(tmp);
  }

  if(unid) {
    GdomeDOMString *tmp = gdome_n_nodeValue(unid,&exc);
    post->unid          = strdup(tmp->str);
    post->unid_len      = strlen(post->unid);
    gdome_str_unref(tmp);

    /* register unique id globaly */
    cf_hash_set(head.unique_ids,post->unid,post->unid_len,&one,sizeof(one));
  }
  else {
    cf_log(LOG_ERR,__FILE__,__LINE__,"could not get unique id for thread t%lld, posting m%lld!\n",thread->tid,post->mid);
  }

  gdome_nnm_unref(atts_ind,&exc);
  gdome_str_unref(str_unid);
  gdome_n_unref(unid,&exc);


  if(ip) {
    GdomeDOMString *tmp = gdome_n_nodeValue(ip,&exc);
    post->user.ip       = strdup(tmp->str);
    post->user.ip_len   = strlen(post->user.ip);
    
    gdome_str_unref(tmp);
  }
  else {
    cf_log(LOG_ERR,__FILE__,__LINE__,"could not get user ip for thread t%lld, posting m%lld!\n",thread->tid,post->mid);
    exit(0);
  }

  if(invi) {
    GdomeDOMString *tmp = gdome_n_nodeValue(invi,&exc);
    post->invisible     = atoi(tmp->str);

    gdome_str_unref(tmp);
  }
  else {
    post->invisible     = 0;
  }

  fcnt = get_posting_content(contents,post->mid);
  if(fcnt) {
    GdomeNode *n = gdome_n_firstChild(fcnt,&exc);
    tmp = gdome_n_nodeValue(n,&exc);
    gdome_n_unref(n,&exc);
  }
  else {
    cf_log(LOG_ERR,__FILE__,__LINE__,"error: could not get content for thread t%lld, message m%lld\n",thread->tid,post->mid);
    exit(0);
  }

  if(tmp) {
    post->content     = strdup(tmp->str);
    post->content_len = strlen(post->content);
    gdome_str_unref(tmp);
  }
  else {
    cf_log(LOG_ERR,__FILE__,__LINE__,"could not get posting content for thread t%lld, message m%lld\n",thread->tid,post->mid);
    exit(0);
  }

  len = gdome_nl_length(childs_nd,&exc);

  for(z=0,i=0;i<len;i++) {
    GdomeNode *element_nd   = gdome_nl_item(childs_nd,i,&exc);
    GdomeNode *element_in   = gdome_nl_item(childs_in,i,&exc);
    GdomeDOMString *name_nd = gdome_n_nodeName(element_nd,&exc);

    if(cf_strcmp(name_nd->str,"Header") == 0) {
      handle_header(post,element_nd,thread->tid);

      if(!thread->newest || post->date > thread->newest->date) thread->newest = post;
      if(!thread->oldest || post->date < thread->oldest->date) thread->oldest = post;
    }
    else if(cf_strcmp(name_nd->str,"Message") == 0) {
      t_posting *p = fo_alloc(NULL,1,sizeof(t_posting),FO_ALLOC_CALLOC);
      z++;

      if(thread->last != NULL) {
        thread->last->next = p;
      }

      p->prev          = thread->last;
      thread->last     = p;

      make_thread_tree(element_nd,element_in,contents,thread,p,level+1,z+pos);
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

/* {{{ handle_header
 * Returns:
 * Parameters:
 *    - t_posting *p   a pointer to the posting
 *    - GdomeNode *n   the 'Header' node
 *
 * this function reads the data from the xml structure
 *
 */
void handle_header(t_posting *p,GdomeNode *n,u_int64_t tid) {
  GdomeException     exc;
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

  p->user.name     = get_node_value(a_name);
  p->user.name_len = strlen(p->user.name);
  p->subject       = get_node_value(subject);
  p->subject_len   = strlen(p->subject);
  p->category      = get_node_value(category);
  if(p->category)   p->category_len   = strlen(p->category);
  p->user.email    = get_node_value(a_email);
  if(p->user.email) p->user.email_len = strlen(p->user.email);
  p->user.hp       = get_node_value(a_hp);
  if(p->user.hp)    p->user.hp_len    = strlen(p->user.hp);
  p->user.img      = get_node_value(a_img);
  if(p->user.img)   p->user.img_len   = strlen(p->user.img);

  if(!p->subject) {
    cf_log(LOG_ERR,__FILE__,__LINE__,"no subject in thread t%lld, message m%lld\n",tid,p->mid);
    exit(0);
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

/* {{{ cleanup_forumtree */
void cleanup_forumtree() {
  t_thread *t,*t1;
  t_posting *p,*p1;

  for(t=head.thread;t;t=t1) {
    for(p=t->postings;p;p=p1) {
      free(p->user.name);
      free(p->subject);
      free(p->unid);
      free(p->user.ip);
      free(p->content);

      if(p->category)   free(p->category);
      if(p->user.email) free(p->user.email);
      if(p->user.hp)    free(p->user.hp);
      if(p->user.img)   free(p->user.img);

      p1 = p->next;
      free(p);
    }

    cf_rwlock_destroy(&t->lock);

    t1 = t->next;
    free(t);
  }

  str_cleanup(&head.cache_invisible);
  str_cleanup(&head.cache_visible);
}
/* }}} */

/* eof */
