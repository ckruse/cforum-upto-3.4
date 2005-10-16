/*
 * Typemaps used in more than one module
 */

typedef unsigned char u_char;

%typemap(in) u_char ** {
  AV *tempav;
  I32 len;
  int i;
  SV  **tv;
  if(!SvROK($input)) croak("Argument $argnum is not a reference.");
  if(SvTYPE(SvRV($input)) != SVt_PVAV) croak("Argument $argnum is not an array.");

  tempav = (AV*)SvRV($input);
  len = av_len(tempav);
  $1 = (u_char **)cf_alloc(NULL,len+2,sizeof(u_char *),CF_ALLOC_MALLOC);
  for(i=0;i<=len;++i) {
    tv = av_fetch(tempav, i, 0);
    $1[i] = (u_char *) SvPV(*tv,PL_na);
  }

  $1[i] = NULL;
};

// This cleans up the char ** array after the function call
%typemap(freearg) u_char ** {
  free($1);
}

// Creates a new Perl array and places a NULL-terminated char ** into it
%typemap(out) u_char ** {
  AV *myav;
  SV **svs;
  int i = 0,len = 0;
  /* Figure out how many elements we have */
  while($1[len]) len++;
  svs = (SV **)cf_alloc(NULL,len,sizeof(SV *),CF_ALLOC_MALLOC);
  for (i = 0; i < len ; i++) {
    svs[i] = sv_newmortal();
    sv_setpv((SV*)svs[i],$1[i]);
  };
  myav = av_make(len,svs);
  free(svs);
  $result = newRV((SV*)myav);
  sv_2mortal($result);
  argvi++;
}

%typemap(perl5,in) SV *cllbck {
  if(!SvROK($input)) croak("Expected a reference.\n");
  $1 = SvRV($input);
}

/* eof */
