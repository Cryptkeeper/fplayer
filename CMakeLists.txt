cmake_minimum_required(VERSION 3.24)
project(fplayer C)

# use strict C99
set(CMAKE_C_STANDARD 99)
set(PEDANTIC_COMPILER_FLAGS "-Wall -Werror -Wextra -Wno-unused-value -pedantic")

enable_testing()

if (DEFINED CMAKE_PREFIX_PATH)
    message(STATUS "CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH}")
    include_directories(${CMAKE_PREFIX_PATH}/include)
    link_directories(${CMAKE_PREFIX_PATH}/lib)
endif ()

if (APPLE)
    # silence macOS OpenAL deprecation warning messages
    add_compile_definitions(OPENAL_DEPRECATED=)
endif ()

add_subdirectory(dep/liblorproto)

file(GLOB_RECURSE COMMON_FILES "common/*.[ch]")
set_source_files_properties(${COMMON_FILES} PROPERTIES COMPILE_FLAGS ${PEDANTIC_COMPILER_FLAGS})
add_library(common STATIC ${COMMON_FILES})
target_include_directories(common PUBLIC "dep/libtinyfseq" "dep/stb")
target_link_libraries(common PUBLIC lorproto)

option(BUILD_MFTOOL "enable building mftool executable" true)
if (BUILD_MFTOOL)
    file(GLOB_RECURSE MFTOOL_FILES "tool/mftool/*.c")
    set_source_files_properties(${MFTOOL_FILES} PROPERTIES COMPILE_FLAGS ${PEDANTIC_COMPILER_FLAGS})
    add_executable(mftool ${MFTOOL_FILES})
    target_include_directories(mftool PRIVATE common)
    target_link_libraries(mftool common)
endif ()

option(BUILD_GENTOOL "enable building gentool executable" true)
if (BUILD_GENTOOL)
    file(GLOB_RECURSE GENTOOL_FILES "tool/gentool/*.c")
    set_source_files_properties(${GENTOOL_FILES} PROPERTIES COMPILE_FLAGS ${PEDANTIC_COMPILER_FLAGS})
    add_executable(gentool ${GENTOOL_FILES})
    target_include_directories(gentool PRIVATE common)
    target_link_libraries(gentool common)
endif ()

file(GLOB_RECURSE SRC_FILES "src/*.[ch]")
set_source_files_properties(${SRC_FILES} PROPERTIES COMPILE_FLAGS ${PEDANTIC_COMPILER_FLAGS})
add_executable(fplayer ${SRC_FILES})

# compiling on FreeBSD without explicitly linking libusb (used by libserialport) results in a build error
if (CMAKE_HOST_SYSTEM MATCHES "FreeBSD-*")
    target_link_libraries(fplayer usb)
endif ()

target_include_directories(fplayer PRIVATE common)
target_link_libraries(fplayer pthread common serialport)

# cJSON
set(CJSON_BUILD_SHARED_LIBS OFF)
set(CJSON_OVERRIDE_BUILD_SHARED_LIBS ON)
set(ENABLE_CJSON_TEST OFF)
add_subdirectory(dep/cJSON)

target_include_directories(fplayer PRIVATE "dep/cJSON")
target_link_libraries(fplayer cjson)

# OpenAL
if (APPLE)
    target_link_libraries(fplayer "-framework OpenAL" alut)
else ()
    target_link_libraries(fplayer openal alut)
endif ()

# zstd
target_link_libraries(fplayer zstd)
target_link_libraries(gentool zstd)

# Testing
add_executable(test_dsprintf test/dsprintf.c)
target_include_directories(test_dsprintf PRIVATE common)
target_link_libraries(test_dsprintf common)
add_test(NAME dsprintf COMMAND test_dsprintf)
