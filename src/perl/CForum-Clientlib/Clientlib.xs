#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "ppport.h"

#include "const-c.inc"

#include "config.h"
#include "defines.h"

#include "utils.h"
#include "charconvert.h"
#include "hashlib.h"
#include "template.h"
#include "readline.h"
#include "configparser.h"
#include "clientlib.h"

typedef struct s_cfxs_conn {
  int sock;
  rline_t tsd;
} t_cfxs_conn;

#define P_ID    0
#define P_STR   1
#define P_INT   2
#define P_FLOAT 3
typedef struct s_cfxs_param {
  int type;
  size_t size;
  void *val;
} t_cfxs_param;

bool xs_has_shm(void) {
  #ifdef CF_SHARED_MEM
  return TRUE;
  #else
  return FALSE;
  #endif
}

void *xs_shm_ptr(void) {
  void *ptr;

  #ifndef CF_SHARED_MEM
  return NULL;
  #else
  if((ptr = get_shm_ptr()) == NULL) {
    return NULL;
  }

  return ptr;
  #endif
}

MODULE = CForum::Clientlib		PACKAGE = CForum::Clientlib		

INCLUDE: const-xs.inc
PROTOTYPES: DISABLE

SV *
new(class)
    u_char *class
  PREINIT:
    const u_char *CLASS = "CForum::Clientlib";
    SV *var;
  CODE:
    var = newSV(0);
    cf_init();
    RETVAL=sv_setref_iv(var,CLASS,0);
  OUTPUT:
    RETVAL

const u_char *
get_uconf_name(class,name)
    SV *class
    const u_char *name
  PREINIT:
    u_char *ret;
  CODE:
    if((ret = get_uconf_name(name)) == NULL) {
      XSRETURN_UNDEF;
    }

    RETVAL=ret;
  OUTPUT:
    RETVAL

const u_char *
get_global_value(class,name)
    SV *class
    u_char *name
  PREINIT:
    u_char *ret;
  CODE:
    if((ret = cf_hash_get(GlobalValues,name,strlen(name))) == NULL) {
      XSRETURN_UNDEF;
    }

    RETVAL=ret;
  OUTPUT:
    RETVAL

void
_set_global_value(class,name,value,len)
    SV *class
    u_char *name
    u_char *value
    I32 len
  CODE:
    cf_hash_set(GlobalValues,name,strlen(name),value,len);

t_cfxs_conn *
connect(class)
    SV *class
  PREINIT:
    t_cfxs_conn *conn;
    const u_char *CLASS = "CForum::Clientlib::Connection";
  CODE:
    conn = fo_alloc(NULL,1,sizeof(*conn),FO_ALLOC_CALLOC);

    if((conn->sock = set_us_up_the_socket()) < 0) {
      free(conn);
      XSRETURN_UNDEF;
    }

    RETVAL=conn;
  OUTPUT:
    RETVAL


void destroy(class)
    SV *class
  ALIAS:
    CForum::Clientlib::DESTROY = 1
  CODE:

bool has_shm(class)
    SV *class
  CODE:
    if(xs_has_shm()) XSRETURN_YES;
    else             XSRETURN_NO;

SV *
get_shm_segment(class)
    SV *class
  PREINIT:
    void *ptr;
    HV *var,*stash;
    SV *val,**hvg;
    const u_char *CLASS = "CForum::Clientlib::SharedMem";
  CODE:
    if((ptr = xs_shm_ptr()) == NULL) {
      XSRETURN_UNDEF;
    }

    var = newHV();
    val = newSV(0);
    sv_setref_pv(val,"CForum::Clientlib::Pointer",ptr);
    hvg = hv_store(var,"ptr",3,val,0);

    stash = gv_stashpv(CLASS,1);
    val   = newRV_inc((SV *)var);
    sv_bless(val,stash);

    RETVAL=val;
  OUTPUT:
    RETVAL

u_char *
charset_convert(class,encode,ilen,from,to)
    SV *class
    const u_char *encode
    size_t ilen
    const u_char *from
    const u_char *to
  PREINIT:
    size_t olen;
    u_char *ret;
  CODE:
    if((ret = charset_convert(encode,ilen,from,to,NULL)) == NULL) {
      XSRETURN_UNDEF;
    }

    RETVAL=ret;
  OUTPUT:
    RETVAL

