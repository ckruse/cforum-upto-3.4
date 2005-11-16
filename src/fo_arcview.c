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
#include "cfconfig.h"
#include "defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dlfcn.h>
#include <errno.h>

#include <sys/types.h>
#include <dirent.h>

#include <signal.h>

#include <locale.h>

#include <sys/stat.h>
#include <fcntl.h>

#include <sys/mman.h>

#include <gdome.h>

#include "readline.h"
#include "hashlib.h"
#include "utils.h"
#include "cfgcomp.h"
#include "cfcgi.h"
#include "template.h"
#include "charconvert.h"
#include "clientlib.h"
#include "htmllib.h"
#include "fo_arcview.h"
/* }}} */


cf_hash_t *ArcviewHandlers = NULL;

static int ArcSortMeth = CF_SORT_ASCENDING;

/* {{{ get_month_name */
size_t get_month_name(int month,const u_char *fn,u_char **name) {
  struct tm tm;
  cf_name_value_t *v = cf_cfg_get_first_value(&fo_default_conf,fn,"DateLocale");
  if(!v) return 0;

  *name = cf_alloc(NULL,BUFSIZ,1,CF_ALLOC_MALLOC);

  memset(&tm,0,sizeof(tm));
  tm.tm_mon = month-1;

  setlocale(LC_TIME,v->values[0]);
  return strftime(*name,BUFSIZ,"%B",&tm);
}
/* }}} */

/* {{{ validation wrapper functions */
int validate_year(const u_char *year) {
  is_valid_year_t pi;

  if((pi = cf_hash_get(ArcviewHandlers,"av_validate_year",16)) != NULL) return pi(year);

  return FLT_EXIT;
}

int validate_month(const u_char *year,const u_char *month) {
  is_valid_month_t pi;

  if((pi = cf_hash_get(ArcviewHandlers,"av_validate_month",17)) != NULL) return pi(year,month);

  return FLT_EXIT;
}

int validate_thread(const u_char *year,const u_char *month,const u_char *tid) {
  is_valid_thread_t pi;

  if((pi = cf_hash_get(ArcviewHandlers,"av_validate_thread",18)) != NULL) return pi(year,month,tid);

  return FLT_EXIT;
}
/* }}} */

/* {{{ signal handler for bad signals */
void sighandler(int segnum) {
  FILE *fd = fopen(PROTOCOL_FILE,"a");
  u_char buff[10],*uname = NULL,*qs = NULL,*pi;

  if(fd) {
    qs    = getenv("QUERY_STRING");
    pi    = getenv("PATH_INFO");

    if(GlobalValues) uname = cf_hash_get(GlobalValues,"UserName",8);

    switch(segnum) {
      case SIGSEGV:
        snprintf(buff,10,"SIGSEGV");
        break;
      case SIGILL:
        snprintf(buff,10,"SIGILL");
        break;
      case SIGFPE:
        snprintf(buff,10,"SIGFPE");
        break;
      case SIGBUS:
        snprintf(buff,10,"SIGBUS");
        break;
      default:
        snprintf(buff,10,"UKNOWN");
        break;
    }

    fprintf(fd,"fo_arcview: Got signal %s!\nUsername: %s\nQuery-String: %s\nPATH_INFO: %s\n----\n",buff,uname?uname:(u_char *)"(null)",qs?qs:(u_char *)"(null)",pi?pi:(u_char *)"(null)");
    fclose(fd);
  }

  exit(0);
}
/* }}} */

/* {{{ cf_array_numeric_compare */
int cf_array_numeric_compare(const void *elem1,const void *elem2) {
  int elem1_i = *((int *)elem1);
  int elem2_i = *((int *)elem2);

  switch(ArcSortMeth) {
    case CF_SORT_ASCENDING:
      if(elem1_i < elem2_i)  return -1;
      if(elem1_i == elem2_i) return  0;
      if(elem1_i > elem2_i)  return  1;

    case CF_SORT_DESCENDING:
      if(elem1_i < elem2_i)  return  1;
      if(elem1_i == elem2_i) return  0;
      if(elem1_i > elem2_i)  return -1;
  }

  /* eh? */
  return 0;
}
/* }}} */

