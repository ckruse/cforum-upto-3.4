SUBDIRS=admin

EXTRA_DIST=fo_post_old \
  fo_userconf \
  fo_usermanagement \
	fo_cleanup \
  .htaccess


install-exec-local:
	if test ! -d $(bindir); then \
		$(MKDIR) -p $(bindir); \
	fi; \
	for file in $(EXTRA_DIST); do \
    if test "$${file}" != ".htaccess"; then \
		  $(INSTALL_PROGRAM) $$file $(bindir)/$$file; \
		else \
			$(INSTALL_DATA) $$file $(bindir)/$$file; \
		fi; \
	done


