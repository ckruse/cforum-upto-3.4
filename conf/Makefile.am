SUBDIRS=patterns

EXTRA_DIST=fo_admin.conf \
	fo_arcview.conf \
  fo_default.conf \
  fo_post.conf \
  fo_server.conf \
  fo_userconf.conf \
  fo_view.conf \
  fo_vote.conf \
  fo_xmlrpc.conf \
	fo_feeds.conf \
  fo_usermanagement.conf \
  errormessages.de.txt \
  errormessages.fr.txt \
	tipoftheday.txt \
  modules.de.xml \
  modules.en.xml \
  modules.fr.xml \
	config.de.xml


install-data-local:
	if test ! -d $(sysconfdir)/cforum; then \
		$(MKDIR) -p $(sysconfdir)/cforum; \
	fi; \
	for file in $(EXTRA_DIST); do \
	  if test ! -f $(sysconfdir)/cforum/$$file; then \
		  $(INSTALL_DATA) $$file $(sysconfdir)/cforum/$$file; \
		fi; \
	done

# eof
