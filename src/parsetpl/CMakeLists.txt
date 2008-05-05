add_custom_command (
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/parsetpl.c
  COMMAND ${FLEX_EXECUTABLE}
  ARGS
    -o${CMAKE_CURRENT_BINARY_DIR}/parsetpl.c
    -Pparsetpl
    ${CMAKE_CURRENT_SOURCE_DIR}/parsetpl.lex
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/parsetpl.lex
)

include_directories (BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_SOURCE_DIR})

add_executable (cf-parsetpl parsetpl.c dereferencors.c helpers.c peeks.c processors.c writers.c)
target_link_libraries (cf-parsetpl cfutils)
set_target_properties (cf-parsetpl PROPERTIES LINK_FLAGS "-export-dynamic")

install (
  TARGETS
    cf-parsetpl
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
