
EXTRA_DIST=fo_xmlrpc


install-exec-local:
	if test ! -d $(bindir)/admin; then \
		$(MKDIR) -p $(bindir)/admin; \
	fi; \
	for file in $(EXTRA_DIST); do \
    if test "$${file}" != ".htaccess"; then \
		  $(INSTALL_PROGRAM) $$file $(bindir)/admin/$$file; \
		else \
			$(INSTALL_DATA) $$file $(bindir)/admin/$$file; \
		fi; \
	done


