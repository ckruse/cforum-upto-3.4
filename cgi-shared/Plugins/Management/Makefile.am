EXTRA_DIST=SQL.pm

install-data-local:
	if test ! -d $(PERLMODULES)/Plugins/Management; then \
		$(MKDIR) -p $(PERLMODULES)/Plugins/Management; \
	fi; \
	for file in $(EXTRA_DIST); do \
	  $(INSTALL_DATA) $$file $(PERLMODULES)/Plugins/Management/$$file; \
	done

