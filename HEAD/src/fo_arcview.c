/**
 * \file fo_arcview.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 * \brief The forum archive viewer program
 */

/* {{{ Initial comment */
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dlfcn.h>
#include <errno.h>

#include <sys/types.h>
#include <dirent.h>

#include <locale.h>

#include <sys/stat.h>
#include <fcntl.h>

#include <sys/mman.h>

#include <gdome.h>

#include "readline.h"
#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "cfcgi.h"
#include "template.h"
#include "charconvert.h"
#include "clientlib.h"
#include "fo_arcview.h"
/* }}} */

/* {{{ Dummy function, for ignoring unknown directives */
#ifndef DOXYGEN
int ignre(t_configfile *cf,u_char **args,int argnum) {
  return 0;
}
#endif
/* }}} */

/* {{{ get_month_name */
/**
 * function for getting the month name from a month number
 * \param month The number of the month
 * \param name A reference pointer to a u_char pointer
 * \return Returns -1 on failure and the size of the created u_char pointer on success
 * \attention You have to free the created u_char pointer
 */
size_t get_month_name(int month,u_char **name) {
  struct tm tm;
  t_name_value *v = cfg_get_first_value(&fo_default_conf,"DateLocale");
  if(!v) return 0;

  *name = fo_alloc(NULL,BUFSIZ,1,FO_ALLOC_MALLOC);

  memset(&tm,0,sizeof(tm));
  tm.tm_mon = month-1;

  setlocale(LC_TIME,v->values[0]);
  return strftime(*name,BUFSIZ,"%B",&tm);
}
/* }}} */

/* {{{ is_numeric */
/**
 * function checking if a given u_char array only consists of numbers
 * \param ptr The array pointer to check
 * \return 0 if there are characters beside 0-9, 1 if there are only 0-9
 */
int is_numeric(register const u_char *ptr) {
  for(;*ptr;ptr++) {
    if(!isdigit(*ptr)) return 0;
  }

  return 1;
}
/* }}} */

/* {{{ nummeric comparison of array elements */
#ifndef DOXYGEN
int array_numeric_compare(const void *elem1,const void *elem2) {
  int elem1_i = *((int *)elem1);
  int elem2_i = *((int *)elem2);

  if(elem1_i < elem2_i)  return -1;
  if(elem1_i == elem2_i) return  0;
  if(elem1_i > elem2_i)  return  1;

  /* eh? */
  return 0;
}
#endif
/* }}} */

/* {{{ read_dir_content */
/**
 * Function for reading directory content. All directory entries beginning with a dot are ignored.
 * \param dir The directory to read
 * \return A (sorted) array of the directory contents
 */
t_array *read_dir_content(const u_char *dir) {
  DIR *d_dir;
  struct dirent *ent;
  t_array *ary = fo_alloc(NULL,1,sizeof(*ary),FO_ALLOC_MALLOC);
  int num;

  array_init(ary,sizeof(num),NULL);

  if((d_dir = opendir(dir)) == NULL) {
    return NULL;
  }

  while((ent = readdir(d_dir)) != NULL) {
    if(*ent->d_name == '.') continue;
    num = atoi(ent->d_name);

    if(num) array_push(ary,&num);
  }

  closedir(d_dir);

  array_sort(ary,array_numeric_compare);
  return ary;
}
/* }}} */

/* {{{ get_next_token */
/**
 * Helper function for the parsing of the XML files
 * \param ptr The position pointer
 * \param base The pointer to the beginning of the field
 * \param len The length of the field
 * \param token The token to search for
 * \param tlen The length of the token
 * \return Returns NULL if the token could not be found and a pointer to the position of the token if found
 */
u_char *get_next_token(register u_char *ptr,const u_char *base,size_t len,const u_char *token,size_t tlen) {
  for(;ptr < base+len;ptr++) {
    if(*ptr == *token) {
      if(cf_strncmp(ptr,token,tlen) == 0) return ptr;
    }
  }

  return NULL;
}
/* }}} */

/* {{{ get_id */
/**
 * Helper function for reading the id of an element
 * \param ptr The position pointer
 * \param base The pointer to the beginning of the field
 * \param len The length of the field
 */
u_int64_t get_id(u_char *ptr,const u_char *base,size_t len) {
  if(!(ptr = get_next_token(ptr,base,len,(const u_char *)"id=",3))) return 0;
  return strtoull(ptr+5,NULL,10);
}
/* }}} */

/* {{{ generate_thread_output */
/**
 * Recursive function for generating a thread view
 * \param msg A pointer to the message
 * \param threads The string reference pointer for the threads contents
 * \param threadlist The string reference pointer for the thread list in the header of the file
 * \param pt_tpl The "per thread" template
 * \param tl_tpl The "threadlist" template
 * \param ud_tpl The "up down" template
 * \param cs The charset to use for output
 * \param admin Is the viewing user an administrator?
 * \param show_invisible Boolean for showing invisible postings of not (only affects if admin is true)
 */
