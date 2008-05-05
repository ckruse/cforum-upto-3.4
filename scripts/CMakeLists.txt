set (SCRIPTS cf-cleanup.pl cforum.sh fo_admin fo_userconf fo_usermanagement fo_xmlrpc)

foreach (script ${SCRIPTS})
  configure_file (${CMAKE_CURRENT_SOURCE_DIR}/${script}.in ${CMAKE_CURRENT_BINARY_DIR}/${script} @ONLY)
  install (PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/${script} DESTINATION ${CMAKE_INSTALL_BINDIR})
endforeach (script ${SCRIPTS})

install (
  PROGRAMS
    cf-indextips.pl
    cf-msgs2db.pl
    cf-xmlconv.pl
    cf-compile_templates.pl
  DESTINATION ${CMAKE_INSTALL_BINDIR}
)
