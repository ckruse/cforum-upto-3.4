
SUFFIXES=.lex

.lex.c:
	flex -Pparsetpl -o$@ $<

bin_PROGRAMS= \
  cf-parsetpl


cf_parsetpl_SOURCES=parsetpl.c dereferencors.c helpers.c peeks.c processors.c writers.c
cf_parsetpl_LDADD=../libcfutils.la
cf_parsetpl_LDFLAGS=-export-dynamic

# eof