void generate_thread_output(t_arc_message *msg,t_string *threads,t_string *threadlist,t_cf_template *pt_tpl,t_cf_template *tl_tpl,t_cf_template *ud_tpl,t_name_value *cs,int admin,int show_invisible) {
  size_t i;
  t_arc_message *child;
  u_char buff[256];
  size_t len;
  t_string *str = fo_alloc(NULL,1,sizeof(*str),FO_ALLOC_CALLOC);
  u_char *cnt = NULL;
  size_t cntlen;
  t_mod_api msg_to_html = cf_get_mod_api_ent("msg_to_html");
  void *ptr = fo_alloc(NULL,1,sizeof(const u_char *) + 2 * sizeof(t_string *),FO_ALLOC_MALLOC);
  int printed = 0;
  u_char *date;

  /* first: set threadlist variables */
  len = snprintf(buff,256,"m%llu",msg->mid);
  cf_set_variable(tl_tpl,cs,"mid",buff,len,1);
  cf_set_variable(tl_tpl,cs,"subject",msg->subject.content,msg->subject.len,1);
  cf_set_variable(tl_tpl,cs,"author",msg->author.content,msg->author.len,1);

  if((date = get_time(&fo_arcview_conf,"DateFormatViewList",&len,&msg->date)) != NULL) {
    tpl_cf_setvar(tl_tpl,"date",date,len,1);
    free(date);
  }

  if(msg->category.len) cf_set_variable(tl_tpl,cs,"category",msg->category.content,msg->category.len,1);
  else tpl_cf_freevar(tl_tpl,"category");

  /* parse threadlist and append output to threadlist content */
  tpl_cf_parse_to_mem(tl_tpl);
  str_chars_append(threadlist,tl_tpl->parsed.content,tl_tpl->parsed.len);

  tl_tpl->parsed.len = 0;

  /* after that: set per thread variables */
  cf_set_variable(pt_tpl,cs,"mid",buff,len,1);
  cf_set_variable(pt_tpl,cs,"subject",msg->subject.content,msg->subject.len,1);
  cf_set_variable(pt_tpl,cs,"author",msg->author.content,msg->author.len,1);

  /* category */
  if(msg->category.len) cf_set_variable(pt_tpl,cs,"category",msg->category.content,msg->category.len,1);
  else                  tpl_cf_freevar(pt_tpl,"category");
  /* email */
  if(msg->email.len)    cf_set_variable(pt_tpl,cs,"email",msg->email.content,msg->email.len,1);
  else                  tpl_cf_freevar(pt_tpl,"email");
  /* homepage url */
  if(msg->hp.len)       cf_set_variable(pt_tpl,cs,"link",msg->hp.content,msg->hp.len,1);
  else                  tpl_cf_freevar(pt_tpl,"link");
  /* image url */
  if(msg->img.len)      cf_set_variable(pt_tpl,cs,"image",msg->img.content,msg->img.len,1);
  else                  tpl_cf_freevar(pt_tpl,"image");

  /* convert message to the right charset */
  if(cf_strcmp(cs->values[0],"UTF-8") == 0 || (cnt = charset_convert_entities(msg->content.content,msg->content.len,"UTF-8",cs->values[0],&cntlen)) == NULL) {
    cnt = strdup(msg->content.content);
  }

  /* set message... */
  memcpy(ptr,&cnt,sizeof(const u_char *));
  memcpy(ptr+sizeof(const u_char *),&str,sizeof(str));
  memset(ptr+sizeof(const u_char *)+sizeof(str),0,sizeof(str));

  msg_to_html(ptr);

  free(cnt);

  tpl_cf_setvar(pt_tpl,"message",str->content,str->len,0);
  str_cleanup(str);
  free(str);

  tpl_cf_parse_to_mem(pt_tpl);
  str_chars_append(threads,pt_tpl->parsed.content,pt_tpl->parsed.len);
  pt_tpl->parsed.len = 0;

  if(msg->childs.elements) {
    str_chars_append(threadlist,"<ul>",4);

    for(i=0;i<msg->childs.elements;i++) {
      child = array_element_at(&msg->childs,i);
      if(child->invisible == 1 && (admin == 0 || show_invisible == 0)) continue;

      printed = 1;

      tpl_cf_parse_to_mem(ud_tpl);
      str_chars_append(threads,ud_tpl->parsed.content,ud_tpl->parsed.len);
      ud_tpl->parsed.len = 0;

      str_chars_append(threadlist,"<li>",4);
      generate_thread_output(child,threads,threadlist,pt_tpl,tl_tpl,ud_tpl,cs,admin,show_invisible);
      str_chars_append(threadlist,"</li>",5);
    }

    if(printed) str_chars_append(threadlist,"</ul>",5);
    else {
      threadlist->len -= 4;
      *(threadlist->content + threadlist->len) = '\0';
    }
  }
}
/* }}} */

/* {{{ print_thread_structure */
/**
 * Function for printing a thread structure
 * \param thr The thread
 * \param year The year of the thread
 * \param month The month of the thread
 * \param admin Boolean If the user is an admin or not
 * \param show_invisible If true invisible messages will be printed (only affecting if user is an administrator)
 */