u_char *
htmlentities(class,encode,sq)
    SV *class
    const u_char *encode
    int sq
  CODE:
    RETVAL=htmlentities(encode,sq);
  OUTPUT:
    RETVAL

u_char *
htmlentities_charset_convert(class,encode,from,to,sq)
    SV *class
    const u_char *encode
    const u_char *from
    const u_char *to
    int sq
  PREINIT:
    u_char *ret;
  CODE:
    if((ret = htmlentities_charset_convert(encode,from,to,NULL,1)) == NULL) {
      XSRETURN_UNDEF;
    }

    RETVAL=ret;
  OUTPUT:
    RETVAL

u_char *charset_convert_entities(class,toencode,len,from,to)
    SV *class
    const u_char *toencode
    size_t len
    const u_char *from
    const u_char *to
  PREINIT:
    u_char *ret;
  CODE:
    if((ret = charset_convert_entities(toencode,len,from,to,NULL)) == NULL) {
      XSRETURN_UNDEF;
    }

    RETVAL=ret;
  OUTPUT:
    RETVAL

bool
is_valid_utf8_string(class,str,len)
    SV *class
    const u_char *str
    size_t len
  CODE:
    if(is_valid_utf8_string(str,len) == 0) {
      XSRETURN_YES;
    }

    XSRETURN_NO;


MODULE = CForum::Clientlib      PACKAGE = CForum::Clientlib::SharedMem

t_cl_thread *
get_message(class,ctid,cmid,tplname,del)
    SV *class
    const u_char *ctid
    const u_char *cmid
    const u_char *tplname
    bool del
    const u_char *CLASS = "CForum::Clientlib::Thread";
  PREINIT:
    void *ptr;
    t_cl_thread *thr;
    u_int64_t tid;
    u_int64_t mid;
    HV *hash;
    SV **val;
  CODE:
    hash = (HV *)SvRV(class);
    val  = hv_fetch(hash,"ptr",3,0);

    if(!val)  XSRETURN_UNDEF;

    ptr  = (void *)SvIV((SV *)SvRV(*val));
    thr  = fo_alloc(NULL,1,sizeof(*thr),FO_ALLOC_CALLOC);

    tid = strtoull(ctid,NULL,10);
    mid = strtoull(cmid,NULL,10);

    if(cf_get_message_through_shm(ptr+sizeof(time_t),thr,tplname,tid,mid,del) == -1) {
      XSRETURN_UNDEF;
    }

    RETVAL=thr;
  OUTPUT:
    RETVAL


t_cl_thread *
get_next_thread(class,tplname=NULL)
    SV *class
    const u_char *tplname
  PREINIT:
    t_cl_thread *thr;
    const u_char *CLASS = "CForum::Clientlib::Thread";
    void *ptr,*ptr1;
    SV **hvg,*val,**val1;
    HV *hash;
  CODE:
    hash = (HV *)SvRV(class);
    val1 = hv_fetch(hash,"ptr",3,0);
    hvg  = hv_fetch(hash,"nextptr",7,0);

    /* no ptr entry in class hash? could not be... */
    if(!val1) XSRETURN_UNDEF;

    if(hvg) ptr = (void *)SvIV((SV *)SvRV(*hvg));
    else    ptr = (void *)SvIV((SV *)SvRV(*val1)) + sizeof(time_t);

    thr = fo_alloc(NULL,1,sizeof(*thr),FO_ALLOC_CALLOC);

    if((ptr1 = cf_get_next_thread_through_shm(ptr,thr,tplname)) == NULL) {
      XSRETURN_UNDEF;
    }

    val = newSV(0);
    sv_setref_pv(val,"CForum::Clientlib::SharedMem::Pointer",ptr1);
    hvg = hv_store(hash,"nextptr",7,val,0);

    RETVAL=thr;
  OUTPUT:
    RETVAL


MODULE = CForum::Clientlib      PACKAGE = CForum::Clientlib::Connection

I32
writen(class,value,len)
    t_cfxs_conn *class
    const u_char *value
    size_t len
  CODE:
    RETVAL=writen(class->sock,value,len);
  OUTPUT:
    RETVAL

