/**
 * \file flt_xmlarc.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 *
 * This plugin handles the XML archive format
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

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <string.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <gdome.h>

#include "readline.h"
#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "cfcgi.h"
#include "template.h"
#include "clientlib.h"
#include "fo_arcview.h"
#include "xml_handling.h"
/* }}} */

static u_char *flt_xmlarc_apath = NULL;
static u_char *flt_xmlarc_fn = NULL;


/* {{{ flt_xmlarc_snvts */
void flt_xmlarc_snvts(GdomeNode *n,t_string *str) {
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

/* {{{ flt_xmlarc_hh */
void flt_xmlarc_hh(t_message *p,GdomeNode *n) {
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

  flt_xmlarc_snvts(a_name,&p->author);
  flt_xmlarc_snvts(subject,&p->subject);
  flt_xmlarc_snvts(category,&p->category);
  flt_xmlarc_snvts(a_email,&p->email);
  flt_xmlarc_snvts(a_hp,&p->hp);
  flt_xmlarc_snvts(a_img,&p->img);

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

/* {{{ flt_xmlarc_mtt */
int flt_xmlarc_mtt(t_cl_thread *thr,t_hierarchical_node *hmsg,GdomeNode *posting) {
  GdomeException e;
  GdomeNodeList     *childs    = gdome_n_childNodes(posting,&e);
  GdomeNamedNodeMap *atts      = gdome_n_attributes(posting,&e);
  GdomeDOMString    *str_id    = gdome_str_mkref("id");
  GdomeDOMString    *str_invi  = gdome_str_mkref("invisible");
  GdomeNode         *invi      = gdome_nnm_getNamedItem(atts,str_invi,&e);
  GdomeNode         *id        = gdome_nnm_getNamedItem(atts,str_id,&e);
  GdomeNode *element;
  GdomeDOMString *name;
  t_hierarchical_node h;

  u_char *ctmp;
  size_t len,i;

  thr->msg_len++;

  if(id) {
    GdomeDOMString *tmp = gdome_n_nodeValue(id,&e);
    hmsg->msg->mid  = str_to_u_int64(tmp->str+1);
    gdome_str_unref(tmp);
  }
  else return -1;

  if(invi) {
    GdomeDOMString *tmp  = gdome_n_nodeValue(invi,&e);
    hmsg->msg->invisible = atoi(tmp->str);
    gdome_str_unref(tmp);
  }
  else h.msg->invisible  = 0;

  for(i=0,len=gdome_nl_length(childs,&e);i<len;i++) {
    element = gdome_nl_item(childs,i,&e);
    name    = gdome_n_nodeName(element,&e);

    if(cf_strcmp(name->str,"Header") == 0) {
      flt_xmlarc_hh(hmsg->msg,element);
    }
    else if(cf_strcmp(name->str,"MessageContent") == 0) {
      str_init(&hmsg->msg->content);

      if((ctmp = xml_get_node_value(element)) != NULL) {
        hmsg->msg->content.content = ctmp;
        hmsg->msg->content.reserved = hmsg->msg->content.len = strlen(ctmp);
      }
    }
    else if(cf_strcmp(name->str,"Message") == 0) {
      h.msg = fo_alloc(NULL,1,sizeof(*h.msg),FO_ALLOC_CALLOC);
      array_init(&h.childs,sizeof(h),NULL);

      if(flt_xmlarc_mtt(thr,&h,element) != 0) return -1;

      array_push(&hmsg->childs,&h);
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

  return 0;
}
/* }}} */

/* {{{ flt_xmlarc_cts */
int flt_xmlarc_cts(GdomeDocument *doc,t_cl_thread *thr) {
  GdomeException e;
  GdomeNodeList *messages;
  GdomeNode *msg;
  GdomeDOMString *message_str = gdome_str_mkref("Message");

  /*
   * We have to create an internal structure from the XML. Trivial
   * recursive algorithm.
   */
  messages = gdome_doc_getElementsByTagName(doc,message_str,&e);
  msg      = gdome_nl_item(messages,0,&e);

  thr->messages = fo_alloc(NULL,1,sizeof(*thr->messages),FO_ALLOC_CALLOC);

  thr->ht   = fo_alloc(NULL,1,sizeof(*thr->ht),FO_ALLOC_CALLOC);
  thr->ht->msg = thr->messages;

  array_init(&thr->ht->childs,sizeof(*thr->ht),NULL);

  if(flt_xmlarc_mtt(thr,thr->ht,msg) != 0) return -1;

  gdome_n_unref(msg,&e);
  gdome_nl_unref(messages,&e);
  gdome_str_unref(message_str);

  return 0;
}
/* }}} */

/* {{{ flt_xmlarc_gnt */
u_char *flt_xmlarc_gnt(register u_char *ptr,const u_char *base,size_t len,const u_char *token,size_t tlen) {
  for(;ptr < base+len;++ptr) {
    if(*ptr == *token) {
      if(cf_strncmp(ptr,token,tlen) == 0) return ptr;
    }
  }

  return NULL;
}
/* }}} */

/* {{{ flt_xmlarc_gi */
u_int64_t flt_xmlarc_gi(u_char *ptr,const u_char *base,size_t len) {
  if(!(ptr = flt_xmlarc_gnt(ptr,base,len,(const u_char *)"id=",3))) return 0;
  return str_to_u_int64(ptr+5);
}
/* }}} */

/* {{{ flt_xmlarc_get_ndc */
t_array *flt_xmlarc_get_ndc(const u_char *dir) {
  DIR *d_dir;
  struct dirent *ent;
  t_array *ary = fo_alloc(NULL,1,sizeof(*ary),FO_ALLOC_MALLOC);
  int num;

  array_init(ary,sizeof(num),NULL);

  if((d_dir = opendir(dir)) == NULL) return NULL;

  while((ent = readdir(d_dir)) != NULL) {
    if(*ent->d_name == '.') continue;
    num = atoi(ent->d_name);

    if(num) array_push(ary,&num);
  }

  closedir(d_dir);

  return ary;
}
/* }}} */

/* {{{ flt_xmlarc_is_numeric */
int flt_xmlarc_is_numeric(register const u_char *ptr) {
  for(;*ptr;ptr++) {
    if(!isdigit(*ptr)) return 0;
  }

  return 1;
}
/* }}} */


/* {{{ flt_xmlarc_tlm */
time_t flt_xmlarc_tlm(const u_char *year,const u_char *month,const u_char *tid) {
  t_string str;
  struct stat st;

  str_init(&str);
  str_char_set(&str,flt_xmlarc_apath,strlen(flt_xmlarc_apath));
  str_char_append(&str,'/');
  str_chars_append(&str,year,strlen(year));
  str_char_append(&str,'/');
  str_chars_append(&str,month,strlen(month));
  str_char_append(&str,'/');
  if(*tid != 't') str_char_append(&str,'t');
  str_chars_append(&str,tid,strlen(tid));
  str_chars_append(&str,".xml",4);

  if(stat(str.content,&st) == -1) {
    str_cleanup(&str);
    return 0;
  }

  str_cleanup(&str);
  return st.st_mtime;
}
/* }}} */

/* {{{ flt_xmlarc_mlm */
time_t flt_xmlarc_mlm(const u_char *year,const u_char *month) {
  t_string str;

  struct stat st;

  str_init(&str);
  str_char_set(&str,flt_xmlarc_apath,strlen(flt_xmlarc_apath));
  str_char_append(&str,'/');
  str_chars_append(&str,year,strlen(year));
  str_char_append(&str,'/');
  str_chars_append(&str,month,strlen(month));
  str_chars_append(&str,"/index.xml",10);

  if(stat(str.content,&st) == -1) {
    str_cleanup(&str);
    return 0;
  }

  str_cleanup(&str);
  return st.st_mtime;
}
/* }}} */

/* {{{ flt_xmlarc_validate_thread */
int flt_xmlarc_validate_thread(const u_char *year,const u_char *month,const u_char *tid) {
  t_string str;
  struct stat st;

  t_name_value *v;

  str_init(&str);
  str_char_set(&str,flt_xmlarc_apath,strlen(flt_xmlarc_apath));
  str_char_append(&str,'/');
  str_chars_append(&str,year,strlen(year));
  str_char_append(&str,'/');
  str_chars_append(&str,month,strlen(month));
  str_char_append(&str,'/');
  if(*tid != 't') str_char_append(&str,'t');
  str_chars_append(&str,tid,strlen(tid));
  str_chars_append(&str,".xml",4);

  if(stat(str.content,&st) != -1) {
    if(S_ISREG(st.st_mode)) {
      str_cleanup(&str);

      if(*tid != 't') {
        v = cfg_get_first_value(&fo_default_conf,flt_xmlarc_fn,"ArchiveURL");

        str_char_set(&str,v->values[0],strlen(v->values[0]));
        str_chars_append(&str,year,strlen(year));
        str_char_append(&str,'/');
        str_chars_append(&str,month,strlen(month));
        str_chars_append(&str,"/t",2);
        str_chars_append(&str,tid,strlen(tid));
        str_char_append(&str,'/');

        printf("Status: 301 Moved Permanently\015\012Location: %s\015\012\015\012",str.content);
        str_cleanup(&str);

        return FLT_EXIT;
      }

      return FLT_OK;
    }
  }

  str_cleanup(&str);
  v = cfg_get_first_value(&fo_default_conf,flt_xmlarc_fn,"ExternCharset");

  printf("Status: 404 Not Found\015\012Content-Type: text/html; charset=%s\015\012\015\012",v->values[0]);
  cf_error_message("E_ARCHIVE_MONTHNOTPRESENT",NULL);

  return FLT_EXIT;
}
/* }}} */

/* {{{ flt_xmlarc_validate_month */
int flt_xmlarc_validate_month(const u_char *year,const u_char *month) {
  t_string str;
  struct stat st;

  t_name_value *v;

  str_init(&str);
  str_char_set(&str,flt_xmlarc_apath,strlen(flt_xmlarc_apath));
  str_char_append(&str,'/');
  str_chars_append(&str,year,strlen(year));
  str_char_append(&str,'/');
  str_chars_append(&str,month,strlen(month));

  if(stat(str.content,&st) != -1) {
    if(S_ISDIR(st.st_mode)) {
      str_cleanup(&str);
      return FLT_OK;
    }
  }

  str_cleanup(&str);
  v = cfg_get_first_value(&fo_default_conf,flt_xmlarc_fn,"ExternCharset");

  printf("Status: 404 Not Found\015\012Content-Type: text/html; charset=%s\015\012\015\012",v->values[0]);
  cf_error_message("E_ARCHIVE_MONTHNOTPRESENT",NULL);

  return FLT_EXIT;
}
/* }}} */

/* {{{ flt_xmlarc_validate_year */
int flt_xmlarc_validate_year(const u_char *year) {
  t_string str;
  struct stat st;

  t_name_value *v;

  str_init(&str);
  str_char_set(&str,flt_xmlarc_apath,strlen(flt_xmlarc_apath));
  str_char_append(&str,'/');
  str_chars_append(&str,year,strlen(year));

  if(stat(str.content,&st) != -1) {
    if(S_ISDIR(st.st_mode)) {
      str_cleanup(&str);
      return FLT_OK;
    }
  }

  str_cleanup(&str);
  v = cfg_get_first_value(&fo_default_conf,flt_xmlarc_fn,"ExternCharset");

  printf("Status: 404 Not Found\015\012Content-Type: text/html; charset=%s\015\012\015\012",v->values[0]);
  cf_error_message("E_ARCHIVE_YEARNOTPRESENT",NULL);

  return FLT_EXIT;
}
/* }}} */

t_cl_thread *flt_xmlarc_getthread(const u_char *year,const u_char *month,const u_char *tid) {
  t_string str;
  t_cl_thread *thr = fo_alloc(NULL,1,sizeof(*thr),FO_ALLOC_CALLOC);

  GdomeException e;
  GdomeDocument *doc;
  GdomeDOMImplementation *impl;

  /* {{{ generate thread file name */
  str_init(&str);
  str_char_set(&str,flt_xmlarc_apath,strlen(flt_xmlarc_apath));
  str_char_append(&str,'/');
  str_chars_append(&str,year,strlen(year));
  str_char_append(&str,'/');
  str_chars_append(&str,month,strlen(month));
  str_char_append(&str,'/');
  if(*tid != 't') str_char_append(&str,'t');
  str_chars_append(&str,tid,strlen(tid));
  str_chars_append(&str,".xml",4);
  /* }}} */

  impl = gdome_di_mkref();
  if((doc = gdome_di_createDocFromURI(impl,str.content,GDOME_LOAD_PARSING,&e)) == NULL) return NULL;

  thr->tid = str_to_u_int64(*tid == 't' ? tid+1 : tid);

  if(flt_xmlarc_cts(doc,thr) != 0) return NULL;

  gdome_doc_unref(doc,&e);
  gdome_di_unref(impl,&e);

  return thr;
}

/* {{{ flt_xmlarc_getthreadlist */
t_array *flt_xmlarc_getthreadlist(const u_char *year,const u_char *month) {
  t_string str,strbuff;
  t_array *ary = fo_alloc(NULL,1,sizeof(*ary),FO_ALLOC_MALLOC);
  t_arc_tl_ent ent;

  int fd,is_invisible;
  int show_invisible = cf_hash_get(GlobalValues,"ShowInvisible",13) != NULL;
  u_int64_t tid;

  register u_char *ptr;
  u_char *file,*tmp2;

  struct stat st;

  /* {{{ get path of xml file */
  str_init(&str);
  str_char_set(&str,flt_xmlarc_apath,strlen(flt_xmlarc_apath));
  str_char_append(&str,'/');
  str_chars_append(&str,year,strlen(year));
  str_char_append(&str,'/');
  str_chars_append(&str,month,strlen(month));
  str_chars_append(&str,"/index.xml",10);
  /* }}} */

  array_init(ary,sizeof(ent),NULL);

  /* {{{ map file into memory */
  if(stat(str.content,&st) == -1) {
    perror("stat");
    return NULL;
  }

  if((fd = open(str.content,O_RDONLY)) == -1) {
    perror("open");
    return NULL;
  }

  if((caddr_t)(file = ptr = mmap(0,st.st_size,PROT_READ,MAP_FILE|MAP_SHARED,fd,0)) == (caddr_t)-1) {
    perror("mmap");
    return NULL;
  }
  /* }}} */

  for(;ptr < file + st.st_size;ptr++) {
    memset(&ent,0,sizeof(ent));

    if((ptr = flt_xmlarc_gnt(ptr,file,st.st_size,"<Thread",7)) == NULL) break;

    tid          = flt_xmlarc_gi(ptr,file,st.st_size);
    is_invisible = 0;

    str_init(&strbuff);
    str_char_append(&strbuff,'t');
    u_int64_to_str(&strbuff,tid);

    ent.tid = strbuff.content;
    ent.tlen = strbuff.len;

    if(!(ptr = flt_xmlarc_gnt(ptr,file,st.st_size,"<Message",8))) break;

    /* we need: date, category, tid, subject, author */
    for(++ptr;ptr < file + st.st_size;++ptr) {
      if(*ptr == '<') {
        if(cf_strncmp(ptr,"<Name>",6) == 0) {
          tmp2 = flt_xmlarc_gnt(ptr,file,st.st_size,"</Name>",7);
          ent.author = strndup(ptr+6,tmp2-ptr-6);
          ent.alen = tmp2-ptr-6;
        }
        else if(cf_strncmp(ptr,"<Category",9) == 0) {
          if(cf_strncmp(ptr,"<Category/>",11) != 0 && cf_strncmp(ptr,"<Category />",11) != 0) {
            tmp2 = flt_xmlarc_gnt(ptr,file,st.st_size,"</Category>",11);
            ent.cat = strndup(ptr+10,tmp2-ptr-10);
            ent.clen = tmp2-ptr-10;
          }
        }
        else if(cf_strncmp(ptr,"<Subject>",9) == 0) {
          tmp2 = flt_xmlarc_gnt(ptr,file,st.st_size,"</Subject>",10);
          ent.subject = strndup(ptr+9,tmp2-ptr-9);
          ent.slen = tmp2-ptr-9;
        }
        else if(cf_strncmp(ptr,"<Date",5) == 0) {
          ptr += 15;
          ent.date = strtol(ptr,NULL,10);
        }
        /* baba, finished */
        else if(cf_strncmp(ptr,"</Header>",9) == 0) break;
      }
      else if(*ptr == 'i') {
        if(cf_strncmp(ptr,"invisible",9) == 0) {
          is_invisible = *(ptr+11) - '0';
        }
      }
    }

    if(is_invisible == 0 || show_invisible == 1) array_push(ary,&ent);
    else {
      if(ent.author) free(ent.author);
      if(ent.cat) free(ent.cat);
      if(ent.subject) free(ent.subject);
      if(ent.tid) free(ent.tid);
    }
  }

  munmap(file,st.st_size);
  close(fd);

  return ary;
}
/* }}} */

/* {{{ flt_xmlarc_getmonthlist */
t_array *flt_xmlarc_getmonthlist(const u_char *year) {
  t_string str;
  t_array *ret;

  str_init(&str);
  str_char_set(&str,flt_xmlarc_apath,strlen(flt_xmlarc_apath));
  str_char_append(&str,'/');
  str_chars_append(&str,year,strlen(year));

  ret = flt_xmlarc_get_ndc(str.content);
  str_cleanup(&str);

  return ret;
}
/* }}} */

/* {{{ flt_xmlarc_getyears */
t_array *flt_xmlarc_getyears(void) {
  return flt_xmlarc_get_ndc(flt_xmlarc_apath);
}
/* }}} */

int flt_xmlarc_init_handler(t_cf_hash *cgi,t_configuration *dc,t_configuration *vc) {
  cf_hash_set_static(ArcviewHandlers,"av_validate_year",16,flt_xmlarc_validate_year);
  cf_hash_set_static(ArcviewHandlers,"av_validate_month",17,flt_xmlarc_validate_month);
  cf_hash_set_static(ArcviewHandlers,"av_validate_thread",18,flt_xmlarc_validate_thread);

  cf_hash_set_static(ArcviewHandlers,"av_get_years",12,flt_xmlarc_getyears);
  cf_hash_set_static(ArcviewHandlers,"av_get_monthlist",16,flt_xmlarc_getmonthlist);
  cf_hash_set_static(ArcviewHandlers,"av_get_threadlist",17,flt_xmlarc_getthreadlist);
  cf_hash_set_static(ArcviewHandlers,"av_get_thread",13,flt_xmlarc_getthread);

  cf_hash_set_static(ArcviewHandlers,"av_threadlist_lm",16,flt_xmlarc_mlm);
  cf_hash_set_static(ArcviewHandlers,"av_thread_lm",12,flt_xmlarc_tlm);

  return FLT_OK;
}

/* {{{ flt_xmlarc_handle */
int flt_xmlarc_handle(t_configfile *cfile,t_conf_opt *opt,const u_char *context,u_char **args,size_t argnum) {
  if(flt_xmlarc_fn == NULL) flt_xmlarc_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(flt_xmlarc_fn,context) != 0) return 0;

  if(flt_xmlarc_apath) free(flt_xmlarc_apath);
  flt_xmlarc_apath = strdup(args[0]);

  return 0;
}
/* }}} */

void flt_xmlarc_cleanup(void) {
  if(flt_xmlarc_apath) free(flt_xmlarc_apath);
}

t_conf_opt flt_xmlarc_config[] = {
  { "ArchivePath", flt_xmlarc_handle, CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_LOCAL, NULL },
  { NULL, NULL, 0, NULL }
};

t_handler_config flt_xmlarc_handlers[] = {
  { INIT_HANDLER, flt_xmlarc_init_handler },
  { 0, NULL }
};

t_module_config flt_xmlarc = {
  flt_xmlarc_config,
  flt_xmlarc_handlers,
  NULL,
  NULL,
  NULL,
  flt_xmlarc_cleanup
};

/* eof */
