
SUFFIXES=.lex

.lex.c:
	flex -Pparsetpl -o$@ $<

bin_PROGRAMS= \
  parsetpl


parsetpl_SOURCES=parsetpl.c
parsetpl_LDADD=../libcfutils.la
parsetpl_LDFLAGS=-export-dynamic

# eof
