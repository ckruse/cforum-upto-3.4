
SUBDIRS=src cgi-shared cgi-bin

install-exec-hook:
	if test ! -d $(bindir)/user/; then \
	  $(MKDIR) -p $(bindir)/user; \
	fi; \
	for file in $(bindir)/fo_*; do \
	  $(LN) -s $${file} $(bindir)/user/`echo -n $${file}|sed 's!$(bindir)!!'`; \
	done

