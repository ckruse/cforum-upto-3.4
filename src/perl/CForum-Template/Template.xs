#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "ppport.h"

#include "const-c.inc"

#include "config.h"
#include "defines.h"
#include "hashlib.h"
#include "utils.h"
#include "template.h"

MODULE = CForum::Template    PACKAGE = CForum::Template

INCLUDE: const-xs.inc
PROTOTYPES: DISABLE

t_cf_template *
new(class,filename)
    const u_char *class
    const u_char *filename
  PREINIT:
    int rc;
    const char *CLASS = class;
    t_cf_template *tpl = fo_alloc(NULL,1,sizeof(t_cf_template),FO_ALLOC_CALLOC);
  CODE:
    if((rc = tpl_cf_init(tpl,filename)) != 0) {
      XSRETURN_UNDEF;
    }

    RETVAL = tpl;
  OUTPUT:
    RETVAL

void set_var(self,name,value,len,escape)
    t_cf_template *self;
    const u_char *name;
    const u_char *value;
    STRLEN len;
    int escape;
  CODE:
    tpl_cf_setvar(self,(u_char *)name,value,len,escape);

const u_char *getVar(self,name)
    t_cf_template *self;
    const u_char *name;
  PREINIT:
    const t_cf_tpl_variable *var;
  CODE:
    if((var = tpl_cf_getvar(self,(u_char *)name)) == NULL) {
      XSRETURN_UNDEF;
    }

    RETVAL = var->data->content;
  OUTPUT:
    RETVAL

void parse(self)
    t_cf_template *self
  CODE:
    tpl_cf_parse(self);

const u_char *parseToMem(self)
    t_cf_template *self
  CODE:
    tpl_cf_parse_to_mem(self);
    RETVAL = self->parsed.content;
    self->parsed.len = 0;
    self->parsed.reserved = 0;
  OUTPUT:
    RETVAL

void append_var(self,name,value,len)
    t_cf_template *self
    const u_char *name
    const u_char *value
    STRLEN len
  CODE:
    tpl_cf_appendvar(self,(u_char *)name,value,len);

void unsetVar(self,name)
    t_cf_template *self
    const u_char *name
  CODE:
    tpl_cf_freevar(self,(u_char *)name);

void cleanup(self)
    t_cf_template *self
  CODE:
    tpl_cf_finish(self);
    free(self);

# eof
