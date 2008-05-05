set (CONFIGS fo_admin.conf fo_default.conf fo_server.conf fo_view.conf fo_arcview.conf fo_feeds.conf fo_userconf.conf fo_vote.conf fo_cleanup.conf fo_post.conf fo_usermanagement.conf fo_xmlrpc.conf)

foreach (config ${CONFIGS})
  configure_file (${CMAKE_CURRENT_SOURCE_DIR}/${config}.in ${CMAKE_CURRENT_BINARY_DIR}/${config} @ONLY)
  install (FILES ${CMAKE_CURRENT_BINARY_DIR}/${config} DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/cforum)
endforeach (config ${CONFIGS})

install (
  FILES
    config.de.xml
    config.dtd
    errormessages.de.txt
    errormessages.fr.txt
    modconf.dtd
    modules.de.xml
    modules.en.xml
    modules.fr.xml
    quotings.xml
    tipoftheday.txt
  DESTINATION
    ${CMAKE_INSTALL_SYSCONFDIR}/cforum
)

install (
  DIRECTORY
    patterns
  DESTINATION
    ${CMAKE_INSTALL_SYSCONFDIR}/cforum
)
