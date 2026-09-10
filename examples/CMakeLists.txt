add_executable(echo_server echo/echo_server.c)
target_link_libraries(echo_server PRIVATE msocket)

add_executable(echo_client echo/echo_client.c)
target_link_libraries(echo_client PRIVATE msocket)
