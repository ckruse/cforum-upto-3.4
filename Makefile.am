
SUBDIRS=src cgi-shared cgi-bin htdocs

install-exec-hook:
	if test ! -d $(bindir)/user/; then \
	  $(MKDIR) -p $(bindir)/user; \
	fi; \
	for file in $(bindir)/fo_*; do \
	  $(LN) -sf $${file} $(bindir)/user/`echo -n $${file}|sed 's!$(bindir)!!'`; \
	done

