SUBDIRS=Management

EXTRA_DIST=HTTPAuth.pm \
  RegisterName.pm \
	ImportExport.pm \
  XMLRPCUserManagement.pm

install-data-local:
	if test ! -d $(PERLMODULES)/Plugins; then \
		$(MKDIR) -p $(PERLMODULES)/Plugins; \
	fi; \
	for file in $(EXTRA_DIST); do \
	  $(INSTALL_DATA) $$file $(PERLMODULES)/Plugins/$$file; \
	done


# eof