void print_thread_structure(t_arc_thread *thr,const u_char *year,const u_char *month,int admin,int show_invisible) {
  t_name_value *main_tpl_cfg       = cfg_get_first_value(&fo_arcview_conf,"ThreadTemplate");
  t_name_value *threadlist_tpl_cfg = cfg_get_first_value(&fo_arcview_conf,"ThreadListTemplate");
  t_name_value *per_thread_tpl_cfg = cfg_get_first_value(&fo_arcview_conf,"PerThreadTemplate");
  t_name_value *up_down_tpl_cfg    = cfg_get_first_value(&fo_arcview_conf,"UpDownTemplate");

  t_name_value *cs = cfg_get_first_value(&fo_default_conf,"ExternCharset");

  u_char main_tpl_name[256],threadlist_tpl_name[256],per_thread_tpl_name[256],up_down_tpl_name[256];
  t_cf_template main_tpl,threadlist_tpl,per_thread_tpl,up_down_tpl;

  u_char *tmp;
  int len;

  t_string threadlist,threads;

  /* Buarghs. Four templates. This is fucking bad. */
  generate_tpl_name(main_tpl_name,256,main_tpl_cfg);
  generate_tpl_name(threadlist_tpl_name,256,threadlist_tpl_cfg);
  generate_tpl_name(per_thread_tpl_name,256,per_thread_tpl_cfg);
  generate_tpl_name(up_down_tpl_name,256,up_down_tpl_cfg);

  if(tpl_cf_init(&main_tpl,main_tpl_name) != 0
    || tpl_cf_init(&threadlist_tpl,threadlist_tpl_name) != 0
    || tpl_cf_init(&per_thread_tpl,per_thread_tpl_name) != 0
    || tpl_cf_init(&up_down_tpl,up_down_tpl_name) != 0) {
    str_error_message("E_TPL_NOT_FOUND",NULL,15);
    return;
  }

  len = get_month_name(atoi(month),&tmp);

  tpl_cf_setvar(&main_tpl,"month",tmp,len,1);
  tpl_cf_setvar(&main_tpl,"year",year,strlen(year),0);
  cf_set_variable(&main_tpl,cs,"subject",thr->msgs->subject.content,thr->msgs->subject.len,1);
  tpl_cf_setvar(&main_tpl,"charset",cs->values[0],strlen(cs->values[0]),0);

  free(tmp);

  str_init(&threads);
  str_init(&threadlist);

  generate_thread_output(thr->msgs,&threads,&threadlist,&per_thread_tpl,&threadlist_tpl,&up_down_tpl,cs,admin,show_invisible);

  tpl_cf_setvar(&main_tpl,"threads",threads.content,threads.len,0);
  tpl_cf_setvar(&main_tpl,"threadlist",threadlist.content,threadlist.len,0);

  tpl_cf_parse(&main_tpl);

  tpl_cf_finish(&main_tpl);
  tpl_cf_finish(&threadlist_tpl);
  tpl_cf_finish(&per_thread_tpl);
  tpl_cf_finish(&up_down_tpl);
}
/* }}} */

/* {{{ node_compare */
#ifndef DOXYGEN
int node_compare(t_cf_tree_dataset *a,t_cf_tree_dataset *b) {
  if(*((u_int64_t *)a->key) < *((u_int64_t *)b->key)) return -1;
  if(*((u_int64_t *)a->key) > *((u_int64_t *)b->key)) return 1;

  return 0;
}
#endif
/* }}} */

/* {{{ set_nodevalue_to_str */
/**
 * Helper function to set the contents of a node to a t_string string
 * \param n The node
 * \param str Reference pointer to a string object
 */
void set_nodevalue_to_str(GdomeNode *n,t_string *str) {
  GdomeException exc;
  GdomeNode *x   = gdome_n_firstChild(n,&exc);
  GdomeDOMString *y;

  str_init(str);

  if(x) {
    y = gdome_n_nodeValue(x,&exc);

    if(y) {
      str_char_set(str,y->str,strlen(y->str));

      gdome_n_unref(x,&exc);
      gdome_str_unref(y);
      return;
    }
    else {
      y = gdome_n_nodeValue(n,&exc);

      if(y) {
        str_char_set(str,y->str,strlen(y->str));

        gdome_n_unref(x,&exc);
        gdome_str_unref(y);
        return;
      }
    }

    gdome_n_unref(x,&exc);
  }
}
/* }}} */

/* {{{ handle_header */
/**
 * Helper function for getting header informations
 * \param p Reference pointer to the message structure
 * \param n The node
 */
