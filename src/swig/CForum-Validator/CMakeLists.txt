include_directories (BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../.. ${CMAKE_CURRENT_SOURCE_DIR})

include (${SWIG_USE_FILE})

set (CMAKE_SWIG_FLAGS "")

set (_exports_list
  is_valid_link
  is_valid_http_link
  is_valid_mailaddress
  is_valid_mailto_link
  is_valid_hostname
  is_valid_telnet_link
  is_valid_nntp_link
  is_valid_news_link
  is_valid_ftp_link
  is_valid_prospero_link
  is_valid_wais_link
  is_valid_gopher_link
  is_valid_file_link
)

set_source_files_properties (CForum-Validator.i PROPERTIES SWIG_FLAGS "-shadow")
swig_add_module (CForum-Validator perl CForum-Validator.i)
swig_link_libraries (CForum-Validator ${PERL_LIBRARIES} cfvalidate cfutils)
set_target_properties (CForum-Validator PROPERTIES OUTPUT_NAME "Validator")
add_custom_command (
  TARGET CForum-Validator POST_BUILD
  COMMAND
  ${ADJUST_EXPORTS_EXECUTABLE} ${PERL_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Validator.pm ${_exports_list}
)

install (TARGETS CForum-Validator LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/cforum/auto/CForum/Validator)
install (FILES ${CMAKE_CURRENT_BINARY_DIR}/Validator.pm DESTINATION ${CMAKE_INSTALL_LIBDIR}/cforum/CForum)
