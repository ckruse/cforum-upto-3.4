/**
 * \file serverlib.c
 * \author Christian Kruse
 *
 * Contains all server library functions
 */

/* {{{ rw list functions */

/* {{{ cf_rw_list_init */
void cf_rw_list_init(const u_char *name,t_cf_rw_list_head *head) {
  cf_rwlock_init(name,&head->lock);
  cf_list_init(&head->head);
}
/* }}} */

/* {{{ cf_rw_list_append */
void cf_rw_list_append(t_cf_rw_list_head *head,void *data,size_t size) {
  CF_RW_WR(&head->lock);
  cf_list_append(&head->head,data,size);
  CF_RW_UN(&head->lock);
}
/* }}} */

/* {{{ cf_rw_list_append_static */
void cf_rw_list_append_static(t_cf_rw_list_head *head,void *data,size_t size) {
  CF_RW_WR(&head->lock);
  cf_list_append_static(&head->head,data,size);
  CF_RW_UN(&head->lock);
}
/* }}} */

/* {{{ cf_rw_list_prepend */
void cf_rw_list_prepend(t_cf_rw_list_head *head,void *data,size_t size) {
  CF_RW_WR(&head->lock);
  cf_list_prepend(&head->head,data,size);
  CF_RW_UN(&head->lock);
}
/* }}} */

/* {{{ cf_rw_list_prepend_static */
void cf_rw_list_prepend_static(t_cf_rw_list_head *head,void *data,size_t size) {
  CF_RW_WR(&head->lock);
  cf_list_prepend_static(&head->head,data,size);
  CF_RW_UN(&head->lock);
}
/* }}} */

/* {{{ cf_list_insert */
void cf_list_insert(t_cf_rw_list_head *head,t_cf_list_element *prev,void *data,size_t size) {
  CF_RW_WR(&head->lock);
  cf_list_insert(&head->head,prev,data,size);
  CF_RW_UN(&head->lock);
}
/* }}} */

/* {{{ cf_rw_list_search */
void *cf_rw_list_search(t_cf_rw_list_head *head,void *data,int (*compare)(const void *data1,const void *data2)) {
  void *tmp;
  
  CF_RW_RD(&head->lock);
  tmp = cf_list_search(&head->head,data,compare);
  CF_RW_UN(&head->lock);

  return tmp;
}
/* }}} */

/* {{{ cf_rw_list_delete */
void cf_rw_list_delete(t_cf_rw_list_head *head,t_cf_list_element *elem) {
  CF_RW_WR(&head->lock);
  cf_list_delete(&head->head,elem);
  CF_RW_UN(&head->lock);
}
/* }}} */

/* {{{ cf_rw_list_destroy */
void cf_rw_list_destroy(t_cf_rw_list_head *head,void (*destroy)(void *data)) {
  CF_RW_WR(&head.lock);
  cf_list_destroy(&head->head,destroy);
  CF_RW_UN(&head.lock);

  cf_rwlock_destroy(&head->lock);
}
/* }}} */
/* }}} */

/* {{{ cf_register_forum */
t_forum *cf_register_forum(const u_char *name) {
  t_forum *forum = fo_alloc(NULL,1,sizeof(*forum),FO_ALLOC_MALLOC);
  t_name_value *nv;

  forum->name = strdup(name);
  forum->fresh = 0;

  str_init(&forum->cache.visible);
  str_init(&forum->cache.invisible);

  forum->date.visible = forum->date.invisible = 0;

  forum->locked = 0;

  #ifdef CF_SHARED_MEM
  nv = cfg_get_first_value(&fo_server_conf,name,"SharedMemIds");
  forum->shm.ids[0] = atoi(nv->values[0]);
  forum->shm.ids[1] = atoi(nv->values[1]);
  forum->shm.sem    = atoi(nv->values[2]);

  cf_mutex_init("forum.shm.lock",&forum.shm.lock);
  #endif

  forum->threads.last_tid = forum->threads.last_mid = 0;
  forum->threads.threads  = cf_hash_new(NULL);

  cf_rw_list_init("forum.threads.thread_list",&forum->threads.thread_list);
  cf_rwlock_init("forum.threads.lock",&forum->threads.lock);

  forum->uniques.ids = cf_hash_new(NULL);
  cf_mutex_init("forum.uniques.lock",&forum->uniques.lock);

  cf_hash_set_static(head.forums,name,strlen(name),forum,sizeof(*forum));
  return forum;
}
/* }}} */

