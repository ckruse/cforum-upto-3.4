EXTRA_DIST=.htaccess

install-data-local:
	if ! -d $(DOCROOT)/archiv; then \
		$(MKDIR) -p $(DOCROOT)/archiv; \
	fi; \
	for file in $(EXTRA_DIST); do \
	  $(INSTALL_DATA) $$file $(DOCROOT)/archiv/$$file; \
	done


