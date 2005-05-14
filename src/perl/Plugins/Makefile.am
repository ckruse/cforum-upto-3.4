SUBDIRS=Management

EXTRA_DIST=HTTPAuth.pm \
  RegisterName.pm \
	ImportExport.pm \
  XMLRPCUserManagement.pm \
	Pathes.pm \
	UserconfCategories.pm

install-data-local:
	if test ! -d $(libdir)/cforum/Plugins; then \
		$(MKDIR) -p $(libdir)/cforum/Plugins; \
	fi; \
	for file in $(EXTRA_DIST); do \
	  $(INSTALL_DATA) $$file $(libdir)/cforum/Plugins/$$file; \
	done


# eof
