
SUBDIRS=config Plugins docs data

EXTRA_DIST=CheckRFC.pm ForumUtils.pm

install-data-local:
	if test ! -d $(PERLMODULES); then \
		$(MKDIR) -p $(PERLMODULES); \
	fi; \
	for file in $(EXTRA_DIST); do \
	  $(INSTALL_DATA) $$file $(PERLMODULES)/$$file; \
	done

# eof
