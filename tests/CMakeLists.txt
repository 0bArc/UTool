add_executable(utool_tests
  driver.cpp
  Core/config_test.cpp
  Core/env_test.cpp
  Core/game_check_test.cpp
  Core/manifest_test.cpp
  Mod/curve_test.cpp
  Mod/json_test.cpp
  Mod/prepare_test.cpp
  Nexus/match_test.cpp
)

target_include_directories(utool_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(utool_tests PRIVATE utool_lib)

utool_set_warnings(utool_tests)

add_executable(pak_list_parser_test
  pak_list_parser_test.cpp
  ../src/Pak/UnrealPakListParser.cpp
)

target_include_directories(pak_list_parser_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../src
  ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

utool_set_warnings(pak_list_parser_test)

add_test(NAME utool_smoke COMMAND utool_tests)
add_test(NAME utool_pak_list_parser COMMAND pak_list_parser_test
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
