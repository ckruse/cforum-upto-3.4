
SUFFIXES=.lex

.lex.c:
	flex -Pflt_scoring -o$@ $<

pkglib_LTLIBRARIES=flt_admin.la
flt_admin_la_SOURCES=flt_admin.c
flt_admin_la_LDFLAGS=-module -export-dynamic -avoid-version

pkglib_LTLIBRARIES+=flt_basic.la
flt_basic_la_SOURCES=flt_basic.c
flt_basic_la_LDFLAGS=-module -export-dynamic -avoid-version

pkglib_LTLIBRARIES+=flt_category.la
flt_category_la_SOURCES=flt_category.c
flt_category_la_LDFLAGS=-module -export-dynamic -avoid-version

pkglib_LTLIBRARIES+=flt_cftp.la
flt_cftp_la_SOURCES=flt_cftp.c
flt_cftp_la_LDFLAGS=-module -export-dynamic -avoid-version

pkglib_LTLIBRARIES+=flt_deleted.la
flt_deleted_la_SOURCES=flt_deleted.c
flt_deleted_la_LIBADD=$(BDB_LIB)
flt_deleted_la_LDFLAGS=-module -export-dynamic -avoid-version

pkglib_LTLIBRARIES+=flt_frameset.la
flt_frameset_la_SOURCES=flt_frameset.c
flt_frameset_la_LDFLAGS=-module -export-dynamic -avoid-version

pkglib_LTLIBRARIES+=flt_http.la
flt_http_la_SOURCES=flt_http.c
flt_http_la_LDFLAGS=-module -export-dynamic -avoid-version

pkglib_LTLIBRARIES+=flt_httpauth.la
flt_httpauth_la_SOURCES=flt_httpauth.c
flt_httpauth_la_LDFLAGS=-module -export-dynamic -avoid-version

pkglib_LTLIBRARIES+=flt_link.la
flt_link_la_SOURCES=flt_link.c
flt_link_la_LDFLAGS=-module -export-dynamic -avoid-version

pkglib_LTLIBRARIES+=flt_listhighlight.la
flt_listhighlight_la_SOURCES=flt_listhighlight.c
flt_listhighlight_la_LDFLAGS=-module -export-dynamic -avoid-version

pkglib_LTLIBRARIES+=flt_livefilter.la
flt_livefilter_la_SOURCES=flt_livefilter.c
flt_livefilter_la_LDFLAGS=-module -export-dynamic -avoid-version

pkglib_LTLIBRARIES+=flt_openclose.la
flt_openclose_la_SOURCES=flt_openclose.c
flt_openclose_la_LDFLAGS=-module -export-dynamic -avoid-version

pkglib_LTLIBRARIES+=flt_include.la
flt_include_la_SOURCES=flt_include.c
flt_include_la_LDFLAGS=-module -export-dynamic -avoid-version

pkglib_LTLIBRARIES+=flt_posting.la
flt_posting_la_SOURCES=flt_posting.c
flt_posting_la_LIBADD=../libcfvalidate.la
flt_posting_la_LDFLAGS=-module -export-dynamic -avoid-version

pkglib_LTLIBRARIES+=flt_registerednames.la
flt_registerednames_la_SOURCES=flt_registerednames.c
flt_registerednames_la_LIBADD=$(BDB_LIB)
flt_registerednames_la_LDFLAGS=-module -export-dynamic -avoid-version

pkglib_LTLIBRARIES+=flt_tid_index.la
flt_tid_index_la_SOURCES=flt_tid_index.c
flt_tid_index_la_LIBADD=$(BDB_LIB)
flt_tid_index_la_LDFLAGS=-module -export-dynamic -avoid-version

pkglib_LTLIBRARIES+=flt_tplmode.la
flt_tplmode_la_SOURCES=flt_tplmode.c
flt_tplmode_la_LDFLAGS=-module -export-dynamic -avoid-version

#pkglib_LTLIBRARIES+=flt_urlrewrite.la
#flt_urlrewrite_la_SOURCES=flt_urlrewrite.c
#flt_urlrewrite_la_LDFLAGS=-module -export-dynamic -avoid-version
#flt_urlrewrite_la_LIBADD=$(PCRE_LIBS)

pkglib_LTLIBRARIES+=flt_visited.la
flt_visited_la_SOURCES=flt_visited.c
flt_visited_la_LIBADD=$(BDB_LIB)
flt_visited_la_LDFLAGS=-module -export-dynamic -avoid-version

pkglib_LTLIBRARIES+=flt_extern.la
flt_extern_la_SOURCES=flt_extern.c
flt_extern_la_LDFLAGS=-module -export-dynamic -avoid-version

