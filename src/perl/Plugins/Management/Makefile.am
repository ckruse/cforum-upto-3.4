EXTRA_DIST=SQL.pm

install-data-local:
	if test ! -d $(libdir)/cforum/Plugins/Management; then \
		$(MKDIR) -p $(libdir)/cforum/Plugins/Management; \
	fi; \
	for file in $(EXTRA_DIST); do \
	  $(INSTALL_DATA) $$file $(libdir)/cforum/Plugins/Management/$$file; \
	done

