add_executable(utool_tests
  smoke_test.cpp
)

target_link_libraries(utool_tests
  PRIVATE
    utool_lib
)

add_test(NAME utool_smoke COMMAND utool_tests)
