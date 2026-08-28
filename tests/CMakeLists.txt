add_executable(utool_tests
  smoke_test.cpp
)

target_link_libraries(utool_tests
  PRIVATE
    utool_lib
)

add_executable(pak_list_parser_test
  pak_list_parser_test.cpp
  ../src/Pak/UnrealPakListParser.cpp
)

target_include_directories(pak_list_parser_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

add_test(NAME utool_smoke COMMAND utool_tests)
add_test(NAME utool_pak_list_parser COMMAND pak_list_parser_test)
