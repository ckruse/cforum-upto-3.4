SUBDIRS=Management

EXTRA_DIST=CategoryCheck.pm \
  PostOkLink.pm \
  FixedLineWidth.pm \
  MarkOwnVisited.pm \
  Preview.pm \
  RandomSignature.pm \
  SpellCheck.pm \
  CheckRegisteredName.pm \
  HTTPAuth.pm \
  PostingAssistant.pm \
  Quotings.pm \
  RegisterName.pm \
  Variables.pm

install-data-local:
	if test ! -d $(PERLMODULES)/Plugins; then \
		$(MKDIR) -p $(PERLMODULES)/Plugins; \
	fi; \
	for file in $(EXTRA_DIST); do \
	  $(INSTALL_DATA) $$file $(PERLMODULES)/Plugins/$$file; \
	done


# eof
