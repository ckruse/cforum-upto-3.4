
EXTRA_DIST=fo_arcview.conf \
  fo_default.conf \
  fo_post.conf \
  fo_server.conf \
  fo_userconf.conf \
  fo_view.conf \
  errormessages.txt \
	tipoftheday.txt \
  modules.xml


install-data-local:
	if ! -d $(CONFIGDIR); then \
		$(MKDIR) -p $(CONFIGDIR); \
	fi; \
	for file in $(EXTRA_DIST); do \
	  if test ! -f $(CONFIGDIR)/$$file; then \
		  $(INSTALL_DATA) $$file $(CONFIGDIR)/$$file; \
		fi; \
	done

# eof