pkglib_LTLIBRARIES+=flt_threadreturnanchor.la
flt_threadreturnanchor_la_SOURCES=flt_threadreturnanchor.c
flt_threadreturnanchor_la_LDFLAGS=-module -export-dynamic -avoid-version

pkglib_LTLIBRARIES+=flt_tipoftheday.la
flt_tipoftheday_la_SOURCES=flt_tipoftheday.c
flt_tipoftheday_la_LDFLAGS=-module -export-dynamic -avoid-version

pkglib_LTLIBRARIES+=flt_postingassistant.la
flt_postingassistant_la_SOURCES=flt_postingassistant.c
flt_postingassistant_la_LDFLAGS=-module -export-dynamic -avoid-version

pkglib_LTLIBRARIES+=flt_remotesignature.la
flt_remotesignature_la_SOURCES=flt_remotesignature.c
flt_remotesignature_la_LIBADD=$(LIBWWW_LIBS)
flt_remotesignature_la_LDFLAGS=-module -export-dynamic -avoid-version

pkglib_LTLIBRARIES+=flt_preview.la
flt_preview_la_SOURCES=flt_preview.c
flt_preview_la_LDFLAGS=-module -export-dynamic -avoid-version

pkglib_LTLIBRARIES+=flt_handle404.la
flt_handle404_la_SOURCES=flt_handle404.c
flt_handle404_la_LDFLAGS=-module -export-dynamic -avoid-version

pkglib_LTLIBRARIES+=flt_voting.la
flt_voting_la_SOURCES=flt_voting.c
flt_voting_la_LDFLAGS=-module -export-dynamic -avoid-version

pkglib_LTLIBRARIES+=flt_scoring.la
flt_scoring_la_SOURCES=flt_scoring.c
flt_scoring_la_LDFLAGS=-module -export-dynamic -avoid-version
flt_scoring_la_LIBADD=$(PCRE_LIBS)

pkglib_LTLIBRARIES+=flt_motd.la
flt_motd_la_SOURCES=flt_motd.c
flt_motd_la_LDFLAGS=-module -export-dynamic -avoid-version

pkglib_LTLIBRARIES+=flt_failsafe.la
flt_failsafe_la_SOURCES=flt_failsafe.c
flt_failsafe_la_LDFLAGS=-module -export-dynamic -avoid-version

pkglib_LTLIBRARIES+=flt_directives.la
flt_directives_la_SOURCES=flt_directives.c
flt_directives_la_LDFLAGS=-module -export-dynamic -avoid-version
flt_directives_la_LIBADD=../libcfvalidate.la $(PCRE_LIBS)

pkglib_LTLIBRARIES+=flt_votingvariables.la
flt_votingvariables_la_SOURCES=flt_votingvariables.c
flt_votingvariables_la_LDFLAGS=-module -export-dynamic -avoid-version

pkglib_LTLIBRARIES+=flt_xmlstorage.la
flt_xmlstorage_la_SOURCES=flt_xmlstorage.c ../xml_handling.c
flt_xmlstorage_la_LIBADD=$(GDOME_LIBS)
flt_xmlstorage_la_LDFLAGS=-module -export-dynamic -avoid-version


pkglib_LTLIBRARIES+=flt_sorting.la
flt_sorting_la_SOURCES=flt_sorting.c
flt_sorting_la_LDFLAGS=-module -export-dynamic -avoid-version

pkglib_LTLIBRARIES+=flt_categorycheck.la
flt_categorycheck_la_SOURCES=flt_categorycheck.c
flt_categorycheck_la_LDFLAGS=-module -export-dynamic -avoid-version

pkglib_LTLIBRARIES+=flt_latex.la
flt_latex_la_SOURCES=flt_latex.c
flt_latex_la_LDFLAGS=-module -export-dynamic -avoid-version
flt_latex_la_LIBADD=-lssl

pkglib_LTLIBRARIES+=flt_syntax.la
flt_syntax_la_SOURCES=flt_syntax.c
flt_syntax_la_LDFLAGS=-module -export-dynamic -avoid-version
flt_syntax_la_LIBADD=$(PCRE_LIBS)

pkglib_LTLIBRARIES+=flt_checkregisteredname.la
flt_checkregisteredname_la_SOURCES=flt_checkregisteredname.c
flt_checkregisteredname_la_LDFLAGS=-module -export-dynamic -avoid-version

pkglib_LTLIBRARIES+=flt_xmlarc.la
flt_xmlarc_la_SOURCES=flt_xmlarc.c ../xml_handling.c
flt_xmlarc_la_LDFLAGS=-module -export-dynamic -avoid-version
flt_xmlarc_la_LIBADD=$(GDOME_LIBS)

# eof
