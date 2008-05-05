include_directories (BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../.. ${CMAKE_CURRENT_SOURCE_DIR})

include (${SWIG_USE_FILE})

set (CMAKE_SWIG_FLAGS "")

set (_exports_list
  get_conf_file

  CFG_OPT_NEEDED
  CFG_OPT_CONFIG
  CFG_OPT_USER
  CFG_OPT_UNIQUE
  CFG_OPT_GLOBAL
  CFG_OPT_LOCAL
  CFG_OPT_NOOVERRIDE

  CFG_OPT_SEEN

  CFG_MODE_CONFIG
  CFG_MODE_USER
  CFG_MODE_NOLOAD

  fo_default_conf
  fo_server_conf
  fo_view_conf
  fo_arcview_conf
  fo_post_conf
  fo_vote_conf

  default_options
  fo_view_options
  fo_post_options
  fo_server_options
  fo_arcview_options
  fo_vote_options
)

set_source_files_properties (CForum-Configparser.i PROPERTIES SWIG_FLAGS "-shadow")
swig_add_module (CForum-Configparser perl CForum-Configparser.i)
swig_link_libraries (CForum-Configparser ${PERL_LIBRARIES} cfconfigparser cfhashlib cfutils)
set_target_properties (CForum-Configparser PROPERTIES OUTPUT_NAME "Configparser")
add_custom_command (
  TARGET CForum-Configparser POST_BUILD
  COMMAND
  ${ADJUST_EXPORTS_EXECUTABLE} ${PERL_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Configparser.pm ${_exports_list}
)

install (TARGETS CForum-Configparser LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/cforum/auto/CForum/Configparser)
install (FILES ${CMAKE_CURRENT_BINARY_DIR}/Configparser.pm DESTINATION ${CMAKE_INSTALL_LIBDIR}/cforum/CForum)
