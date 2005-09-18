EXTRA_DIST=SQL.pm

install-data-local:
	if test ! -d $(DESTDIR)$(pkglibdir)/Plugins/Management; then \
		$(MKDIR) -p $(DESTDIR)$(pkglibdir)/Plugins/Management; \
	fi; \
	for file in $(EXTRA_DIST); do \
	  $(INSTALL_DATA) $$file $(DESTDIR)$(pkglibdir)/Plugins/Management/$$file; \
	done

