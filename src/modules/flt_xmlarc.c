/**
 * \file flt_xmlarc.c
 * \author Christian Kruse, <cjk@wwwtech.de>
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
#include "cfconfig.h"
#include "defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <string.h>
#include <ctype.h>

#include <errno.h>

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
#include "cfgcomp.h"
#include "cfcgi.h"
#include "template.h"
#include "clientlib.h"
#include "charconvert.h"
#include "fo_arcview.h"
#include "xml_handling.h"
/* }}} */

static u_char *flt_xmlarc_apath = NULL;
static u_char *flt_xmlarc_fn = NULL;


/* {{{ flt_xmlarc_snvts */
void flt_xmlarc_snvts(GdomeNode *n,cf_string_t *str) {
  GdomeException exc;
  GdomeNode *x   = gdome_n_firstChild(n,&exc);
  GdomeDOMString *y;

  cf_str_init(str);

  if(x) {
    y = gdome_n_nodeValue(x,&exc);

    if(y) {
      cf_str_char_set(str,y->str,strlen(y->str));

      gdome_n_unref(x,&exc);
      gdome_str_unref(y);
      return;
    }
    else {
      y = gdome_n_nodeValue(n,&exc);

      if(y) {
        cf_str_char_set(str,y->str,strlen(y->str));

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
void flt_xmlarc_hh(message_t *p,GdomeNode *n) {
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
int flt_xmlarc_mtt(cl_thread_t *thr,hierarchical_node_t *hmsg,GdomeNode *posting) {
  GdomeException e;
  GdomeNodeList     *childs    = gdome_n_childNodes(posting,&e);
  GdomeNamedNodeMap *atts      = gdome_n_attributes(posting,&e);
  GdomeDOMString    *cf_str_id    = gdome_str_mkref("id");
  GdomeDOMString    *cf_str_invi  = gdome_str_mkref("invisible");
  GdomeNode         *invi      = gdome_nnm_getNamedItem(atts,cf_str_invi,&e);
  GdomeNode         *id        = gdome_nnm_getNamedItem(atts,cf_str_id,&e);
  GdomeNode *element;
  GdomeDOMString *name;
  GdomeDOMString *tmp;
  hierarchical_node_t h;

  u_char *ctmp;
  size_t len,i;

  thr->msg_len++;

  if(id) {
    tmp = gdome_n_nodeValue(id,&e);
    hmsg->msg->mid  = cf_str_to_uint64(tmp->str+1);
    gdome_str_unref(tmp);
  }
  else return -1;

  if(invi) {
    tmp  = gdome_n_nodeValue(invi,&e);
    hmsg->msg->invisible = atoi(tmp->str);
    gdome_str_unref(tmp);
  }
  else hmsg->msg->invisible  = 0;

  for(i=0,len=gdome_nl_length(childs,&e);i<len;i++) {
    element = gdome_nl_item(childs,i,&e);
    name    = gdome_n_nodeName(element,&e);

    if(cf_strcmp(name->str,"Header") == 0) {
      flt_xmlarc_hh(hmsg->msg,element);
    }
    else if(cf_strcmp(name->str,"MessageContent") == 0) {
      cf_str_init(&hmsg->msg->content);

      if((ctmp = xml_get_node_value(element)) != NULL) {
        hmsg->msg->content.content = ctmp;
        hmsg->msg->content.reserved = hmsg->msg->content.len = strlen(ctmp);
      }
    }
    else if(cf_strcmp(name->str,"Message") == 0) {
      h.msg = cf_alloc(NULL,1,sizeof(*h.msg),CF_ALLOC_CALLOC);
      cf_array_init(&h.childs,sizeof(h),NULL);

      if(flt_xmlarc_mtt(thr,&h,element) != 0) return -1;

      cf_array_push(&hmsg->childs,&h);
    }

    gdome_str_unref(name);
    gdome_n_unref(element,&e);
  }

  gdome_nl_unref(childs,&e);
  gdome_nnm_unref(atts,&e);
  gdome_str_unref(cf_str_id);
  gdome_str_unref(cf_str_invi);
  gdome_n_unref(invi,&e);
  gdome_n_unref(id,&e);

  return 0;
}
/* }}} */

/* {{{ flt_xmlarc_cts */
int flt_xmlarc_cts(GdomeDocument *doc,cl_thread_t *thr) {
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

  thr->messages = cf_alloc(NULL,1,sizeof(*thr->messages),CF_ALLOC_CALLOC);

  thr->ht   = cf_alloc(NULL,1,sizeof(*thr->ht),CF_ALLOC_CALLOC);
  thr->ht->msg = thr->messages;

  cf_array_init(&thr->ht->childs,sizeof(*thr->ht),NULL);

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
  return cf_str_to_uint64(ptr+5);
}
/* }}} */

/* {{{ flt_xmlarc_get_ndc */
cf_array_t *flt_xmlarc_get_ndc(const u_char *dir) {
  DIR *d_dir;
  struct dirent *ent;
  cf_array_t *ary = cf_alloc(NULL,1,sizeof(*ary),CF_ALLOC_MALLOC);
  int num;

  cf_array_init(ary,sizeof(num),NULL);

  if((d_dir = opendir(dir)) == NULL) return NULL;

  while((ent = readdir(d_dir)) != NULL) {
    if(*ent->d_name == '.') continue;
    num = atoi(ent->d_name);

    if(num) cf_array_push(ary,&num);
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
  cf_string_t str;
  struct stat st;

  cf_str_init(&str);
  cf_str_char_set(&str,flt_xmlarc_apath,strlen(flt_xmlarc_apath));
  cf_str_char_append(&str,'/');
  cf_str_chars_append(&str,year,strlen(year));
  cf_str_char_append(&str,'/');
  cf_str_chars_append(&str,month,strlen(month));
  cf_str_char_append(&str,'/');
  if(*tid != 't') cf_str_char_append(&str,'t');
  cf_str_chars_append(&str,tid,strlen(tid));
  cf_str_chars_append(&str,".xml",4);

  if(stat(str.content,&st) == -1) {
    cf_str_cleanup(&str);
    return 0;
  }

  cf_str_cleanup(&str);
  return st.st_mtime;
}
/* }}} */

/* {{{ flt_xmlarc_mlm */
time_t flt_xmlarc_mlm(const u_char *year,const u_char *month) {
  cf_string_t str;

  struct stat st;

  cf_str_init(&str);
  cf_str_char_set(&str,flt_xmlarc_apath,strlen(flt_xmlarc_apath));
  cf_str_char_append(&str,'/');
  cf_str_chars_append(&str,year,strlen(year));
  cf_str_char_append(&str,'/');
  cf_str_chars_append(&str,month,strlen(month));
  cf_str_chars_append(&str,"/index.xml",10);

  if(stat(str.content,&st) == -1) {
    cf_str_cleanup(&str);
    return 0;
  }

  cf_str_cleanup(&str);
  return st.st_mtime;
}
/* }}} */

/* {{{ flt_xmlarc_validate_thread */
int flt_xmlarc_validate_thread(const u_char *year,const u_char *month,const u_char *tid) {
  cf_string_t str;
  struct stat st;

  cf_name_value_t *v;

  cf_str_init(&str);
  cf_str_char_set(&str,flt_xmlarc_apath,strlen(flt_xmlarc_apath));
  cf_str_char_append(&str,'/');
  cf_str_chars_append(&str,year,strlen(year));
  cf_str_char_append(&str,'/');
  cf_str_chars_append(&str,month,strlen(month));
  cf_str_char_append(&str,'/');
  if(*tid != 't') cf_str_char_append(&str,'t');
  cf_str_chars_append(&str,tid,strlen(tid));
  cf_str_chars_append(&str,".xml",4);

  if(stat(str.content,&st) != -1) {
    if(S_ISREG(st.st_mode)) {
      cf_str_cleanup(&str);

      if(*tid != 't') {
        v = cf_cfg_get_first_value(&fo_default_conf,flt_xmlarc_fn,"DF:ArchiveURL");

        cf_str_char_set(&str,v->values[0],strlen(v->values[0]));
        cf_str_chars_append(&str,year,strlen(year));
        cf_str_char_append(&str,'/');
        cf_str_chars_append(&str,month,strlen(month));
        cf_str_chars_append(&str,"/t",2);
        cf_str_chars_append(&str,tid,strlen(tid));
        cf_str_char_append(&str,'/');

        cf_http_redirect_with_nice_uri(str.content,1);
        cf_str_cleanup(&str);

        return FLT_EXIT;
      }

      return FLT_OK;
    }
  }

  cf_str_cleanup(&str);
  v = cf_cfg_get_first_value(&fo_default_conf,flt_xmlarc_fn,"DF:ExternCharset");

  printf("Status: 404 Not Found\015\012Content-Type: text/html; charset=%s\015\012\015\012",v->values[0]);
  cf_error_message("E_ARCHIVE_THREADNOTPRESENT",NULL);

  return FLT_EXIT;
}
/* }}} */

/* {{{ flt_xmlarc_validate_month */
int flt_xmlarc_validate_month(const u_char *year,const u_char *month) {
  cf_string_t str;
  struct stat st;

  cf_name_value_t *v;

  cf_str_init(&str);
  cf_str_char_set(&str,flt_xmlarc_apath,strlen(flt_xmlarc_apath));
  cf_str_char_append(&str,'/');
  cf_str_chars_append(&str,year,strlen(year));
  cf_str_char_append(&str,'/');
  cf_str_chars_append(&str,month,strlen(month));

  if(stat(str.content,&st) != -1) {
    if(S_ISDIR(st.st_mode)) {
      cf_str_cleanup(&str);
      return FLT_OK;
    }
  }

  cf_str_cleanup(&str);
  v = cf_cfg_get_first_value(&fo_default_conf,flt_xmlarc_fn,"DF:ExternCharset");

  printf("Status: 404 Not Found\015\012Content-Type: text/html; charset=%s\015\012\015\012",v->values[0]);
  cf_error_message("E_ARCHIVE_MONTHNOTPRESENT",NULL);

  return FLT_EXIT;
}
/* }}} */

/* {{{ flt_xmlarc_validate_year */
int flt_xmlarc_validate_year(const u_char *year) {
  cf_string_t str;
  struct stat st;

  cf_name_value_t *v;

  cf_str_init(&str);
  cf_str_char_set(&str,flt_xmlarc_apath,strlen(flt_xmlarc_apath));
  cf_str_char_append(&str,'/');
  cf_str_chars_append(&str,year,strlen(year));

  if(stat(str.content,&st) != -1) {
    if(S_ISDIR(st.st_mode)) {
      cf_str_cleanup(&str);
      return FLT_OK;
    }
  }

  cf_str_cleanup(&str);
  v = cf_cfg_get_first_value(&fo_default_conf,flt_xmlarc_fn,"DF:ExternCharset");

  printf("Status: 404 Not Found\015\012Content-Type: text/html; charset=%s\015\012\015\012",v->values[0]);
  cf_error_message("E_ARCHIVE_YEARNOTPRESENT",NULL);

  return FLT_EXIT;
}
/* }}} */

cl_thread_t *flt_xmlarc_getthread(const u_char *year,const u_char *month,const u_char *tid) {
  cf_string_t str;
  cl_thread_t *thr = cf_alloc(NULL,1,sizeof(*thr),CF_ALLOC_CALLOC);

  GdomeException e;
  GdomeDocument *doc;
  GdomeDOMImplementation *impl;

  /* {{{ generate thread file name */
  cf_str_init(&str);
  cf_str_char_set(&str,flt_xmlarc_apath,strlen(flt_xmlarc_apath));
  cf_str_char_append(&str,'/');
  cf_str_chars_append(&str,year,strlen(year));
  cf_str_char_append(&str,'/');
  cf_str_chars_append(&str,month,strlen(month));
  cf_str_char_append(&str,'/');
  if(*tid != 't') cf_str_char_append(&str,'t');
  cf_str_chars_append(&str,tid,strlen(tid));
  cf_str_chars_append(&str,".xml",4);
  /* }}} */

  impl = gdome_di_mkref();
  if((doc = gdome_di_createDocFromURI(impl,str.content,GDOME_LOAD_PARSING,&e)) == NULL) return NULL;

  thr->tid = cf_str_to_uint64(*tid == 't' ? tid+1 : tid);

  if(flt_xmlarc_cts(doc,thr) != 0) return NULL;

  gdome_doc_unref(doc,&e);
  gdome_di_unref(impl,&e);

  return thr;
}

/* {{{ flt_xmlarc_getthreadlist */
cf_array_t *flt_xmlarc_getthreadlist(const u_char *year,const u_char *month) {
  cf_string_t str,strbuff;
  cf_array_t *ary = cf_alloc(NULL,1,sizeof(*ary),CF_ALLOC_MALLOC);
  arc_tl_ent_t ent;

  int fd;
  u_int64_t tid;

  register u_char *ptr;
  u_char *file,*tmp2,*val;

  struct stat st;

  /* {{{ get path of xml file */
  cf_str_init(&str);
  cf_str_char_set(&str,flt_xmlarc_apath,strlen(flt_xmlarc_apath));
  cf_str_char_append(&str,'/');
  cf_str_chars_append(&str,year,strlen(year));
  cf_str_char_append(&str,'/');
  cf_str_chars_append(&str,month,strlen(month));
  cf_str_chars_append(&str,"/index.xml",10);
  /* }}} */

  cf_array_init(ary,sizeof(ent),NULL);

  /* {{{ map file into memory */
  if(stat(str.content,&st) == -1) {
    fprintf(stderr,"flt_xmlarc: stat: could not stat '%s': %s\n",str.content,strerror(errno));
    return NULL;
  }

  if((fd = open(str.content,O_RDONLY)) == -1) {
    fprintf(stderr,"flt_xmlarc: open: could not open '%s': %s\n",str.content,strerror(errno));
    return NULL;
  }

  if((caddr_t)(file = ptr = mmap(0,st.st_size,PROT_READ,MAP_FILE|MAP_SHARED,fd,0)) == (caddr_t)-1) {
    fprintf(stderr,"flt_xmlarc: mmap: could not map '%s': %s\n",str.content,strerror(errno));
    return NULL;
  }
  /* }}} */

  for(;ptr < file + st.st_size;ptr++) {
    memset(&ent,0,sizeof(ent));

    if((ptr = flt_xmlarc_gnt(ptr,file,st.st_size,"<Thread",7)) == NULL) break;

    tid          = flt_xmlarc_gi(ptr,file,st.st_size);

    cf_str_init(&strbuff);
    cf_str_char_append(&strbuff,'t');
    cf_uint64_to_str(&strbuff,tid);

    ent.tid = strbuff.content;
    ent.tlen = strbuff.len;

    if(!(ptr = flt_xmlarc_gnt(ptr,file,st.st_size,"<Message",8))) break;

    /* we need: date, category, tid, subject, author */
    for(++ptr;ptr < file + st.st_size;++ptr) {
      if(*ptr == '<') {
        if(cf_strncmp(ptr,"<Name>",6) == 0) {
          tmp2 = flt_xmlarc_gnt(ptr,file,st.st_size,"</Name>",7);
          val = strndup(ptr+6,tmp2-ptr-6);
          ent.author = htmlentities_decode(val,&ent.alen);
          free(val);
        }
        else if(cf_strncmp(ptr,"<Category",9) == 0) {
          if(cf_strncmp(ptr,"<Category/>",11) != 0 && cf_strncmp(ptr,"<Category />",11) != 0) {
            tmp2 = flt_xmlarc_gnt(ptr,file,st.st_size,"</Category>",11);
            val = strndup(ptr+10,tmp2-ptr-10);
            ent.cat = htmlentities_decode(val,&ent.clen);
            free(val);
          }
        }
        else if(cf_strncmp(ptr,"<Subject>",9) == 0) {
          tmp2 = flt_xmlarc_gnt(ptr,file,st.st_size,"</Subject>",10);
          val = strndup(ptr+9,tmp2-ptr-9);
          ent.subject = htmlentities_decode(val,&ent.slen);
          free(val);
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
          ent.invisible = *(ptr+11) - '0';
        }
      }
    }

    cf_array_push(ary,&ent);
  }

  munmap(file,st.st_size);
  close(fd);

  return ary;
}
/* }}} */

/* {{{ flt_xmlarc_getmonthlist */
cf_array_t *flt_xmlarc_getmonthlist(const u_char *year) {
  cf_string_t str;
  cf_array_t *ret;

  cf_str_init(&str);
  cf_str_char_set(&str,flt_xmlarc_apath,strlen(flt_xmlarc_apath));
  cf_str_char_append(&str,'/');
  cf_str_chars_append(&str,year,strlen(year));

  ret = flt_xmlarc_get_ndc(str.content);
  cf_str_cleanup(&str);

  return ret;
}
/* }}} */

/* {{{ flt_xmlarc_getyears */
cf_array_t *flt_xmlarc_getyears(void) {
  return flt_xmlarc_get_ndc(flt_xmlarc_apath);
}
/* }}} */

int flt_xmlarc_init_handler(cf_hash_t *cgi,cf_configuration_t *dc,cf_configuration_t *vc) {
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
int flt_xmlarc_handle(cf_configfile_t *cfile,cf_conf_opt_t *opt,const u_char *context,u_char **args,size_t argnum) {
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

cf_conf_opt_t flt_xmlarc_config[] = {
  { "ArchivePath", flt_xmlarc_handle, CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, NULL },
  { NULL, NULL, 0, NULL }
};

cf_handler_config_t flt_xmlarc_handlers[] = {
  { INIT_HANDLER, flt_xmlarc_init_handler },
  { 0, NULL }
};

cf_module_config_t flt_xmlarc = {
  MODULE_MAGIC_COOKIE,
  flt_xmlarc_config,
  flt_xmlarc_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  flt_xmlarc_cleanup
};

/* eof */
