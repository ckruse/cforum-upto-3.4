EXTRA_DIST=html.pat perl.pat php.pat

install-data-local:
	if test ! -d $(sysconfdir)/cforum/patterns; then \
		$(MKDIR) -p $(sysconfdir)/cforum/patterns; \
	fi; \
	for file in $(EXTRA_DIST); do \
		$(INSTALL_DATA) $$file $(sysconfdir)/cforum/patterns/$$file; \
	done

# eof
