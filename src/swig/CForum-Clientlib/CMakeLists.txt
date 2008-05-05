include_directories (BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../.. ${CMAKE_CURRENT_SOURCE_DIR})

include (${SWIG_USE_FILE})

set (CMAKE_SWIG_FLAGS "")

set (_exports_list
  get_global_value
  cf_get_shm_ptr
  cf_reget_shm_ptr
  cf_get_uconf_name
  cf_socket_setup
  cf_pl_tpl_name
  cf_set_variable
  cf_error_message
  cf_get_error_message
  cf_pl_get_link
  cf_pl_advanced_get_link
  cf_pl_general_get_time
  cf_pl_get_message_through_sock
  cf_pl_get_next_thread_through_sock
  cf_pl_get_message_through_shm
  charset_convert
  htmlentities
  utf8_to_unicode
  htmlentities_charset_convert
  charset_convert_entities
  is_valid_utf8_string
  htmlentities_decode
  cf_has_shm
  new_rlinet
)

set_source_files_properties (CForum-Clientlib.i PROPERTIES SWIG_FLAGS "-shadow")
swig_add_module (CForum-Clientlib perl CForum-Clientlib.i)
swig_link_libraries (CForum-Clientlib ${PERL_LIBRARIES} cfclientlib cfhashlib cfutils)
set_target_properties (CForum-Clientlib PROPERTIES OUTPUT_NAME "Clientlib")
add_custom_command (
  TARGET CForum-Clientlib POST_BUILD
  COMMAND
  ${ADJUST_EXPORTS_EXECUTABLE} ${PERL_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Clientlib.pm ${_exports_list}
)

install (TARGETS CForum-Clientlib LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/cforum/auto/CForum/Clientlib)
install (FILES ${CMAKE_CURRENT_BINARY_DIR}/Clientlib.pm DESTINATION ${CMAKE_INSTALL_LIBDIR}/cforum/CForum)