/* {{{ cf_log */
void cf_log(int mode,const u_char *file,unsigned int line,const u_char *format, ...) {
  u_char str[300];
  int status;
  t_name_value *v;
  time_t t;
  struct tm *tm;
  int sz;
  va_list ap;
  register u_char *ptr,*ptr1;

  #ifndef DEBUG
  if(mode == CF_DBG) return;
  #endif

  for(ptr1=ptr=(u_char *)file;*ptr;ptr++) {
    if(*ptr == '/') ptr1 = ptr + 1;
  }

  t  = time(NULL);
  tm = localtime(&t);

  if((status = pthread_mutex_lock(&head.log.lock)) != 0) {
    fprintf(head.log.err,"pthread_mutex_lock: %s\n",strerror(status));
    return;
  }

  if(!head.log.std) {
    v = cfg_get_first_value(&fo_server_conf,NULL,"StdLog");
    head.log.std = fopen(v->values[0],"a");
  }
  if(!head.log.err) {
    v = cfg_get_first_value(&fo_server_conf,NULL,"ErrorLog");
    head.log.err = fopen(v->values[0],"a");
  }

  sz = snprintf(str,300,"[%4d-%02d-%02d %02d:%02d:%02d]:%s:%d ",tm->tm_year+1900,tm->tm_mon+1,tm->tm_mday,tm->tm_hour,tm->tm_min,tm->tm_sec,ptr1,line);

  va_start(ap, format);
  if(mode == CF_ERR) {
    fwrite(str,sz,1,head.log.err);
    vfprintf(head.log.err,format,ap);
    fflush(head.log.err);

    #ifdef DEBUG
    fwrite(str,sz,1,stderr);
    vfprintf(stderr,format,ap);
    #endif
  }
  else if(mode == CF_STD) {
    fwrite(str,sz,1,head.log.std);
    vfprintf(head.log.std,format,ap);

    /*
     * we want fflush() only if debugging is enabled
     * if not, we want to avoid system calls so fflush()
     * is silly and buffering is ok (stdlog is not critical,
     * stdlog contains only non-critical infos)
     */
    #ifdef DEBUG
    fflush(head.log.std);

    fwrite(str,sz,1,stdout);
    vfprintf(stdout,format,ap);
    #endif
  }
  #ifdef DEBUG
  else {
    fwrite(str,sz,1,head.log.std);
    fwrite(str,sz,1,stdout);
    fwrite("DEBUG: ",7,1,head.log.std);
    fwrite("DEBUG: ",7,1,stdout);
    vfprintf(head.log.std,format,ap);
    vfprintf(stdout,format,ap);
    fflush(head.log.std);
    fflush(stdout);
  }
  #endif
  va_end(ap);

  pthread_mutex_unlock(&head.log.lock);
}
/* }}} */

/* {{{ cf_load_data */
int cf_load_data(t_forum *forum) {
  /* all references to this thread are released, so run the archiver plugins */
  if(Modules[DATA_LOADING_HANDLER].elements) {
    ret = FLT_OK;

    for(i=0;i<Modules[DATA_LOADING_HANDLER].elements && ret == FLT_DECLINE;i++) {
      handler = array_element_at(&Modules[DATA_LOADING_HANDLER],i);
      fkt     = (t_dataloading_filter)handler->func;
      ret     = fkt(forum);
    }
  }

  return ret;
}
/* }}} */

/* eof */
