
SUFFIXES=.lex

.lex.c:
	flex -Pparsetpl -o$@ $<

bin_PROGRAMS= \
  cf-parsetpl


cf_parsetpl_SOURCES=parsetpl.c
cf_parsetpl_LDADD=../libcfutils.la
cf_parsetpl_LDFLAGS=-export-dynamic

# eof
