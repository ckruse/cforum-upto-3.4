
SUBDIRS=Plugins

EXTRA_DIST=ForumUtils.pm

install-data-local:
	for file in $(EXTRA_DIST); do \
	  $(INSTALL_DATA) $$file $(libdir)/cforum/$$file; \
	done
