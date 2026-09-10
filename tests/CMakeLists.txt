set(TEST_SRCS
    ${ADT_DIR}/test/CuTest.c
    TestMain.c
    testsuite_testsocket.c
    testsuite_msocket_common.c
    testsuite_msocket_loopback.c
    testsuite_msocket_server.c
    testsuite_cpp_socket.cpp
)

add_executable(msocket_unit ${TEST_SRCS})
target_link_libraries(msocket_unit PRIVATE msocket_adapter msocket adt ${PLATFORM_LIBS})
target_include_directories(msocket_unit PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../source
    ${ADT_DIR}/include
    ${ADT_DIR}/test
)

add_test(NAME msocket_unit COMMAND msocket_unit)
set_tests_properties(msocket_unit PROPERTIES
    PASS_REGULAR_EXPRESSION "OK \\([0-9]+ tests\\)"
    LABELS "unit"
)
