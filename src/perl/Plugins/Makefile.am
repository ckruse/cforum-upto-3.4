SUBDIRS=Management

EXTRA_DIST=HTTPAuth.pm \
	RegisterName.pm \
	ImportExport.pm \
	XMLRPCUserManagement.pm \
	Pathes.pm \
	UserconfDF:Categories.pm \
	Wizard.pm \
	Setvalue.pm

install-data-local:
	if test ! -d $(DESTDIR)$(pkglibdir)/Plugins; then \
		$(MKDIR) -p $(DESTDIR)$(pkglibdir)/Plugins; \
	fi; \
	for file in $(EXTRA_DIST); do \
	  $(INSTALL_DATA) $$file $(DESTDIR)$(pkglibdir)/Plugins/$$file; \
	done


# eof
