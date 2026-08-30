cmake_minimum_required(VERSION 3.20)

project(msocket LANGUAGES C CXX VERSION 1.0.0)

set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)
set(CMAKE_DISABLE_SOURCE_CHANGES ON)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_OPTIMIZE_DEPENDENCIES ON)

if(MSVC)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall)
endif()

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

option(UNIT_TEST "Unit Test Build" OFF)

set(MSOCKET_SANITIZERS "" CACHE STRING "Comma-separated sanitizers to enable (for example: address,undefined or thread)")
set_property(CACHE MSOCKET_SANITIZERS PROPERTY STRINGS "" "address" "address,undefined" "thread")

if(MSOCKET_SANITIZERS)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        add_compile_options(-fsanitize=${MSOCKET_SANITIZERS} -fno-omit-frame-pointer)
        add_link_options(-fsanitize=${MSOCKET_SANITIZERS})
    else()
        message(FATAL_ERROR "MSOCKET_SANITIZERS is only supported with GCC or Clang")
    endif()
endif()

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

add_library(msocket ${MSOCKET_HEADERS} ${MSOCKET_SOURCES})
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
if (UNIT_TEST)
    target_compile_definitions(msocket_adapter PUBLIC UNIT_TEST)
endif()

###



