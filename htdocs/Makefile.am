SUBDIRS=my archiv

EXTRA_DIST=.htaccess live_syntax.html scoring.html

install-data-local:
	if test ! -d $(DOCROOT); then \
		$(MKDIR) -p $(DOCROOT); \
	fi; \
	for file in $(EXTRA_DIST); do \
	  $(INSTALL_DATA) $$file $(DOCROOT)/$$file; \
	done


