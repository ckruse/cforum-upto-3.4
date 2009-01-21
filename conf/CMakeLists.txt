set (CONFIGS fo_admin.cfcl fo_cleanup.cfcl fo_feeds.cfcl fo_server.cfcl
  fo_usermanagement.cfcl  fo_vote.cfcl fo_arcview.cfcl fo_default.cfcl
  fo_post.cfcl fo_userconf.cfcl fo_view.cfcl fo_xmlrpc.cfcl)



foreach (config ${CONFIGS})
  configure_file (${CMAKE_CURRENT_SOURCE_DIR}/${config}.in ${CMAKE_CURRENT_BINARY_DIR}/${config} @ONLY)
  install (FILES ${CMAKE_CURRENT_BINARY_DIR}/${config} DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/cforum/dist)
endforeach (config ${CONFIGS})



install (
  FILES
    config.dtd
    modconf.dtd
  DESTINATION
    ${CMAKE_INSTALL_SYSCONFDIR}/cforum
)


install (
  FILES
    config.de.xml
    errormessages.de.txt
    errormessages.fr.txt
    modules.de.xml
    modules.en.xml
    modules.fr.xml
    quotings.xml
    tipoftheday.txt
  DESTINATION
    ${CMAKE_INSTALL_SYSCONFDIR}/cforum/dist
)

install (
  DIRECTORY
    patterns
  DESTINATION
    ${CMAKE_INSTALL_SYSCONFDIR}/cforum
)