/* {{{ show_years */
void show_years() {
  size_t i;
  int *y;

  cf_array_t *ary;
  get_years_t gy;

  u_char *fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  u_char *username = cf_hash_get(GlobalValues,"UserName",8),*script;
  cf_name_value_t *cs = cf_cfg_get_first_value(&fo_default_conf,fn,"ExternCharset");
  cf_name_value_t *sy = cf_cfg_get_first_value(&fo_arcview_conf,fn,"SortYearList");
  cf_name_value_t *yt = cf_cfg_get_first_value(&fo_arcview_conf,fn,"YearsTemplate");
  cf_name_value_t *forumpath = cf_cfg_get_first_value(&fo_default_conf,fn,username?"UBaseURL":"BaseURL");

  cf_template_t tpl;

  u_char yt_name[256];

  cf_tpl_variable_t array;

  if((gy = cf_hash_get(ArcviewHandlers,"av_get_years",12)) == NULL) {
    printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
    cf_error_message("E_ARCHIVE_ERROR",NULL);
    return;
  }

  if((ary = gy()) == NULL) {
    printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
    cf_error_message("E_ARCHIVE_ERROR",NULL);
    return;
  }

  ArcSortMeth = cf_strcmp(sy->values[0],"ascending") == 0 ? CF_SORT_ASCENDING : CF_SORT_DESCENDING;
  cf_array_sort(ary,cf_array_numeric_compare);

  cf_gen_tpl_name(yt_name,256,yt->values[0]);
  if(cf_tpl_init(&tpl,yt_name) != 0) {
    printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
    cf_error_message("E_CONFIG_ERR",NULL);
    return;
  }

  cf_tpl_var_init(&array,TPL_VARIABLE_ARRAY);

  for(i=0;i<ary->elements;++i) {
    y = cf_array_element_at(ary,i);
    cf_tpl_var_addvalue(&array,TPL_VARIABLE_INT,*y);
  }

  cf_tpl_setvar(&tpl,"years",&array);
  cf_set_variable(&tpl,cs,"forumbase",forumpath->values[0],strlen(forumpath->values[0]),1);

  cf_set_variable(&tpl,cs,"charset",cs->values[0],strlen(cs->values[0]),1);
  if((script = getenv("SCRIPT_NAME")) != NULL) cf_set_variable(&tpl,cs,"script",script,strlen(script),1);

  printf("Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
  cf_tpl_parse(&tpl);

  cf_tpl_finish(&tpl);

  cf_array_destroy(ary);
  free(ary);
}
/* }}} */

/* {{{ show_year_content */
void show_year_content(const u_char *year) {
  int *y;
  size_t i,len;

  get_monthlist_t ml;

  u_char *fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  u_char *username = cf_hash_get(GlobalValues,"UserName",8),*script;
  u_char mt_name[256],*mname;

  cf_name_value_t *mt = cf_cfg_get_first_value(&fo_arcview_conf,fn,"MonthsTemplate");
  cf_name_value_t *cs = cf_cfg_get_first_value(&fo_default_conf,fn,"ExternCharset");
  cf_name_value_t *sm = cf_cfg_get_first_value(&fo_arcview_conf,fn,"SortMonthList");
  cf_name_value_t *forumpath = cf_cfg_get_first_value(&fo_default_conf,fn,username?"UBaseURL":"BaseURL");

  cf_array_t *ary;

  cf_template_t mt_tpl;

  cf_tpl_variable_t array,array1;

  if((ml = cf_hash_get(ArcviewHandlers,"av_get_monthlist",16)) == NULL) {
    printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
    cf_error_message("E_ARCHIVE_ERROR",NULL);
    return;
  }

  if((ary = ml(year)) == NULL) {
    printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
    cf_error_message("E_ARCHIVE_ERROR",NULL);
    return;
  }

  ArcSortMeth = cf_strcmp(sm->values[0],"ascending") == 0 ? CF_SORT_ASCENDING : CF_SORT_DESCENDING;
  cf_array_sort(ary,cf_array_numeric_compare);

  cf_gen_tpl_name(mt_name,256,mt->values[0]);
  if(cf_tpl_init(&mt_tpl,mt_name) != 0) {
    cf_error_message("E_CONFIG_ERR",NULL);
    return;
  }

  cf_tpl_var_init(&array,TPL_VARIABLE_ARRAY);

  for(i=0;i<ary->elements;++i) {
    y   = cf_array_element_at(ary,i);
    len = get_month_name(*y,fn,&mname);

    cf_tpl_var_init(&array1,TPL_VARIABLE_ARRAY);
    cf_tpl_var_addvalue(&array1,TPL_VARIABLE_INT,*y);
    cf_tpl_var_addvalue(&array1,TPL_VARIABLE_STRING,mname,len);

    free(mname);

    cf_tpl_var_add(&array,&array1);
  }

  cf_tpl_setvar(&mt_tpl,"months",&array);
  cf_set_variable(&mt_tpl,cs,"forumbase",forumpath->values[0],strlen(forumpath->values[0]),1);
  cf_set_variable(&mt_tpl,cs,"year",year,strlen(year),1);

  cf_set_variable(&mt_tpl,cs,"charset",cs->values[0],strlen(cs->values[0]),1);
  if((script = getenv("SCRIPT_NAME")) != NULL) cf_set_variable(&mt_tpl,cs,"script",script,strlen(script),1);

  printf("Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
  cf_tpl_parse(&mt_tpl);

  cf_tpl_finish(&mt_tpl);

  cf_array_destroy(ary);
  free(ary);
}
/* }}} */

/* {{{ sort_threadlist */
int sort_threadlist(const void *a,const void *b) {
  arc_tl_ent_t *ea = (arc_tl_ent_t *)a;
  arc_tl_ent_t *eb = (arc_tl_ent_t *)b;

  switch(ArcSortMeth) {
    case CF_SORT_ASCENDING:
      if(ea->date < eb->date)  return -1;
      if(ea->date == eb->date) return  0;
      if(ea->date > eb->date)  return  1;

    case CF_SORT_DESCENDING:
      if(ea->date < eb->date)  return  1;
      if(ea->date == eb->date) return  0;
      if(ea->date > eb->date)  return -1;
  }

  /* hu? */
  return 0;
}
/* }}} */

/* {{{ prep_var */
size_t prep_var(const u_char *val,size_t len,u_char **out,cf_name_value_t *cs,int html) {
  u_char *tmp = NULL;
  size_t len1 = 0;

  if(cf_strcmp(cs->values[0],"UTF-8") != 0) {
    if(html) {
      tmp = htmlentities_charset_convert(val,"UTF-8",cs->values[0],&len1,0);
      html = 0;
    }
    else tmp = charset_convert_entities(val,len,"UTF-8",cs->values[0],&len1);

    /* This should only happen if we use charset_convert() -- and we should not use it. */
    if(!tmp) {
      tmp = htmlentities(val,0);
      len1 = strlen(val);
    }
  }
  /* ExternCharset is also UTF-8 */
  else {
    if(html) {
      tmp = htmlentities(val,0);
      len1 = strlen(tmp);
    }
  }

  *out = tmp;
  return len1;
}
/* }}} */

/* {{{ show_month_content */
void show_month_content(const u_char *year,const u_char *month) {
  get_threadlist_t gt;
  month_last_modified_t mt;

  int cache_level = 0,do_cache = 0,show_invisible = cf_hash_get(GlobalValues,"ShowInvisible",13) != NULL;
  size_t i,len,len1;
  time_t last_modified;

  u_char *fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  u_char mt_name[256],pi[256],*tmp,*tmp1,*script;

  cf_name_value_t *cs = cf_cfg_get_first_value(&fo_default_conf,fn,"ExternCharset");
  cf_name_value_t *stl = cf_cfg_get_first_value(&fo_arcview_conf,fn,"SortThreadList");
  cf_name_value_t *m_tp = cf_cfg_get_first_value(&fo_arcview_conf,fn,"MonthsTemplate");
  cf_name_value_t *forumpath = cf_cfg_get_first_value(&fo_default_conf,fn,"BaseURL");
  cf_name_value_t *ecache = cf_cfg_get_first_value(&fo_arcview_conf,fn,"EnableCache");
  cf_name_value_t *df = cf_cfg_get_first_value(&fo_arcview_conf,fn,"DateFormatList");
  cf_name_value_t *lc = cf_cfg_get_first_value(&fo_default_conf,fn,"DateLocale");
  cf_name_value_t *cache  = NULL;
  cf_name_value_t *clevel = NULL;

  cf_array_t *ary;

  arc_tl_ent_t *ent;

  cf_cache_entry_t *cent;

  cf_template_t m_tpl;
  cf_tpl_variable_t array,array1;

  /* {{{ check for cache */
  if(ecache && *ecache->values[0] == 'y' && !show_invisible) {
    do_cache = 1;
    cache  = cf_cfg_get_first_value(&fo_arcview_conf,fn,"CacheDir");
    clevel = cf_cfg_get_first_value(&fo_arcview_conf,fn,"CacheLevel");

    if(ecache) cache_level = atoi(clevel->values[0]);
    else       cache_level = 6;

    snprintf(pi,256,"%s/%s.idx",year,month);

    if((mt = cf_hash_get(ArcviewHandlers,"av_threadlist_lm",16)) != NULL) {
      if((last_modified = mt(year,month)) != 0) {
        if(cf_cache_outdated_date(cache->values[0],pi,last_modified) != -1 && (cent = cf_get_cache(cache->values[0],pi,cache_level)) != NULL) {
          printf("Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
          fwrite(cent->ptr,1,cent->size,stdout);
          cf_cache_destroy(cent);
          return;
        }
      }
    }
  }
  /* }}} */

  if((gt = cf_hash_get(ArcviewHandlers,"av_get_threadlist",17)) == NULL) {
    printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
    cf_error_message("E_ARCHIVE_ERROR",NULL);
    return;
  }

  if((ary = gt(year,month)) == NULL) {
    printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
    cf_error_message("E_ARCHIVE_ERROR",NULL);
    return;
  }

  ArcSortMeth = cf_strcmp(stl->values[0],"ascending") == 0 ? CF_SORT_ASCENDING : CF_SORT_DESCENDING;
  cf_array_sort(ary,sort_threadlist);

  cf_gen_tpl_name(mt_name,256,m_tp->values[0]);

  if(cf_tpl_init(&m_tpl,mt_name) != 0) {
    printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
    cf_error_message("E_CONFIG_ERR",NULL);
    return;
  }


  cf_tpl_var_init(&array,TPL_VARIABLE_ARRAY);

  for(i=0;i<ary->elements;++i) {
    ent = cf_array_element_at(ary,i);

    if(ent->invisible && show_invisible == 0) continue;

    cf_tpl_var_init(&array1,TPL_VARIABLE_ARRAY);

    len = prep_var(ent->author,ent->alen,&tmp,cs,1);
    cf_tpl_var_addvalue(&array1,TPL_VARIABLE_STRING,tmp?tmp:ent->author,tmp?len:ent->alen);
    if(tmp) free(tmp);

    len = prep_var(ent->subject,ent->slen,&tmp,cs,1);
    cf_tpl_var_addvalue(&array1,TPL_VARIABLE_STRING,tmp?tmp:ent->subject,tmp?len:ent->slen);
    if(tmp) free(tmp);

    cf_tpl_var_addvalue(&array1,TPL_VARIABLE_STRING,ent->tid,ent->tlen);

    tmp1 = cf_general_get_time(df->values[0],lc->values[0],&len1,&ent->date);
    if(tmp1) {
      len = prep_var(tmp1,len1,&tmp,cs,1);
      cf_tpl_var_addvalue(&array1,TPL_VARIABLE_STRING,tmp?tmp:tmp1,tmp?len:len1);
      free(tmp1);
      if(tmp) free(tmp);
    }

    cf_tpl_var_addvalue(&array1,TPL_VARIABLE_INT,ent->invisible);

    if(ent->cat) {
      len = prep_var(ent->cat,ent->clen,&tmp,cs,1);
      cf_tpl_var_addvalue(&array1,TPL_VARIABLE_STRING,tmp?tmp:ent->cat,tmp?len:ent->clen);
      if(tmp) free(tmp);
    }

    cf_tpl_var_add(&array,&array1);
  }

  len = get_month_name(atoi(month),fn,&tmp);
  cf_set_variable(&m_tpl,cs,"month",tmp,len,1);
  free(tmp);
  cf_set_variable(&m_tpl,cs,"year",year,strlen(year),1);
  cf_set_variable(&m_tpl,cs,"forumbase",forumpath->values[0],strlen(forumpath->values[0]),1);

  cf_tpl_setvar(&m_tpl,"threads",&array);

  cf_set_variable(&m_tpl,cs,"charset",cs->values[0],strlen(cs->values[0]),1);
  if((script = getenv("SCRIPT_NAME")) != NULL) cf_set_variable(&m_tpl,cs,"script",script,strlen(script),1);

  printf("Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
  cf_tpl_parse_to_mem(&m_tpl);

  fwrite(m_tpl.parsed.content,1,m_tpl.parsed.len,stdout);
  if(show_invisible == 0 && do_cache) cf_cache(cache->values[0],pi,m_tpl.parsed.content,m_tpl.parsed.len,cache_level);

  cf_tpl_finish(&m_tpl);

  cf_array_destroy(ary);
  free(ary);
}
/* }}} */

/* {{{ msgs_cmp */
int msgs_cmp(const void *a,const void *b) {
  hierarchical_node_t *na = (hierarchical_node_t *)a;
  hierarchical_node_t *nb = (hierarchical_node_t *)b;

  switch(ArcSortMeth) {
    case CF_SORT_ASCENDING:
      if(na->msg->date > nb->msg->date) return 1;
      else if(na->msg->date < nb->msg->date) return -1;

    case CF_SORT_DESCENDING:
      if(na->msg->date > nb->msg->date) return -1;
      else if(na->msg->date < nb->msg->date) return 1;
  }

  return 0;
}
/* }}} */

/* {{{ sort_messages */
void sort_messages(hierarchical_node_t *h) {
  size_t i;

  cf_array_sort(&h->childs,msgs_cmp);

  for(i=0;i<h->childs.elements;++i) sort_messages(cf_array_element_at(&h->childs,i));
}
/* }}} */

/* {{{ generate_thread_output */
void generate_thread_output(cl_thread_t *thread,hierarchical_node_t *msg,cf_tpl_variable_t *threads,cf_string_t *threadlist,cf_template_t *tl_tpl,cf_name_value_t *cs,int admin,int show_invisible,const u_char *fn) {
  size_t i;
  hierarchical_node_t *child;
  size_t len,len1,cntlen;
  cf_string_t str;
  u_char *cnt = NULL;
  int printed = 0;
  u_char *date,*tmp;
  cf_string_t strbuffer;
  cf_name_value_t *qc = cf_cfg_get_first_value(&fo_arcview_conf,fn,"QuotingChars"),
               *ms = cf_cfg_get_first_value(&fo_arcview_conf,fn,"MaxSigLines"),
               *ss = cf_cfg_get_first_value(&fo_arcview_conf,fn,"ShowSig");

  cf_name_value_t *tf = cf_cfg_get_first_value(&fo_arcview_conf,fn,"DateFormatViewList");
  cf_name_value_t *dl = cf_cfg_get_first_value(&fo_default_conf,fn,"DateLocale");

  cf_tpl_variable_t ary;

  cf_str_init(&strbuffer);
  cf_str_init(&str);

  cf_tpl_var_init(&ary,TPL_VARIABLE_ARRAY);


  /* {{{ first: set threadlist and per thread variables */
  cf_str_char_append(&strbuffer,'m');
  cf_uint64_to_str(&strbuffer,msg->msg->mid);

  cf_set_variable(tl_tpl,cs,"mid",strbuffer.content,strbuffer.len,1);
  cf_set_variable(tl_tpl,cs,"subject",msg->msg->subject.content,msg->msg->subject.len,1);
  cf_set_variable(tl_tpl,cs,"author",msg->msg->author.content,msg->msg->author.len,1);
  if(msg->msg->invisible) cf_tpl_setvalue(tl_tpl,"deleted",TPL_VARIABLE_INT,1);
  else cf_tpl_freevar(tl_tpl,"deleted");

  cf_tpl_var_addvalue(&ary,TPL_VARIABLE_STRING,strbuffer.content,strbuffer.len);

  len = prep_var(msg->msg->subject.content,msg->msg->subject.len,&tmp,cs,1);
  cf_tpl_var_addvalue(&ary,TPL_VARIABLE_STRING,tmp?tmp:msg->msg->subject.content,tmp?len:msg->msg->subject.len);
  if(tmp) free(tmp);

  len = prep_var(msg->msg->author.content,msg->msg->author.len,&tmp,cs,1);
  cf_tpl_var_addvalue(&ary,TPL_VARIABLE_STRING,tmp?tmp:msg->msg->author.content,tmp?len:msg->msg->author.len);
  if(tmp) free(tmp);

  if((date = cf_general_get_time(tf->values[0],dl->values[0],&len,&msg->msg->date)) != NULL) {
    cf_set_variable(tl_tpl,cs,"date",date,len,1);

    len1 = prep_var(date,len,&tmp,cs,1);
    cf_tpl_var_addvalue(&ary,TPL_VARIABLE_STRING,tmp?tmp:date,tmp?len1:len);
    if(tmp) free(tmp);

    free(date);
  }

  /* {{{ add message content to array */
  /* convert message to the right charset */
  if(cf_strcmp(cs->values[0],"UTF-8") == 0 || (cnt = charset_convert_entities(msg->msg->content.content,msg->msg->content.len,"UTF-8",cs->values[0],&cntlen)) == NULL) {
    cnt = strdup(msg->msg->content.content);
  }

  /* convert message to html */
  msg_to_html(thread,cnt,&str,NULL,qc->values[0],ms ? atoi(ms->values[0]) : -1,ss ? cf_strcmp(ss->values[0],"yes") == 0 : 1);

  free(cnt);

  cf_tpl_var_addvalue(&ary,TPL_VARIABLE_STRING,str.content,str.len);
  cf_str_cleanup(&str);
  /* }}} */

  if(msg->msg->category.len) cf_set_variable(tl_tpl,cs,"category",msg->msg->category.content,msg->msg->category.len,1);
  else cf_tpl_freevar(tl_tpl,"category");

  /* category */
  if(msg->msg->category.len) {
    len = prep_var(msg->msg->category.content,msg->msg->category.len,&tmp,cs,1);
    cf_tpl_var_addvalue(&ary,TPL_VARIABLE_STRING,tmp?tmp:msg->msg->category.content,tmp?len:msg->msg->category.len);
    if(tmp) free(tmp);
  }
  else cf_tpl_var_addvalue(&ary,TPL_VARIABLE_INT,0);

  /* email */
  if(msg->msg->email.len) {
    len = prep_var(msg->msg->email.content,msg->msg->email.len,&tmp,cs,1);
    cf_tpl_var_addvalue(&ary,TPL_VARIABLE_STRING,tmp?tmp:msg->msg->email.content,tmp?len:msg->msg->email.len);
    if(tmp) free(tmp);
  }
  else cf_tpl_var_addvalue(&ary,TPL_VARIABLE_INT,0);

  /* image url */
  if(msg->msg->img.len) {
    len = prep_var(msg->msg->img.content,msg->msg->img.len,&tmp,cs,1);
    cf_tpl_var_addvalue(&ary,TPL_VARIABLE_STRING,tmp?tmp:msg->msg->img.content,tmp?len:msg->msg->img.len);
    if(tmp) free(tmp);
  }
  else cf_tpl_var_addvalue(&ary,TPL_VARIABLE_INT,0);

  /* homepage url */
  if(msg->msg->hp.len) {
    len = prep_var(msg->msg->hp.content,msg->msg->hp.len,&tmp,cs,1);
    cf_tpl_var_addvalue(&ary,TPL_VARIABLE_STRING,tmp?tmp:msg->msg->hp.content,tmp?len:msg->msg->hp.len);
    if(tmp) free(tmp);
  }
  else cf_tpl_var_addvalue(&ary,TPL_VARIABLE_INT,0);
  /* }}} */

  /* parse threadlist and append output to threadlist content */
  cf_tpl_parse_to_mem(tl_tpl);
  cf_str_chars_append(threadlist,tl_tpl->parsed.content,tl_tpl->parsed.len);

  tl_tpl->parsed.len = 0;

  cf_tpl_var_add(threads,&ary);

  if(msg->childs.elements) {
    cf_str_chars_append(threadlist,"<ul>",4);

    for(i=0;i<msg->childs.elements;i++) {
      child = cf_array_element_at(&msg->childs,i);
      if(child->msg->invisible == 1 && (admin == 0 || show_invisible == 0)) continue;

      printed = 1;

      cf_str_chars_append(threadlist,"<li>",4);
      generate_thread_output(thread,child,threads,threadlist,tl_tpl,cs,admin,show_invisible,fn);
      cf_str_chars_append(threadlist,"</li>",5);
    }

    if(printed) cf_str_chars_append(threadlist,"</ul>",5);
    else {
      threadlist->len -= 4;
      *(threadlist->content + threadlist->len) = '\0';
    }
  }
}
/* }}} */

/* {{{ print_thread */
void print_thread(cl_thread_t *thr,const u_char *year,const u_char *month,const u_char *pi,int admin,int show_invisible) {
  u_char *fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);

  cf_name_value_t *main_tpl_cfg       = cf_cfg_get_first_value(&fo_arcview_conf,fn,"ThreadTemplate");
  cf_name_value_t *threadlist_tpl_cfg = cf_cfg_get_first_value(&fo_arcview_conf,fn,"ThreadListTemplate");
  cf_name_value_t *forumpath          = cf_cfg_get_first_value(&fo_default_conf,fn,"BaseURL");
  cf_name_value_t *ecache = cf_cfg_get_first_value(&fo_arcview_conf,fn,"EnableCache");
  cf_name_value_t *cache, *clevel;

  cf_name_value_t *cs = cf_cfg_get_first_value(&fo_default_conf,fn,"ExternCharset");

  u_char main_tpl_name[256],threadlist_tpl_name[256];
  cf_template_t main_tpl,threadlist_tpl;

  u_char *tmp,*script;
  int len,cache_level;

  cf_string_t threadlist;

  cf_tpl_variable_t ary;

  /* Buarghs. Four templates. This is fucking bad. */
  cf_gen_tpl_name(main_tpl_name,256,main_tpl_cfg->values[0]);
  cf_gen_tpl_name(threadlist_tpl_name,256,threadlist_tpl_cfg->values[0]);

  if(cf_tpl_init(&main_tpl,main_tpl_name) != 0 || cf_tpl_init(&threadlist_tpl,threadlist_tpl_name) != 0) {
    printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
    cf_error_message("E_TPL_NOT_FOUND",NULL);
    return;
  }

  len = get_month_name(atoi(month),fn,&tmp);

  cf_set_variable(&main_tpl,cs,"month",tmp,len,1);
  cf_set_variable(&main_tpl,cs,"year",year,strlen(year),1);
  cf_set_variable(&main_tpl,cs,"subject",thr->messages->subject.content,thr->messages->subject.len,1);
  cf_set_variable(&main_tpl,cs,"charset",cs->values[0],strlen(cs->values[0]),1);
  cf_set_variable(&main_tpl,cs,"forumbase",forumpath->values[0],strlen(forumpath->values[0]),1);

  free(tmp);

  cf_str_init(&threadlist);
  cf_tpl_var_init(&ary,TPL_VARIABLE_ARRAY);

  generate_thread_output(thr,thr->ht,&ary,&threadlist,&threadlist_tpl,cs,admin,show_invisible,fn);

  cf_tpl_setvar(&main_tpl,"threads",&ary);
  cf_tpl_setvalue(&main_tpl,"threadlist",TPL_VARIABLE_STRING,threadlist.content,threadlist.len);

  cf_set_variable(&main_tpl,cs,"charset",cs->values[0],strlen(cs->values[0]),1);
  if((script = getenv("SCRIPT_NAME")) != NULL) cf_set_variable(&main_tpl,cs,"script",script,strlen(script),1);

  cf_tpl_parse_to_mem(&main_tpl);

  printf("Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
  fwrite(main_tpl.parsed.content,1,main_tpl.parsed.len,stdout);

  if(show_invisible == 0 && ecache && *ecache->values[0] == 'y') {
    cache  = cf_cfg_get_first_value(&fo_arcview_conf,fn,"CacheDir");
    clevel = cf_cfg_get_first_value(&fo_arcview_conf,fn,"CacheLevel");

    if(clevel) cache_level = atoi(clevel->values[0]);
    else       cache_level = 6;

    cf_cache(cache->values[0],pi,main_tpl.parsed.content,main_tpl.parsed.len,cache_level);
  }

  cf_tpl_finish(&main_tpl);
  cf_tpl_finish(&threadlist_tpl);
}
/* }}} */

/* {{{ show_thread */
void show_thread(const u_char *year,const u_char *month,const u_char *tid) {
  get_thread_t gt;
  thread_last_modified_t tlm;

  int cache_level;
  int show_invisible = cf_hash_get(GlobalValues,"ShowInvisible",13) != NULL;
  time_t last_modified;

  u_char *fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  u_char *uname = cf_hash_get(GlobalValues,"UserName",8);
  u_char pi[256];

  mod_api_t is_admin = cf_get_mod_api_ent("is_admin");
  int admin = uname ? is_admin(uname) == NULL ? 0 : 1 : 0;

  cf_name_value_t *cs = cf_cfg_get_first_value(&fo_default_conf,fn,"ExternCharset");
  cf_name_value_t *sm = cf_cfg_get_first_value(&fo_arcview_conf,fn,"SortMessages");
  cf_name_value_t *ecache = cf_cfg_get_first_value(&fo_arcview_conf,fn,"EnableCache");
  cf_name_value_t *cache, *clevel;

  cf_cache_entry_t *cent;

  cl_thread_t *thr;

  /* {{{ check for cache */
  if(ecache && *ecache->values[0] == 'y' && !show_invisible) {
    cache  = cf_cfg_get_first_value(&fo_arcview_conf,fn,"CacheDir");
    clevel = cf_cfg_get_first_value(&fo_arcview_conf,fn,"CacheLevel");

    if(ecache) cache_level = atoi(clevel->values[0]);
    else       cache_level = 6;

    snprintf(pi,256,"%s/%s/t%s",year,month,*tid == 't' ? tid+1 : tid);

    if((tlm = cf_hash_get(ArcviewHandlers,"av_thread_lm",12)) != NULL) {
      if((last_modified = tlm(year,month,tid)) != 0) {
        if(cf_cache_outdated_date(cache->values[0],pi,last_modified) != -1 && (cent = cf_get_cache(cache->values[0],pi,cache_level)) != NULL) {
          printf("Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
          fwrite(cent->ptr,1,cent->size,stdout);
          cf_cache_destroy(cent);
          return;
        }
      }
    }
  }
  /* }}} */

  if((gt = cf_hash_get(ArcviewHandlers,"av_get_thread",13)) == NULL) {
    printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
    cf_error_message("E_ARCHIVE_ERROR",NULL);
    return;
  }

  if((thr = gt(year,month,tid)) == NULL) {
    printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
    cf_error_message("E_ARCHIVE_ERROR",NULL);
    return;
  }

  /*
   * we have to sort the messages in the thread; due to our hierarchical
   * structure this is very easy
   */
  ArcSortMeth = cf_strcmp(sm->values[0],"ascending") == 0 ? CF_SORT_ASCENDING : CF_SORT_DESCENDING;
  sort_messages(thr->ht);
  cf_msg_linearize(thr,thr->ht);

  if(thr->messages->invisible == 0 || (admin == 1 && show_invisible == 1)) print_thread(thr,year,month,pi,admin,show_invisible);
  else {
    printf("Status: 404 Not Found\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
    cf_error_message("E_FO_404",NULL);
  }

}
/* }}} */

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

  u_char *forum_name = NULL,*fname;
  cf_array_t *cfgfiles;
  cf_configfile_t conf,dconf;
  cf_name_value_t *cs,*v;
  u_char **infos = NULL,*pi;
  cf_hash_t *head;
  cf_string_t tmp;

  cf_readmode_t rm_infos;

  size_t len;
  int ret = 0;

  /* set signal handler for SIGSEGV (for error reporting) */
  signal(SIGSEGV,sighandler);
  signal(SIGILL,sighandler);
  signal(SIGFPE,sighandler);
  signal(SIGBUS,sighandler);

  if((cfgfiles = cf_get_conf_file(wanted,2)) == NULL) {
    fprintf(stderr,"Could not find configuration files...\n");
    return EXIT_FAILURE;
  }

  ArcviewHandlers = cf_hash_new(NULL);

  head = cf_cgi_new();
  len  = cf_cgi_path_info_parsed(&infos);

  cf_cfg_init();
  init_modules();
  cf_init();
  cf_htmllib_init();

  /* {{{ read configuration */
  fname = *((u_char **)cf_array_element_at(cfgfiles,0));
  cf_cfg_init_file(&dconf,fname);
  free(fname);

  fname = *((u_char **)cf_array_element_at(cfgfiles,1));
  cf_cfg_init_file(&conf,fname);
  free(fname);

  cf_cfg_register_options(&dconf,default_options);
  cf_cfg_register_options(&conf,fo_arcview_options);

  if(cf_read_config(&dconf,NULL,CF_CFG_MODE_CONFIG) != 0 || cf_read_config(&conf,NULL,CF_CFG_MODE_CONFIG) != 0) {
    fprintf(stderr,"config file error!\n");

    cf_cfg_cleanup_file(&conf);
    cf_cfg_cleanup_file(&dconf);

    return EXIT_FAILURE;
  }
  /* }}} */

  /* {{{ ensure that CF_FORUM_NAME is set and we have got a context in every file */
  if((forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10)) == NULL) {
    fprintf(stderr,"Could not get forum name!");

    cf_cfg_cleanup_file(&conf);
    cf_cfg_cleanup_file(&dconf);

    cf_cfg_destroy();
    cf_fini();

    return EXIT_FAILURE;
  }

  if(cf_cfg_get_first_value(&fo_default_conf,forum_name,"ThreadIndexFile") == NULL) {
    fprintf(stderr,"Have no context for forum %s in default configuration file!\n",forum_name);

    cf_cfg_cleanup_file(&conf);
    cf_cfg_cleanup_file(&dconf);

    cf_cfg_destroy();
    cf_fini();

    return EXIT_FAILURE;
  }

  if(cf_cfg_get_first_value(&fo_arcview_conf,forum_name,"SortYearList") == NULL) {
    fprintf(stderr,"Have no context for forum %s in fo_view configuration file!\n",forum_name);

    cf_cfg_cleanup_file(&conf);
    cf_cfg_cleanup_file(&dconf);

    cf_cfg_destroy();
    cf_fini();

    return EXIT_FAILURE;
  }
  /* }}} */

  cs = cf_cfg_get_first_value(&fo_default_conf,forum_name,"ExternCharset");

  /* first action: authorization modules */
  ret = cf_run_auth_handlers(head);

  /* {{{ check if URI ends with a slash */
  if((pi = getenv("PATH_INFO")) != NULL && ret != FLT_EXIT) {
    if(*pi && pi[strlen(pi)-1] != '/') {
      cf_str_init(&tmp);

      v = cf_cfg_get_first_value(&fo_default_conf,forum_name,"ArchiveURL");
      cf_str_chars_append(&tmp,v->values[0],strlen(v->values[0]));
      cf_str_chars_append(&tmp,pi,strlen(pi));
      cf_str_char_append(&tmp,'/');

      cf_http_redirect_with_nice_uri(tmp.content,1);
      cf_str_cleanup(&tmp);

      ret = FLT_EXIT;
    }
  }
  /* }}} */

  if(ret != FLT_EXIT) {
    memset(&rm_infos,0,sizeof(rm_infos));
    v = cf_cfg_get_first_value(&fo_default_conf,forum_name,"PostingURL");
    rm_infos.posting_uri[0] = v->values[0];

    v = cf_cfg_get_first_value(&fo_default_conf,forum_name,"UPostingURL");
    rm_infos.posting_uri[1] = v->values[0];
    cf_hash_set(GlobalValues,"RM",2,&rm_infos,sizeof(rm_infos));
  }

  if(ret != FLT_EXIT && (ret = cf_run_init_handlers(head)) != FLT_EXIT) {
    switch(len) {
      case 3:
        ret = validate_thread(infos[0],infos[1],infos[2]);
        break;
      case 2:
        ret = validate_month(infos[0],infos[1]);
        break;
      case 1:
        ret = validate_year(infos[0]);
        break;

      case 0:
        ret = FLT_OK;
        break;

      default:
        printf("Status: 404 Not Found\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
        cf_error_message("E_ARCHIVE_GENERIC404",NULL);
        ret = FLT_EXIT;
    }

    if(ret != FLT_EXIT) {
      switch(len) {
        case 0:
          show_years();
          break;
        case 1:
          show_year_content(infos[0]);
          break;
        case 2:
          show_month_content(infos[0],infos[1]);
          break;
        case 3:
          show_thread(infos[0],infos[1],infos[2]);
          break;
      }
    }

  }


  return EXIT_SUCCESS;
}

/* eof */
