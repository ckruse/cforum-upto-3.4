
EXTRA_DIST=messages.po

DIR=@CONFIGDIR@/locale/en/LC_MESSAGES/

install-data-local:
	for file in $(EXTRA_DIST); do \
		msgfmt $${file}; \
	done; \
	if test ! -d $(DIR); then \
		$(MKDIR) -p $(DIR); \
	fi; \
	for file in $(EXTRA_DIST); do \
		$(INSTALL_DATA) $$file $(DIR)/$$file; \
	done