u_char *
readline(class)
    t_cfxs_conn *class
  PREINIT:
    u_char *line;
  CODE:
    if((line = readline(class->sock,&class->tsd)) == NULL) {
      XSRETURN_UNDEF;
    }

    RETVAL=line;
  OUTPUT:
    RETVAL

void destroy(class)
    t_cfxs_conn *class
  ALIAS:
    CForum::Clientlib::Connection::DESTROY = 1
  CODE:
    free(class);

t_cl_thread *
get_next_thread(class,tplname=NULL)
    t_cfxs_conn *class
    const u_char *tplname
  PREINIT:
    t_cl_thread *thr;
    const u_char *CLASS = "CForum::Clientlib::Thread";
  CODE:
    thr = fo_alloc(NULL,1,sizeof(*thr),FO_ALLOC_CALLOC);

    if(cf_get_next_thread_through_sock(class->sock,&class->tsd,thr,tplname) == -1) {
      XSRETURN_UNDEF;
    }

    RETVAL=thr;
  OUTPUT:
    RETVAL

t_cl_thread *
get_message(class,ctid,cmid,tplname,del)
    t_cfxs_conn *class
    const u_char *ctid
    const u_char *cmid
    const u_char *tplname
    bool del
    const u_char *CLASS = "CForum::Clientlib::Thread";
  PREINIT:
    t_cl_thread *thr;
    u_int64_t tid;
    u_int64_t mid;
  CODE:
    thr = fo_alloc(NULL,1,sizeof(*thr),FO_ALLOC_CALLOC);

    tid = strtoull(ctid,NULL,10);
    mid = strtoull(cmid,NULL,10);

    if(cf_get_message_through_sock(class->sock,&class->tsd,thr,tplname,(u_int64_t)tid,(u_int64_t)mid,del) == -1) {
      XSRETURN_UNDEF;
    }

    RETVAL=thr;
  OUTPUT:
    RETVAL


MODULE = CForum::Clientlib          PACKAGE = CForum::Clientlib::Thread

t_cl_thread *
new(class)
    u_char *class
  PREINIT:
    t_cl_thread *thr;
    const u_char *CLASS = "CForum::Clientlib::Thread";
  CODE:
    thr = fo_alloc(NULL,1,sizeof(*thr),FO_ALLOC_CALLOC);
    RETVAL=thr;
  OUTPUT:
    RETVAL

u_char *
tid(class,ctid=NULL)
    t_cl_thread *class
    const u_char *ctid
  PREINIT:
    u_char *tid;
  CODE:
    tid = fo_alloc(NULL,21,1,FO_ALLOC_MALLOC);
    snprintf(tid,21,"%lld",class->tid);

    if(ctid) {
      class->tid = strtoull(ctid,NULL,10);
    }

    RETVAL=tid;
  OUTPUT:
    RETVAL

I32
postnum(class)
    t_cl_thread *class
  CODE:
    RETVAL=class->msg_len;
  OUTPUT:
    RETVAL

t_message *
postings(class,postings=NULL)
    t_cl_thread *class
    t_message *postings
  PREINIT:
    const u_char *CLASS = "CForum::Clientlib::Thread::Posting";
  CODE:
    if(class->messages == NULL) {
      class->messages = postings;
      XSRETURN_UNDEF;
    }

    RETVAL=class->messages;

    if(postings) {
      class->messages = postings;
    }
  OUTPUT:
    RETVAL

void
append_posting(class,posting)
    t_cl_thread *class
    t_message *posting
  CODE:
    class->last->next = posting;
    posting->prev = class->last;
    class->last = posting;


MODULE = CForum::Clientlib      PACKAGE = CForum::Clientlib::Thread::Posting

t_message *
new(class)
    const u_char *class
  PREINIT:
    t_message *msg;
    const u_char *CLASS = "CForum::Clientlib::Thread::Posting";
  CODE:
    msg = fo_alloc(NULL,1,sizeof(*msg),FO_ALLOC_CALLOC);
    RETVAL=msg;
  OUTPUT:
    RETVAL

u_char *
mid(class,cmid=NULL)
    t_message *class
    const u_char *cmid
  PREINIT:
    u_char *buff;
  CODE:
    buff = fo_alloc(NULL,21,1,FO_ALLOC_MALLOC);
    snprintf(buff,21,"%lld",class->mid);

    if(cmid) {
      class->mid = strtoull(cmid,NULL,10);
    }

    RETVAL=buff;
  OUTPUT:
    RETVAL

