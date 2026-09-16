cmake_minimum_required(VERSION 3.20)

project(msocket LANGUAGES C CXX VERSION 2.0.0)

set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)
set(CMAKE_DISABLE_SOURCE_CHANGES ON)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_OPTIMIZE_DEPENDENCIES ON)

if(MSVC)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    add_compile_options(/W4)
    option(ENABLE_MSVC_ANALYZE "Enable MSVC static code analysis (/analyze)" OFF)
    if(ENABLE_MSVC_ANALYZE)
        add_compile_options(/analyze)
    endif()
else()
    add_compile_options(-Wall -Wextra)
endif()

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
        set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build." FORCE)
        set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
    endif()

    option(UNIT_TEST "Unit Test Build" OFF)
    option(LEAK_CHECK "Memory Leak Detection" OFF)
endif()

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

### Dependency: adt
if(NOT TARGET adt)
    set(ADT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../adt" CACHE PATH "Path to adt repository")
    if(EXISTS "${ADT_DIR}/CMakeLists.txt")
        add_subdirectory("${ADT_DIR}" "${CMAKE_BINARY_DIR}/adt" EXCLUDE_FROM_ALL)
    else()
        find_package(adt REQUIRED)
    endif()
endif()

### Library: msocket
set(MSOCKET_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/msocket.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/msocket_error.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/msocket_server.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/testsocket.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/testsocket_spy.h
)

set(MSOCKET_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/source/msocket_common.c
    ${CMAKE_CURRENT_SOURCE_DIR}/source/msocket_error.c
    ${CMAKE_CURRENT_SOURCE_DIR}/source/msocket_server.c
    ${CMAKE_CURRENT_SOURCE_DIR}/source/testsocket.c
    ${CMAKE_CURRENT_SOURCE_DIR}/source/testsocket_spy.c
)

if(WIN32)
    list(APPEND MSOCKET_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/source/msocket_windows.c)
    set(PLATFORM_LIBS ws2_32)
else()
    list(APPEND MSOCKET_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/source/msocket_posix.c)
    find_package(Threads REQUIRED)
    set(PLATFORM_LIBS Threads::Threads)
endif()

add_library(msocket ${MSOCKET_SOURCES} ${MSOCKET_HEADERS})
target_link_libraries(msocket PUBLIC adt ${PLATFORM_LIBS})
target_include_directories(msocket
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/source
)

### Library: msocket_adapter (C++)
set(MSOCKET_ADAPTER_HEADERS
    include/msocket_adapter.h
)
set(MSOCKET_ADAPTER_SOURCES
    source/msocket_adapter.cpp
)

add_library(msocket_adapter ${MSOCKET_ADAPTER_SOURCES} ${MSOCKET_ADAPTER_HEADERS})
target_link_libraries(msocket_adapter PUBLIC msocket)
target_include_directories(msocket_adapter
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/source
)

### Tests
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    if(UNIT_TEST)
        enable_testing()
        add_subdirectory(tests)
    endif()

    ### Examples
    option(BUILD_EXAMPLES "Build example programs" OFF)
    if(BUILD_EXAMPLES)
        add_subdirectory(examples)
    endif()
endif()

