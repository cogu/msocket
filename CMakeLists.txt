cmake_minimum_required(VERSION 3.14)

project(msocket LANGUAGES C VERSION 0.10.0)

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

option(UNIT_TEST "Unit Test Build" OFF)

### Library msocket

set (MSOCKET_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/inc/osmacro.h
    ${CMAKE_CURRENT_SOURCE_DIR}/inc/osutil.h
    ${CMAKE_CURRENT_SOURCE_DIR}/inc/msocket.h
    ${CMAKE_CURRENT_SOURCE_DIR}/inc/msocket_adt.h
)

set (MSOCKET_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/msocket.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/msocket_adt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/osutil.c
)

add_library(msocket ${MSOCKET_HEADERS} ${MSOCKET_SOURCES} )
target_link_libraries(msocket PRIVATE Threads::Threads)
target_include_directories(msocket PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc)
###

### Library msocket_server

set (MSOCKET_SERVER_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/inc/msocket_server.h
)

set (MSOCKET_SERVER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/msocket_server.c
)

add_library(msocket_server ${MSOCKET_SERVER_HEADERS} ${MSOCKET_SERVER_SOURCES})
target_link_libraries(msocket_server PRIVATE msocket)
target_include_directories(msocket_server PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc)
###

### Library msocket_testsocket

if (UNIT_TEST)
    set (MSOCKET_TESTSOCKET_HEADERS
        ${CMAKE_CURRENT_SOURCE_DIR}/inc/testsocket_spy.h
        ${CMAKE_CURRENT_SOURCE_DIR}/inc/testsocket.h
    )

    set (MSOCKET_TESTSOCKET_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/testsocket_spy.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/testsocket.c
    )

    add_library(msocket_testsocket ${MSOCKET_TESTSOCKET_HEADERS} ${MSOCKET_TESTSOCKET_SOURCES})
    target_link_libraries(msocket_testsocket PRIVATE msocket)
    target_include_directories(msocket_testsocket PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc)
endif()
###

project(msocket_adapter LANGUAGES CXX VERSION 0.10.0)

### Library cpp_msocket
set (MSOCKET_ADAPTER_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/inc/msocket_adapter.h
)

set (MSOCKET_ADAPTER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/msocket_adapter.cpp
)
add_library(msocket_adapter ${MSOCKET_ADAPTER_HEADERS} ${MSOCKET_ADAPTER_SOURCES})
target_link_libraries(msocket_adapter PRIVATE msocket msocket_server)
target_include_directories(msocket_adapter PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc)
###