const u_char *
author(class,author=NULL)
    t_message *class
    const u_char *author
  PREINIT:
    u_char *ret;
  CODE:
    ret = class->author;

    if(author) {
      class->author = strdup(author);
      class->author_len = strlen(author);
    }

    if(ret == NULL) {
      XSRETURN_UNDEF;
    }
    RETVAL=ret;
  OUTPUT:
    RETVAL


const u_char *
subject(class,subj=NULL)
    t_message *class
    const u_char *subj
  PREINIT:
    u_char *ret;
  CODE:
    ret = class->subject;

    if(subj) {
      class->subject = strdup(subj);
      class->subject_len = strlen(subj);
    }

    if(ret == NULL) {
      XSRETURN_UNDEF;
    }
    RETVAL=ret;
  OUTPUT:
    RETVAL


const u_char *
category(class,cat=NULL)
    t_message *class;
    const u_char *cat;
  PREINIT:
    u_char *ret;
  CODE:
    ret = class->category;

    if(cat) {
      class->category = strdup(cat);
      class->category_len = strlen(cat);
    }

    if(ret == NULL) XSRETURN_UNDEF;
    RETVAL=ret;
  OUTPUT:
    RETVAL

const u_char *
content(class,cnt=NULL,len=0)
    t_message *class
    const u_char *cnt
    size_t len
  PREINIT:
    u_char *ret;
  CODE:
    ret = class->content;

    if(cnt) {
      class->content = strdup(cnt);
      if(len) class->content_len = len;
      else    class->content_len = strlen(cnt);
    }

    if(ret == NULL) {
      XSRETURN_UNDEF;
    }
    RETVAL=ret;
  OUTPUT:
    RETVAL

const u_char *
email(class,email=NULL)
    t_message *class
    const u_char *email
  PREINIT:
    u_char *ret;
  CODE:
    ret = class->email;

    if(email) {
      class->email     = strdup(email);
      class->email_len = strlen(email);
    }

    if(ret == NULL) {
      XSRETURN_UNDEF;
    }
    RETVAL=ret;
  OUTPUT:
    RETVAL

const u_char *
hp(class,hp=NULL)
    t_message *class
    const u_char *hp
  PREINIT:
    u_char *ret;
  CODE:
    ret = class->hp;

    if(hp) {
      class->hp     = strdup(hp);
      class->hp_len = strlen(hp);
    }

    if(ret == NULL) {
      XSRETURN_UNDEF;
    }
    RETVAL=ret;
  OUTPUT:
    RETVAL

const u_char *
img(class,img=NULL)
    t_message *class
    const u_char *img
  PREINIT:
    u_char *ret;
  CODE:
    ret = class->img;

    if(img) {
      class->img     = strdup(img);
      class->img_len = strlen(img);
    }

    if(ret == NULL) {
      XSRETURN_UNDEF;
    }
    RETVAL=ret;
  OUTPUT:
    RETVAL

short
level(class,level=-1)
    t_message *class
    short level
  CODE:
    RETVAL=class->level;

    if(level != -1) {
      class->level = level;
    }

  OUTPUT:
    RETVAL

I32
date(class,date=-1)
    t_message *class
    I32 date
  CODE:
    RETVAL=class->date;

    if(date != -1) {
      class->date = date;
    }

  OUTPUT:
    RETVAL

short
may_show(class,ms=-1)
    t_message *class
    short ms
  CODE:
    RETVAL=class->may_show;

    if(ms != -1) {
      class->may_show = ms;
    }

  OUTPUT:
    RETVAL

bool invisible(class)
    t_message *class
  CODE:
    RETVAL=class->invisible != 0;
  OUTPUT:
    RETVAL

t_cf_template *
template(class)
    t_message *class
  PREINIT:
    const u_char *CLASS = "CForum::Template";
  CODE:
    RETVAL=&class->tpl;
  OUTPUT:
    RETVAL

t_message *
next(class)
    t_message *class
  PREINIT:
    const u_char *CLASS = "CForum::Clientlib::Thread::Posting";
  CODE:
    if(class->next == NULL) XSRETURN_UNDEF;
    RETVAL=class->next;
  OUTPUT:
    RETVAL

