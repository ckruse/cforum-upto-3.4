include_directories (BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../.. ${CMAKE_CURRENT_SOURCE_DIR})

include (${SWIG_USE_FILE})

set (CMAKE_SWIG_FLAGS "")

set_source_files_properties (CForum-Template.i PROPERTIES SWIG_FLAGS "-shadow")
swig_add_module (CForum-Template perl CForum-Template.i)
swig_link_libraries (CForum-Template ${PERL_LIBRARIES} cftemplate cfhashlib cfutils)
set_target_properties (CForum-Template PROPERTIES OUTPUT_NAME "Template")

find_program (CAT_EXECUTABLE cat)

add_custom_command (
  TARGET CForum-Template POST_BUILD
  COMMAND
  ${CAT_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/Template.pm.part >> ${CMAKE_CURRENT_BINARY_DIR}/Template.pm
)

install (TARGETS CForum-Template LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/cforum/auto/CForum/Template)
install (FILES ${CMAKE_CURRENT_BINARY_DIR}/Template.pm DESTINATION ${CMAKE_INSTALL_LIBDIR}/cforum/CForum)
