
EXTRA_DIST=forum.xml

install-data-local:
	for file in $(EXTRA_DIST); do \
	  if test ! -f $(with_message_dir)/$${file}; then \
			$(INSTALL) $${file} $(with_message_dir)/$${file}; \
		fi; \
	done

