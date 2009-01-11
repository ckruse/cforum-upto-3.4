include_directories (BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_SOURCE_DIR})

add_library (flt_admin MODULE flt_admin.c)
add_library (flt_cftp MODULE flt_cftp.c)
add_library (flt_httpauth MODULE flt_httpauth.c)
add_library (flt_posting MODULE flt_posting.c)
target_link_libraries (flt_posting cfvalidate)
add_library (flt_list MODULE flt_list.c)
add_library (flt_nested MODULE flt_nested.c)
add_library (flt_tid_index MODULE flt_tid_index.c)
target_link_libraries (flt_tid_index ${BERKELEY_DB_LIBRARIES})
add_library (flt_failsafe MODULE flt_failsafe.c)
add_library (flt_directives MODULE flt_directives.c)
target_link_libraries (flt_directives cfvalidate ${PCRE_LIBRARIES})
add_library (flt_xmlstorage MODULE flt_xmlstorage.c ../xml_handling.c)
target_link_libraries (flt_xmlstorage ${GDOME_LIBRARIES})
add_library (flt_categorycheck MODULE flt_categorycheck.c)
add_library (flt_xmlarc MODULE flt_xmlarc.c ../xml_handling)
target_link_libraries (flt_xmlarc ${GDOME_LIBRARIES})
add_library (flt_http MODULE flt_http.c)

set_target_properties (
    flt_admin
    flt_cftp
    flt_httpauth
    flt_posting
    flt_list
    flt_nested
    flt_tid_index
    flt_failsafe
    flt_directives
    flt_xmlstorage
    flt_categorycheck
    flt_xmlarc
    flt_http
  PROPERTIES
    LINK_FLAGS "-export-dynamic -avoid-version"
    PREFIX ""
)

install (
  TARGETS
    flt_admin
    flt_cftp
    flt_httpauth
    flt_posting
    flt_list
    flt_nested
    flt_tid_index
    flt_failsafe
    flt_directives
    flt_xmlstorage
    flt_categorycheck
    flt_xmlarc
    flt_http
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/cforum
)

# Optional modules
set (ALL_MODULES flt_basic flt_category flt_deleted flt_frameset flt_http flt_link flt_listhighlight flt_livefilter flt_openclose flt_include flt_registerednames flt_tplmode flt_urlrewrite flt_visited flt_extern flt_threadreturnanchor flt_tipoftheday flt_postingassistant flt_remotesignature flt_preview flt_spellcheck flt_handle404 flt_voting flt_scoring flt_motd flt_votingvariables flt_sorting flt_latex flt_syntax flt_checkregisteredname flt_mailonpost flt_cgiconfig flt_rot13 flt_gummizelle flt_noanonymous flt_noanswer flt_chooser flt_moderated flt_cookieauth flt_noarchive flt_noarchive_server flt_stdreplacements flt_interesting flt_admin_mysql flt_shortcuts flt_phpsessauth flt_bbcodes flt_pavatar)

SET (EXTRALIBS_flt_deleted ${BERKELEY_DB_LIBRARIES})
SET (EXTRALIBS_flt_registerednames ${BERKELEY_DB_LIBRARIES})
SET (EXTRALIBS_flt_urlrewrite ${PCRE_LIBRARIES})
SET (EXTRALIBS_flt_visited ${BERKELEY_DB_LIBRARIES})
SET (EXTRALIBS_flt_scoring ${BERKELEY_DB_LIBRARIES} ${PCRE_LIBRARIES})
SET (EXTRALIBS_flt_syntax ${PCRE_LIBRARIES})

add_custom_command (
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/flt_scoring.c
  COMMAND ${FLEX_EXECUTABLE}
  ARGS
    -o${CMAKE_CURRENT_BINARY_DIR}/flt_scoring.c
    -Pflt_scoring
    ${CMAKE_CURRENT_SOURCE_DIR}/flt_scoring.lex
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/flt_scoring.lex
)

foreach (module ${ALL_MODULES})
  option (ENABLE_${module} "Enable ${module}" ON)
endforeach (module ${ALL_MODULES})

if (ENABLE_flt_mailonpost STREQUAL "ON")
  find_package (ESMTP REQUIRED)
  include_directories (${ESMTP_INCLUDE_DIR})
  SET (EXTRALIBS_flt_mailonpost ${BERKELEY_DB_LIBRARIES} ${ESMTP_LIBRARIES})
endif (ENABLE_flt_mailonpost STREQUAL "ON")

if (ENABLE_flt_admin_mysql STREQUAL "ON")
  find_package (MySQL REQUIRED)
  include_directories (${MYSQL_INCLUDE_DIR})
  SET (EXTRALIBS_flt_admin_mysql ${MYSQL_LIBRARIES})
endif (ENABLE_flt_admin_mysql STREQUAL "ON")

if (ENABLE_flt_spellcheck STREQUAL "ON")
  find_package (ASPELL REQUIRED)
  include_directories (${ASPELL_INCLUDE_DIR})
  SET (EXTRALIBS_flt_spellcheck ${ASPELL_LIBRARIES})
endif (ENABLE_flt_spellcheck STREQUAL "ON")

if (ENABLE_flt_latex STREQUAL "ON")
  find_package (OpenSSL REQUIRED)
  include_directories (${OPENSSL_INCLUDE_DIR})
  SET (EXTRALIBS_flt_latex ${OPENSSL_LIBRARIES})
endif (ENABLE_flt_latex STREQUAL "ON")

foreach (module ${ALL_MODULES})
  if (ENABLE_${module} STREQUAL "ON")
    message (STATUS "Enabling ${module}")
    add_library (${module} MODULE ${module}.c)
    set_target_properties (${module} PROPERTIES LINK_FLAGS "-export-dynamic -avoid-version" PREFIX "")
    if (EXTRALIBS_${module})
      target_link_libraries (${module} ${EXTRALIBS_${module}})
    endif (EXTRALIBS_${module})
    install (TARGETS ${module} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/cforum)
  endif (ENABLE_${module} STREQUAL "ON")
endforeach (module ${ALL_MODULES})
