
SUFFIXES=.mo

EXTRA_DIST=messages.mo

DIR=@CONFIGDIR@/locale/fr/LC_MESSAGES/

messages.mo:
	msgfmt messages.po

install-data-local: $(EXTRA_DIST)
	if test ! -d $(DIR); then \
		$(MKDIR) -p $(DIR); \
	fi; \
	for file in $(EXTRA_DIST); do \
		$(INSTALL_DATA) $$file $(DIR)/$$file; \
	done

