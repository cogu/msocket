cmake_minimum_required(VERSION 3.14)

project(msocket LANGUAGES C VERSION 0.9.0)

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    include(../adt/cmake/UnitTest.cmake)
    add_subdirectory(../adt ${CMAKE_CURRENT_BINARY_DIR}/adt)
endif()

### Library msocket

set (MSOCKET_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/inc/osmacro.h
    ${CMAKE_CURRENT_SOURCE_DIR}/inc/osutil.h
    ${CMAKE_CURRENT_SOURCE_DIR}/inc/msocket.h
)

set (MSOCKET_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/msocket.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/osutil.c
)

add_library(msocket ${MSOCKET_HEADERS} ${MSOCKET_SOURCES} )
target_link_libraries(msocket PRIVATE adt Threads::Threads)
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
target_link_libraries(msocket_server PRIVATE adt msocket)
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
    target_link_libraries(msocket_testsocket PRIVATE adt msocket)
    target_include_directories(msocket_testsocket PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc)
endif()
###

project(msocket_wrapper LANGUAGES CXX VERSION 0.9.0)

### Library cpp_msocket
set (MSOCKET_WRAPPER_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/inc/MSocketWrapper.hpp
)

set (MSOCKET_WRAPPER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/MSocketWrapper.cpp
)

add_library(cpp_msocket ${MSOCKET_WRAPPER_HEADERS} ${MSOCKET_WRAPPER_SOURCES})
target_link_libraries(cpp_msocket PRIVATE adt msocket)
target_include_directories(cpp_msocket PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc)
###

### Library cpp_msocket_server
set (MSOCKET_SERVER_WRAPPER_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/inc/MSocketServer.hpp
)

set (MSOCKET_SERVER_WRAPPER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/MSocketServer.cpp
)

add_library(cpp_msocket_server ${MSOCKET_SERVER_WRAPPER_HEADERS} ${MSOCKET_SERVER_WRAPPER_SOURCES})
target_link_libraries(cpp_msocket_server PRIVATE adt cpp_msocket)
target_include_directories(cpp_msocket_server PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc)
###