MODULE = CForum::Clientlib      PACKAGE = CForum::Clientlib::API

MODULE = CForum::Clientlib      PACKAGE = CForum::Clientlib::API::Hook
t_mod_api_ent *
new(class,unid)
    const u_char *class
    const u_char *unid
  PREINIT:
    const u_char *CLASS = "CForum::Clientlib::API::Hook";
    t_mod_api_ent *ret;
    size_t len;
  CODE:
    len = strlen(unid);

    if((ret = cf_hash_get(APIEntries,(u_char *)unid,len)) == NULL) {
      XSRETURN_UNDEF;
    }

    RETVAL=ret;
  OUTPUT:
    RETVAL

SV *
call(class,arg=NULL)
    t_mod_api_ent *class
    t_cfxs_param *arg
  PREINIT:
    void *ret;
    SV *sret;
    const u_char *CLASS = "CForum::Clientlib::API::Hook:RetVal";
  CODE:
    if(arg) {
      if((ret = class->function(arg->val)) == NULL) {
        XSRETURN_UNDEF;
      }
    }
    else {
      if((ret = class->function(NULL)) == NULL) {
        XSRETURN_UNDEF;
      }
    }

    sret = newSV(0);
    sv_setref_pv(sret,(char *)CLASS,ret);
    RETVAL=sret;
  OUTPUT:
    RETVAL

void
destroy(void)
  ALIAS:
    CForum::Clientlib::API::Hook::DESTROY = 1
  CODE:
    ;

MODULE = CForum::Clientlib      PACKAGE = CForum::Clientlib::API::Hook::Parameter
t_cfxs_param *
new(class)
    const u_char *class
  PREINIT:
    const u_char *CLASS = "CForum::Clientlib::API::Hook::Parameter";
    t_cfxs_param *p;
  CODE:
    p = fo_alloc(NULL,1,sizeof(*p),FO_ALLOC_CALLOC);
    RETVAL=p;
  OUTPUT:
    RETVAL

void
setVal(class,val,type)
    t_cfxs_param *class
    SV *val
    int type
  PREINIT:
    u_char *cptr;
    u_int64_t idv;
    int iv;
    double fv;
  CODE:
    switch(type) {
      case P_ID:
        cptr = SvPV_nolen(val);
        idv = strtoull(cptr,NULL,10);
        class->val  = memdup(&idv,sizeof(idv));
        class->size = sizeof(idv);
        break;
      case P_STR:
        cptr = SvPV(val,class->size);
        class->val = strdup(cptr);
        break;
      case P_INT:
        iv = SvIV(val);
        class->val  = memdup(&iv,sizeof(iv));
        class->size = sizeof(iv);
        break;
      case P_FLOAT:
        fv = SvNV(val);
        class->val  = memdup(&fv,sizeof(fv));
        class->size = sizeof(fv);
        break;
      default:
        XSRETURN_UNDEF;
    }

void
destroy(class)
    t_cfxs_param *class
  ALIAS:
    CForum::Clientlib::API::Hook::Parameter::DESTROY = 1
  CODE:
    if(class->val) free(class->val);
    free(class);

MODULE = CForum::Clientlib      PACKAGE = CForum::Clientlib::API::Hook::RetVal

u_char *
to_s(class)
    SV *class
  CODE:
    RETVAL=(u_char *)SvIV((SV *)SvRV(class));
  OUTPUT:
    RETVAL

u_char *
id_to_s(class)
    SV *class
  PREINIT:
    u_int64_t v;
    char buff[50];
  CODE:
    v = *((u_int64_t *)SvIV((SV *)SvRV(class)));
    snprintf(buff,50,"%lld",v);
    RETVAL=strdup(buff);
  OUTPUT:
    RETVAL

int
to_i(class)
    SV *class
  CODE:
    RETVAL=*((int *)SvIV((SV *)SvRV(class)));
  OUTPUT:
    RETVAL

double
to_f(class)
    SV *class
  CODE:
    RETVAL=*((double *)SvIV((SV *)SvRV(class)));
  OUTPUT:
    RETVAL

void
destroy(class)
    SV *class
  CODE:
    free((void *)SvIV((SV *)SvRV(class)));

# eof