void handle_header(t_arc_message *p,GdomeNode *n) {
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

  set_nodevalue_to_str(a_name,&p->author);
  set_nodevalue_to_str(subject,&p->subject);
  set_nodevalue_to_str(category,&p->category);
  set_nodevalue_to_str(a_email,&p->email);
  set_nodevalue_to_str(a_hp,&p->hp);
  set_nodevalue_to_str(a_img,&p->img);

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

/* {{{ sort_compare */
#ifndef DOXYGEN
int sort_compare(const void *a,const void *b) {
  if(((t_arc_message *)a)->mid > ((t_arc_message *)b)->mid) return 1;
  if(((t_arc_message *)a)->mid < ((t_arc_message *)b)->mid) return -1;

  return 0;
}
#endif
/* }}} */

/* {{{ make_thread_tree */
/**
 * Recursive function to create a hierarchical thread tree
 * \param thread Reference pointer to the thread
 * \param msg Reference pointer to the message
 * \param posting The posting node
 * \param tree A tree of MessageContent elements (for fast access)
 */
void make_thread_tree(t_arc_thread *thread,t_arc_message *msg,GdomeNode *posting,t_cf_tree *tree) {
  GdomeException e;
  GdomeNodeList     *childs    = gdome_n_childNodes(posting,&e);
  GdomeNamedNodeMap *atts      = gdome_n_attributes(posting,&e);
  GdomeDOMString    *str_id    = gdome_str_mkref("id");
  GdomeDOMString    *str_invi  = gdome_str_mkref("invisible");
  GdomeNode         *invi      = gdome_nnm_getNamedItem(atts,str_invi,&e);
  GdomeNode         *id        = gdome_nnm_getNamedItem(atts,str_id,&e);
  GdomeNode         *fcnt      = NULL;
  GdomeDOMString    *tmp       = NULL;
  GdomeNode *element;
  GdomeDOMString *name;
  t_arc_message m;

  t_cf_tree_dataset d,*rs;
  size_t len,i;

  thread->msg_len++;


  if(id) {
    GdomeDOMString *tmp = gdome_n_nodeValue(id,&e);
    msg->mid            = strtoull(tmp->str+1,NULL,10);

    gdome_str_unref(tmp);
  }
  else {
    fprintf(stderr,"thread %llu: no id found\n",thread->tid);
    str_error_message("E_ARCHIVE_ERROR",NULL,15);
    exit(0);
  }

  if(invi) {
    GdomeDOMString *tmp = gdome_n_nodeValue(invi,&e);
    msg->invisible     = atoi(tmp->str);

    gdome_str_unref(tmp);
  }
  else {
    msg->invisible     = 0;
  }

  tmp   = NULL;
  d.key = &msg->mid;
  rs    = (t_cf_tree_dataset *)cf_tree_find(tree,tree->root,&d);

  if(rs) {
    fcnt         = (GdomeNode *)rs->data;
    GdomeNode *n = gdome_n_firstChild(fcnt,&e);
    tmp          = gdome_n_nodeValue(n,&e);
    gdome_n_unref(n,&e);
  }
  else {
    fprintf(stderr,"thread %llu: no posting content found\n",thread->tid);
    str_error_message("E_ARCHIVE_ERROR",NULL,15);
    exit(0);
  }

  if(tmp) {
    str_init(&msg->content);
    str_char_set(&msg->content,tmp->str,gdome_str_length(tmp));
    gdome_str_unref(tmp);
  }
  else {
    fprintf(stderr,"thread %llu: could not get posting content\n",thread->tid);
    str_error_message("E_ARCHIVE_ERROR",NULL,15);
    exit(0);
  }

  for(i=0,len=gdome_nl_length(childs,&e);i<len;i++) {
    element = gdome_nl_item(childs,i,&e);
    name    = gdome_n_nodeName(element,&e);

    if(cf_strcmp(name->str,"Header") == 0) {
      handle_header(msg,element);
    }
    else if(cf_strcmp(name->str,"Message") == 0) {
      memset(&m,0,sizeof(m));

      if(msg->childs.element_size == 0) {
        /* \todo cleanup routine for child postings array */
        array_init(&msg->childs,sizeof(m),NULL);
      }

      make_thread_tree(thread,&m,element,tree);

      array_push(&msg->childs,&m);
    }

    gdome_str_unref(name);
    gdome_n_unref(element,&e);
  }

  gdome_nl_unref(childs,&e);
  gdome_nnm_unref(atts,&e);
  gdome_str_unref(str_id);
  gdome_str_unref(str_invi);
  gdome_n_unref(invi,&e);
  gdome_n_unref(id,&e);

  /* ok, this level of postings has been created. Go and sort (if necessary) */
  if(msg->childs.elements > 1) {
    array_sort(&msg->childs,sort_compare);
  }
}
/* }}} */

/* {{{ create_thread_structure */
/**
 * Function for creating a thread structure. Uses make_thread_tree()
 * \param doc The XML DOM document
 * \param thr A reference pointer to the thread
 */
void create_thread_structure(GdomeDocument *doc,t_arc_thread *thr) {
  size_t i,len;
  t_cf_tree tree;
  GdomeException e;
  GdomeDOMString *msgcnt_str = gdome_str_mkref("MessageContent");
  GdomeNodeList *message_content = gdome_doc_getElementsByTagName(doc,msgcnt_str,&e);
  GdomeNodeList *messages;
  GdomeNode *n,*msg;
  GdomeNamedNodeMap *atts;
  GdomeDOMString *mid_str = gdome_str_mkref("mid"),*mid_v;
  GdomeDOMString *message_str = gdome_str_mkref("Message");
  GdomeNode *mid_n;
  t_cf_tree_dataset d;

  memset(thr,0,sizeof(*thr));

  cf_tree_init(&tree,node_compare,NULL);

  /*
   * first we put every MessageContent node into a tree. This is necessary
   * because the MessageContent nodes are childs of the ContentList node.
   * And the ContentList node is the last child of the Thread node...
   */
   for(i=0,len=gdome_nl_length(message_content,&e);i<len;i++) {

     n      = gdome_nl_item(message_content,i,&e);
     atts   = gdome_n_attributes(n,&e);
     mid_n  = gdome_nnm_getNamedItem(atts,mid_str,&e);
     mid_v  = gdome_n_nodeValue(mid_n,&e);

     d.data = n;
     d.key  = fo_alloc(NULL,1,sizeof(u_int64_t),FO_ALLOC_MALLOC);
     *((u_int64_t *)d.key) = strtoull(mid_v->str+1,NULL,10);

     cf_tree_insert(&tree,NULL,&d);

     gdome_str_unref(mid_v);
     gdome_n_unref(mid_n,&e);
     gdome_nnm_unref(atts,&e);
   }

   gdome_nl_unref(message_content,&e);
   gdome_str_unref(msgcnt_str);
   gdome_str_unref(mid_str);

   /*
    * ok... now we have fast access to the node values. The
    * next thing we have to do is creating an internal structure
    * from the XML. Trivial recursive algorithm -- if there weren't
    * the need of sorting. So we cannot use the standard (flat) data
    * types, we have to use a datatype which is not flat and therefore
    * possible to sort (has to be recursive, too)
    *
    * Ah. Nice idea: we can sort them in make_thread_tree(). A little
    * array_qsort() before every return would probably work very well
    */
    messages = gdome_doc_getElementsByTagName(doc,message_str,&e);
    msg      = gdome_nl_item(messages,0,&e);

    thr->msgs = fo_alloc(NULL,1,sizeof(*thr->msgs),FO_ALLOC_CALLOC);
    make_thread_tree(thr,thr->msgs,msg,&tree);

    gdome_n_unref(msg,&e);
    gdome_nl_unref(messages,&e);
    gdome_str_unref(message_str);

}
/* }}} */

/* {{{ show_thread */
/**
 * Function for showing a thread of the archive
 * \param year The year of the thread
 * \param month The month of the thread
 * \param tid The thread id
 */
void show_thread(const u_char *year,const u_char *month,const u_char *tid) {
  t_name_value *apath = cfg_get_first_value(&fo_default_conf,"ArchivePath");
  struct stat st;
  t_string path;
  t_arc_thread thr;
  t_mod_api is_admin = cf_get_mod_api_ent("is_admin");
  u_char *uname = cf_hash_get(GlobalValues,"UserName",8);
  int admin = uname ? (int)is_admin(uname) : 0;
  int show_invisible = cf_hash_get(GlobalValues,"ShowInvisible",13) != NULL;

  GdomeException e;
  GdomeDocument *doc;
  GdomeDOMImplementation *impl;

  /* no further headers (at the moment) */
  fwrite("\015\012",1,2,stdout);

  memset(&thr,0,sizeof(thr));

  /* generate file name */
  str_init(&path);
  str_chars_append(&path,apath->values[0],strlen(apath->values[0]));
  str_char_append(&path,'/');
  str_chars_append(&path,year,strlen(year));
  str_char_append(&path,'/');
  str_chars_append(&path,month,strlen(month));
  if(*tid == 't') str_char_append(&path,'/');
  else            str_chars_append(&path,"/t",2);
  str_chars_append(&path,tid,strlen(tid));
  str_chars_append(&path,".xml",4);

  if(stat(path.content,&st) == -1) {
    str_error_message("E_ARCHIVE_THREADNOTPRESENT",NULL,26);
    return;
  }

  impl = gdome_di_mkref();
  if((doc = gdome_di_createDocFromURI(impl,path.content,GDOME_LOAD_PARSING,&e)) == NULL) {
    str_error_message("E_ARCHIVE_THREADNOTPRESENT",NULL,26);
    gdome_di_unref(impl,&e);
    return;
  }

  create_thread_structure(doc,&thr);

  gdome_doc_unref(doc,&e);
  gdome_di_unref(impl,&e);

  if(thr.msgs->invisible == 0 || (admin == 1 && show_invisible == 1)) {
    print_thread_structure(&thr,year,month,admin,show_invisible);
  }
  else {
    str_error_message("E_FO_404",NULL,8);
  }
}
/* }}} */

/* {{{ show_month_content */
/**
 * Function for showing the contents of a month
 * \param year The year
 * \param month The month
 */
void show_month_content(const u_char *year,const u_char *month) {
  t_name_value *v      = cfg_get_first_value(&fo_default_conf,"ArchivePath");
  t_name_value *cs     = cfg_get_first_value(&fo_default_conf,"ExternCharset");
  t_name_value *m_tp   = cfg_get_first_value(&fo_arcview_conf,"MonthsTemplate");
  t_name_value *tl_tp  = cfg_get_first_value(&fo_arcview_conf,"ThreadListMonthTemplate");
  t_string path;
  struct stat st;
  u_char *ptr,*file,*tmp1,*tmp2;
  int fd;
  u_int64_t tid;
  time_t date;
  int len;

  t_cf_template m_tpl,tl_tpl;
  u_char mt_name[256],tl_name[256],buff[256];

  /* no additional headers */
  fwrite("\015\012",1,2,stdout);


  /* generate path to index file */
  str_init(&path);

  str_chars_append(&path,v->values[0],strlen(v->values[0]));
  str_char_append(&path,'/');
  str_chars_append(&path,year,strlen(year));
  str_char_append(&path,'/');
  str_chars_append(&path,month,strlen(month));
  str_chars_append(&path,"/index.xml",10);

  if(stat(path.content,&st) == -1) {
    perror("stat");
    str_error_message("E_ARCHIVE_MONTHNOTPRESENT",NULL,25);
    return;
  }


  /* get templates */
  generate_tpl_name(mt_name,256,m_tp);
  generate_tpl_name(tl_name,256,tl_tp);

  if(tpl_cf_init(&m_tpl,mt_name) != 0 || tpl_cf_init(&tl_tpl,tl_name) != 0) {
    str_error_message("E_CONFIG_ERR",NULL,12);
    return;
  }


  /* open file for mmap */
  if((fd = open(path.content,O_RDONLY)) == -1) {
    perror("open");
    str_error_message("E_ARCHIVE_MONTHNOTPRESENT",NULL,25);
    return;
  }

  if((caddr_t)(file = ptr = mmap(0,st.st_size,PROT_READ,MAP_FILE|MAP_SHARED,fd,0)) == (caddr_t)-1) {
    perror("mmap");
    str_error_message("E_ARCHIVE_MONTHNOTPRESENT",NULL,25);
    return;
  }

  for(;ptr < file + st.st_size;ptr++) {
    if(!(ptr = get_next_token(ptr,file,st.st_size,"<Thread",7))) break;
    tid = get_id(ptr,file,st.st_size);
    len = snprintf(buff,256,"%llu",tid);
    cf_set_variable(&tl_tpl,cs,"link",buff,len,1);

    if(!(ptr = get_next_token(ptr,file,st.st_size,"<Message",8))) break;

    /* we need: date, category, tid, subject, author */
    for(++ptr;ptr < file + st.st_size;ptr++) {
      if(*ptr == '<') {
        if(cf_strncmp(ptr,"<Name>",6) == 0) {
          tmp2 = get_next_token(ptr,file,st.st_size,"</Name>",7);
          tmp1 = strndup(ptr+6,tmp2-ptr-6);
          cf_set_variable(&tl_tpl,cs,"author",tmp1,tmp2-ptr-6,0);
          free(tmp1);
        }
        else if(cf_strncmp(ptr,"<Category",9) == 0) {
          if(cf_strncmp(ptr,"<Category/>",11) == 0) {
            tpl_cf_freevar(&tl_tpl,"cat");
          }
          else {
            tmp2 = get_next_token(ptr,file,st.st_size,"</Category>",11);
            tmp1 = strndup(ptr+10,tmp2-ptr-10);
            cf_set_variable(&tl_tpl,cs,"cat",tmp1,tmp2-ptr-10,1);
            free(tmp1);
          }
        }
        else if(cf_strncmp(ptr,"<Subject>",9) == 0) {
          tmp2 = get_next_token(ptr,file,st.st_size,"</Subject>",10);
          tmp1 = strndup(ptr+9,tmp2-ptr-9);
          cf_set_variable(&tl_tpl,cs,"subject",tmp1,tmp2-ptr-9,0);
          free(tmp1);
        }
        else if(cf_strncmp(ptr,"<Date",5) == 0) {
          ptr += 15;
          date = strtol(ptr,NULL,10);
          tmp1 = get_time(&fo_arcview_conf,"DateFormatList",&len,&date);
          cf_set_variable(&tl_tpl,cs,"date",tmp1,len,1);
          free(tmp1);
        }
        /* baba, finished */
        else if(cf_strncmp(ptr,"<Message",8) == 0 || cf_strncmp(ptr,"</Message>",10) == 0) break;
      }
    }

    tpl_cf_parse_to_mem(&tl_tpl);
  }

  munmap(file,st.st_size);

  len = get_month_name(atoi(month),&tmp1);
  tpl_cf_setvar(&m_tpl,"month",tmp1,len,1);
  cf_set_variable(&m_tpl,cs,"year",year,strlen(year),1);

  tpl_cf_setvar(&m_tpl,"charset",cs->values[0],strlen(cs->values[0]),0);
  tpl_cf_setvar(&m_tpl,"threads",tl_tpl.parsed.content,tl_tpl.parsed.len,0);
  tpl_cf_parse(&m_tpl);

  tpl_cf_finish(&tl_tpl);
  tpl_cf_finish(&m_tpl);
}
/* }}} */

/* {{{ show_year_content */
/**
 * Functions for showing the contents of a year
 * \param year The year
 */
void show_year_content(const u_char *year) {
  t_name_value *v   = cfg_get_first_value(&fo_default_conf,"ArchivePath");
  t_name_value *mt  = cfg_get_first_value(&fo_arcview_conf,"MonthsTemplate");
  t_name_value *mlt = cfg_get_first_value(&fo_arcview_conf,"MonthsListTemplate");
  t_name_value *cs  = cfg_get_first_value(&fo_default_conf,"ExternCharset");

  t_cf_template mt_tpl,mlt_tpl;

  t_array *months;
  size_t len,i;
  int month;

  u_char mt_name[256],mlt_name[256],path[256],buff[10],*name;

  struct stat st;

  fwrite("\015\012",1,2,stdout);

  if(!v || !mt || !mlt) {
    str_error_message("E_CONFIG_ERR",NULL,12);
    return;
  }

  snprintf(path,256,"%s/%s",v->values[0],year);
  if(stat(path,&st) == -1) {
    perror("stat");
    str_error_message("E_ARCHIVE_YEARNOTPRESENT",NULL,24);
    return;
  }

  generate_tpl_name(mt_name,256,mt);
  generate_tpl_name(mlt_name,256,mlt);

  if(tpl_cf_init(&mt_tpl,mt_name) != 0 || tpl_cf_init(&mlt_tpl,mlt_name) != 0) {
    str_error_message("E_CONFIG_ERR",NULL,12);
    return;
  }

  if((months = read_dir_content(path)) == NULL) {
    str_error_message("E_ARCHIVE_YEARNOTPRESENT",NULL,24);
    tpl_cf_finish(&mlt_tpl);
    tpl_cf_finish(&mt_tpl);
    return;
  }

  for(i=0;i<months->elements;i++) {
    month = *((int *)array_element_at(months,i));
    len = snprintf(buff,10,"%d",month);

    tpl_cf_setvar(&mlt_tpl,"month",buff,len,0);

    if((len = get_month_name(month,&name)) != 0) {
      tpl_cf_setvar(&mlt_tpl,"name",name,len,1);
      free(name);
    }

    tpl_cf_parse_to_mem(&mlt_tpl);
  }

  tpl_cf_setvar(&mt_tpl,"charset",cs->values[0],strlen(cs->values[0]),0);
  tpl_cf_setvar(&mt_tpl,"months",mlt_tpl.parsed.content,mlt_tpl.parsed.len,0);
  tpl_cf_parse(&mt_tpl);

  tpl_cf_finish(&mlt_tpl);
  tpl_cf_finish(&mt_tpl);

  array_destroy(months);
  free(months);
}
/* }}} */

/* {{{ show_year_list */
/**
 * Function for showing a list of years
 */
void show_year_list(void) {
  t_name_value *ap = cfg_get_first_value(&fo_default_conf,"ArchivePath");
  t_array *ary = read_dir_content(ap->values[0]);

  t_name_value *yt  = cfg_get_first_value(&fo_arcview_conf,"YearsTemplate");
  t_name_value *ylt = cfg_get_first_value(&fo_arcview_conf,"YearListTemplate");

  t_cf_template years,year;
  u_char buff[10],yt_name[256],ylt_name[256];

  unsigned int i,y;

  fwrite("\015\012",1,2,stdout);

  if(!ap || !yt || !ylt) {
    str_error_message("E_CONFIG_ERR",NULL,12);
    return;
  }

  generate_tpl_name(yt_name,256,yt);
  generate_tpl_name(ylt_name,256,ylt);

  if(tpl_cf_init(&years,yt_name) != 0 || tpl_cf_init(&year,ylt_name) != 0) {
    str_error_message("E_CONFIG_ERR",NULL,12);
    return;
  }

  for(i=0;i<ary->elements;i++) {
    y = *((int *)array_element_at(ary,i));
    y = snprintf(buff,10,"%d",y);

    tpl_cf_setvar(&year,"year",buff,y,0);
    tpl_cf_parse_to_mem(&year);
  }

  tpl_cf_setvar(&years,"years",year.parsed.content,year.parsed.len,0);
  tpl_cf_parse(&years);

  tpl_cf_finish(&year);
  tpl_cf_finish(&years);

  array_destroy(ary);
  free(ary);
}
/* }}} */


/* {{{ main */
/**
 * The main function of the forum archive viewer. No command line switches used.
 * \param argc The argument count
 * \param argv The argument vector
 * \param env The environment vector
 * \return EXIT_SUCCESS on success, EXIT_FAILURE on error
 */
int main(int argc,char *argv[],char *env[]) {
  static const u_char *wanted[] = {
    "fo_default", "fo_arcview"
  };

  u_int32_t pieces = 0;
  int ret;
  size_t i;
  u_char  *ucfg;
  t_array *cfgfiles = get_conf_file(wanted,2);
  t_configfile conf,dconf;
  t_name_value *cs = NULL;
  u_char *UserName;
  u_char *fname;
  u_char **path_infos;
  t_cf_hash *head = cf_cgi_new();
  size_t len;

  if(!cfgfiles) {
    return EXIT_FAILURE;
  }

  cf_init();
  init_modules();
  cfg_init();

  ret  = FLT_OK;

  fname = *((u_char **)array_element_at(cfgfiles,0));
  cfg_init_file(&dconf,fname);
  free(fname);

  fname = *((u_char **)array_element_at(cfgfiles,1));
  cfg_init_file(&conf,fname);
  free(fname);

  cfg_register_options(&dconf,default_options);
  cfg_register_options(&conf,fo_arcview_options);

  if(read_config(&dconf,NULL) != 0 || read_config(&conf,NULL) != 0) {
    fprintf(stderr,"config file error!\n");

    cfg_cleanup_file(&conf);
    cfg_cleanup_file(&dconf);

    return EXIT_FAILURE;
  }

  /* first action: authorization modules */
  if(Modules[AUTH_HANDLER].elements) {
    size_t i;
    t_filter_begin exec;
    t_handler_config *handler;

    ret = FLT_DECLINE;

    for(i=0;i<Modules[AUTH_HANDLER].elements && ret == FLT_DECLINE;i++) {
      handler = array_element_at(&Modules[AUTH_HANDLER],i);

      exec = (t_filter_begin)handler->func;
      ret = exec(head,&fo_default_conf,&fo_view_conf);
    }
  }

  if((UserName = cf_hash_get(GlobalValues,"UserName",8)) != NULL) {
    /* get user config */
    ucfg = get_uconf_name(UserName);
    if(ucfg) {
      free(conf.filename);
      conf.filename = ucfg;

      if(read_config(&conf,ignre) != 0) {
        fprintf(stderr,"config file error!\n");

        cfg_cleanup_file(&conf);
        cfg_cleanup_file(&dconf);

        return EXIT_FAILURE;
      }
    }
  }

  /* first state: let the begin-filters run! :-) */
  if(ret != FLT_EXIT && Modules[INIT_HANDLER].elements) {
    t_handler_config *handler;
    t_filter_begin exec;

    for(i=0;i<Modules[INIT_HANDLER].elements && (ret == FLT_OK || ret == FLT_DECLINE);i++) {
      handler = array_element_at(&Modules[INIT_HANDLER],i);
      exec    = (t_filter_begin)handler->func;
      ret     = exec(head,&fo_default_conf,&fo_view_conf);
    }
  }

  cs = cfg_get_first_value(&fo_default_conf,"ExternCharset");

  if(ret != FLT_EXIT) {
    printf("Content-Type: text/html; charset=%s\015\012",cs?cs->values[0]?cs->values[0]:(u_char *)"UTF-8":(u_char *)"UTF-8");

    /* ok, let's check whats going on... parse PATH_INFO */
    pieces = path_info_parsed(&path_infos);

    /* {{{ we do not want bad values */
    /* bad, ugly ass! don't fool me! */
    for(len=0;len<pieces;len++) {
      /* we accept an trailing t for the tid */
      if(len == 3 && *path_infos[len] == 't') {
        if(!is_numeric(path_infos[len]+1)) {
          /* destroy everything */
          for(len=0;len<pieces;len++) {
            free(path_infos[len]);
          }

          free(path_infos);
          pieces = 0;
          break;
        }
      }
      else {
        if(!is_numeric(path_infos[len])) {
          /* destroy everything */
          for(len=0;len<pieces;len++) {
            free(path_infos[len]);
          }

          free(path_infos);
          pieces = 0;
          break;
        }
      }
    }
    /* }}} */

    switch(pieces) {
      /* year given */
      case 1:
        show_year_content(path_infos[0]);
        break;
      /* and month given */
      case 2:
        show_month_content(path_infos[0],path_infos[1]);
        break;
      /* and tid given */
      case 3:
        show_thread(path_infos[0],path_infos[1],path_infos[2]);
        break;

      /* show list of years due to an input error or nothing given */
      default:
        show_year_list();
    }
  }

  for(i=0;i<pieces;i++) {
    free(path_infos[i]);
  }
  free(path_infos);

  array_destroy(cfgfiles);
  free(cfgfiles);

  cfg_cleanup_file(&conf);
  cfg_cleanup_file(&dconf);

  cfg_cleanup(&fo_default_conf);
  cfg_cleanup(&fo_arcview_conf);

  cleanup_modules(Modules);
  cf_fini();

  return EXIT_SUCCESS;
}
/* }}} */

/* eof */
