EXTRA_DIST=fo_arcview.conf \
  fo_default.conf \
  fo_post.conf \
  fo_server.conf \
  fo_userconf.conf \
  fo_view.conf \
  fo_vote.conf \
  fo_usermanagement.conf \
  errormessages.de.txt \
  errormessages.fr.txt \
	tipoftheday.txt \
  modules.de.xml \
  modules.en.xml \
  modules.fr.xml


install-data-local:
	if test ! -d $(CONFIGDIR); then \
		$(MKDIR) -p $(CONFIGDIR); \
	fi; \
	for file in $(EXTRA_DIST); do \
	  if test ! -f $(CONFIGDIR)/$$file; then \
		  $(INSTALL_DATA) $$file $(CONFIGDIR)/$$file; \
		fi; \
	done

# eof
