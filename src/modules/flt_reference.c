/**
 * \file flt_reference.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 *
 * This plugin changes [ref:]-tags to real URIs
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

#include "readline.h"
#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "cfcgi.h"
#include "template.h"
#include "clientlib.h"
#include "charconvert.h"
/* }}} */

typedef struct {
  u_char *id;
  u_char *uri;
} t_ref_uri;

static t_array ref_uris = { 0, 0, 0, NULL, NULL };

void flt_reference_cleanup_entry(void *e) {
  t_ref_uri *uri = (t_ref_uri *)e;
  free(uri->uri);
  free(uri->id);
}

int flt_reference_handle_posting(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_cl_thread *thr,t_cf_template *tpl) {
  t_string str;
  int found = 0;
  register u_char *ptr;
  u_char *id,*uri,*start,*save;
  size_t i;
  t_ref_uri *suri;

  str_init(&str);

  for(ptr=thr->messages->content;*ptr;ptr++) {
    if(*ptr == '[') {
      if(cf_strncasecmp(ptr,"[ref:",5) == 0) {
        save = ptr;
        ptr += 5;

        for(start = ptr += 1;*ptr && (isalnum(*ptr) || *ptr == '.');ptr++);

        if(*ptr == ';') {
          id = strndup(start-1,ptr-start+1);

          for(start=ptr+=1;*ptr && !isspace(*ptr) && *ptr != ']';ptr++);

          if(*ptr == ']') {
            uri = strndup(start,ptr-start);

            for(i=0;i<ref_uris.elements;i++) {
              suri = array_element_at(&ref_uris,i);

              if(cf_strcmp(suri->id,id) == 0) {
                found = 1;
                str_chars_append(&str,"<a href=\"",9);
                str_chars_append(&str,suri->uri,strlen(suri->uri));
                str_chars_append(&str,uri,strlen(uri));
                str_chars_append(&str,"\">",2);
                str_chars_append(&str,suri->uri,strlen(suri->uri));
                str_chars_append(&str,uri,strlen(uri));
                str_chars_append(&str,"</a>",4);

                break;
              }
            }

            free(uri);
            free(id);

            if(found == 0) {
              ptr = save;
              str_char_append(&str,*ptr);
            }
          }
          else {
            ptr = save;
            free(id);
            str_char_append(&str,*ptr);
          }
        }
        else {
          ptr = save;
          str_char_append(&str,*ptr);
        }
      }
      else str_char_append(&str,*ptr);
    }
    else str_char_append(&str,*ptr);
  }

  thr->messages->content     = str.content;
  thr->messages->content_len = str.len;

  return FLT_DECLINE;
}


int flt_reference_handle_command(t_configfile *cfile,t_conf_opt *opt,u_char **args,int argnum) {
  t_ref_uri uri;

  uri.id  = strdup(args[0]);
  uri.uri = strdup(args[1]);

  if(ref_uris.element_size == 0) array_init(&ref_uris,sizeof(uri),flt_reference_cleanup_entry);

  array_push(&ref_uris,&uri);

  return 0;
}

void flt_reference_cleanup(void) {
}

t_conf_opt flt_reference_config[] = {
  { "ReferenceURI",  flt_reference_handle_command, NULL },
  { NULL, NULL, NULL }
};

t_handler_config flt_reference_handlers[] = {
  { POSTING_HANDLER,   flt_reference_handle_posting },
  { 0, NULL }
};

t_module_config flt_reference = {
  flt_reference_config,
  flt_reference_handlers,
  NULL,
  NULL,
  NULL,
  flt_reference_cleanup
};

/* eof */

