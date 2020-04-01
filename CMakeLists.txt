cmake_minimum_required(VERSION 3.14)

project(msocket LANGUAGES C VERSION 1.0)

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    add_subdirectory(../adt ${CMAKE_CURRENT_BINARY_DIR}/adt)
endif()

### Library msocket

set (OS_SUPPORT_HEADER_LIST
    inc/osmacro.h
)

set (MSOCKET_HEADER_LIST
    inc/msocket.h
)

set (MSOCKET_SOURCE_LIST
    src/msocket.c
)

add_library(msocket ${MSOCKET_SOURCE_LIST} ${MSOCKET_HEADER_LIST} ${OS_SUPPORT_HEADER_LIST})
target_link_libraries(msocket PRIVATE adt)
target_include_directories(msocket PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc)
###

### Library msocket_server

set (MSOCKET_SERVER_HEADER_LIST
    inc/msocket_server.h
)

set (MSOCKET_SERVER_SOURCE_LIST
    src/msocket_server.c
)

add_library(msocket_server ${MSOCKET_SERVER_SOURCE_LIST} ${MSOCKET_SERVER_HEADER_LIST})
target_link_libraries(msocket_server PRIVATE adt msocket)
target_include_directories(msocket_server PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc)
###

project(msocket_wrapper LANGUAGES CXX VERSION 1.0)

### Library cpp_msocket
set (MSOCKET_WRAPPER_HEADER_LIST
    inc/MSocketWrapper.hpp
)

set (MSOCKET_WRAPPER_SOURCE_LIST
    src/MSocketWrapper.cpp
)

add_library(cpp_msocket ${MSOCKET_WRAPPER_SOURCE_LIST} ${MSOCKET_WRAPPER_HEADER_LIST})
target_link_libraries(cpp_msocket PRIVATE adt msocket)
#set_target_properties(cpp_msocket PROPERTIES LINKER_LANGUAGE CXX)
target_include_directories(cpp_msocket PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc)
###

### Library cpp_msocket_server
set (MSOCKET_SERVER_WRAPPER_HEADER_LIST
    inc/MSocketServer.hpp
)

set (MSOCKET_SERVER_WRAPPER_SOURCE_LIST
    src/MSocketServer.cpp
)

add_library(cpp_msocket_server ${MSOCKET_SERVER_WRAPPER_SOURCE_LIST} ${MSOCKET_SERVER_WRAPPER_HEADER_LIST})
target_link_libraries(cpp_msocket_server PRIVATE adt cpp_msocket)
#set_target_properties(cpp_msocket_server PROPERTIES LINKER_LANGUAGE CXX)
target_include_directories(cpp_msocket_server PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc)
###

