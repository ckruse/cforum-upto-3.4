
SUBDIRS=Plugins

EXTRA_DIST=ForumUtils.pm

install-data-local:
	if test ! -d $(PERLMODULES); then \
		$(MKDIR) -p $(PERLMODULES); \
	fi; \
	for file in $(EXTRA_DIST); do \
	  $(INSTALL_DATA) $$file $(PERLMODULES)/$$file; \
	done
