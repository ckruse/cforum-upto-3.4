/**
 * \file cfgcomp.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 *
 * This file contains the wrapper functions for the config file compiler framework
 */

/* {{{ includes */
#include "config.h"
#include "defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include <dlfcn.h>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <inttypes.h>

#include <pwd.h>

#include "utils.h"
#include "cfgcomp.h"
/* }}} */

/* {{{ cf_cfg_cfgcomp_compile_if_needed */
int cf_cfg_cfgcomp_compile_if_needed(const u_char *filename) {
  cf_string_t str,str1;
  struct stat st,st1;
  int retval;
  FILE *fd;

  cf_str_init(&str1);
  cf_str_init_growth(&str,256);

  cf_str_cstr_set(&str,filename);
  cf_str_chars_append(&str,".cf",3);

  if(stat(filename,&st1) == -1) {
    cf_str_cleanup(&str);
    fprintf(stderr,"could not stat %s: %s\n",filename,strerror(errno));
    return -1;
  }

  /* file does not exist or config file has been modified */
  if(stat(str.content,&st) == -1 || st.st_mtime < st1.st_mtime) {
    if((retval = cf_cfg_assemble(filename,&str1)) != 0) {
      cf_str_cleanup(&str);
      cf_str_cleanup(&str1);
      return retval;
    }

    if((fd = fopen(str.content,"wb")) == NULL) {
      cf_str_cleanup(&str);
      cf_str_cleanup(&str1);
      fprintf(stderr,"could not open file %s: %s\n",filename,strerror(errno));
    }

    fwrite(str1.content,1,str1.len,fd);
    fclose(fd);
  }

  cf_str_cleanup(&str);
  cf_str_cleanup(&str1);

  return 0;
}
/* }}} */

/* {{{ cf_cfg_interprete_file */
int cf_cfg_interprete_file(const u_char *filename,cf_cfg_config_t *cfg) {
  cf_string_t str;
  struct stat st;
  FILE *fd;
  int retval;

  cf_cfg_vm_t vm;

  cf_str_init_growth(&str,256);
  cf_str_cstr_set(&str,filename);
  cf_str_chars_append(&str,".cf",3);

  if(stat(str.content,&st) == -1) {
    fprintf(stderr,"could not stat %s: %s\n",str.content,strerror(errno));
    cf_str_cleanup(&str);
    return -1;
  }

  memset(&vm,0,sizeof(vm));

  if((fd = fopen(str.content,"rb")) == NULL) {
    fprintf(stderr,"could not open %s: %s\n",str.content,strerror(errno));
    cf_str_cleanup(&str);
    return -1;
  }

  vm.content = cf_alloc(NULL,1,st.st_size,CF_ALLOC_MALLOC);
  fread(vm.content,1,st.st_size,fd);
  fclose(fd);

  vm.len = st.st_size;

  if((retval = cf_cfg_vm_start(&vm,cfg)) != 0) {
    /* TODO: cleanup vm */
    cf_str_cleanup(&str);
    return retval;
  }

  /* TODO: cleanup vm */
  cf_str_cleanup(&str);

  return 0;
}
/* }}} */

int cf_cfg_cmp(cf_tree_dataset_t *dt,cf_tree_dataset_t *dt1) {
  return strcmp(dt->key,dt1->key);
}

/* {{{ cf_cfg_destroy_value */
void cf_cfg_destroy_value(cf_cfg_config_value_t *val) {
  size_t i;

  if(val->sval) free(val->sval);

  if(val->avals) {
    for(i=0;i<val->alen;++i) cf_cfg_destroy_value(&val->avals[i]);
    free(val->avals);
  }
}
/* }}} */

/* {{{ cf_cfg_destroy_node */
void cf_cfg_destroy_node(cf_tree_dataset_t *dt) {
  cf_cfg_config_value_t *val = (cf_cfg_config_value_t *)dt->data;
  free(dt->key);

  cf_cfg_destroy_value(val);
  free(val);
}
/* }}} */

/* {{{ cf_cfg_config_destroy */
void cf_cfg_config_destroy(cf_cfg_config_t *cfg) {
  if(cfg->name) free(cfg->name);
  cf_tree_destroy(&cfg->args);
  cf_array_destroy(&cfg->nmspcs);
}
/* }}} */

/* {{{ cf_cfg_init_cfg */
void cf_cfg_init_cfg(cf_cfg_config_t *cfg) {
  cfg->name = NULL;
  cf_tree_init(&cfg->args,cf_cfg_cmp,NULL);
  cf_array_init(&cfg->nmspcs,sizeof(*cfg),(void (*)(void *))cf_cfg_config_destroy);
}
/* }}} */

int main(int argc,char *argv[]) {
  cf_cfg_config_t cfg;

  if(argc < 2) {
    fprintf(stderr,"usage: %s <file>\n",argv[0]);
    return EXIT_FAILURE;
  }

  memset(&cfg,0,sizeof(cfg));
  cf_cfg_init_cfg(&cfg);

  if(cf_cfg_cfgcomp_compile_if_needed(argv[1]) != 0) return EXIT_FAILURE;
  if(cf_cfg_interprete_file(argv[1],&cfg) != 0) return EXIT_FAILURE;


  return EXIT_SUCCESS;
}

/* eof */
