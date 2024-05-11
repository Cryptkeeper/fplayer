cmake_minimum_required(VERSION 3.24)
project(fplayer C)

# use strict C99
set(CMAKE_C_STANDARD 99)
set(PEDANTIC_COMPILER_FLAGS "-Wall -Werror -Wextra -Wno-unused-value -pedantic")

if (DEFINED CMAKE_PREFIX_PATH)
    message(STATUS "CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH}")
    include_directories(${CMAKE_PREFIX_PATH}/include)
    link_directories(${CMAKE_PREFIX_PATH}/lib)
endif ()

if (APPLE)
    # silence macOS OpenAL deprecation warning messages
    add_compile_definitions(OPENAL_DEPRECATED=)
endif ()

# liblorproto dependency (include tests)
enable_testing()
add_subdirectory(dep/liblorproto)

file(GLOB_RECURSE COMMON_FILES "common/*.[ch]")
set_source_files_properties(${COMMON_FILES} PROPERTIES COMPILE_FLAGS ${PEDANTIC_COMPILER_FLAGS})
add_library(common STATIC ${COMMON_FILES})
target_include_directories(common PUBLIC "dep/libtinyfseq" "dep/stb")
target_link_libraries(common PUBLIC lorproto)

# mftool executable
file(GLOB_RECURSE MFTOOL_FILES "tool/mftool/*.c")
set_source_files_properties(${MFTOOL_FILES} PROPERTIES COMPILE_FLAGS ${PEDANTIC_COMPILER_FLAGS})
add_executable(mftool ${MFTOOL_FILES})
target_include_directories(mftool PRIVATE common)
target_link_libraries(mftool common)

# gentool executable
file(GLOB_RECURSE GENTOOL_FILES "tool/gentool/*.c")
set_source_files_properties(${GENTOOL_FILES} PROPERTIES COMPILE_FLAGS ${PEDANTIC_COMPILER_FLAGS})
add_executable(gentool ${GENTOOL_FILES})
target_include_directories(gentool PRIVATE common)
target_link_libraries(gentool common zstd)

# fplayer executable
file(GLOB_RECURSE SRC_FILES "src/*.[ch]")
set_source_files_properties(${SRC_FILES} PROPERTIES COMPILE_FLAGS ${PEDANTIC_COMPILER_FLAGS})
add_executable(fplayer ${SRC_FILES})

# compiling on FreeBSD without explicitly linking libusb (used by libserialport) results in a build error
if (CMAKE_HOST_SYSTEM MATCHES "FreeBSD-*")
    target_link_libraries(fplayer usb)
endif ()

target_include_directories(fplayer PRIVATE common "dep/cJSON")
target_link_libraries(fplayer pthread common serialport cjson zstd)

# OpenAL
if (APPLE)
    target_link_libraries(fplayer "-framework OpenAL" alut)
else ()
    target_link_libraries(fplayer openal alut)
endif ()

# Testing
add_executable(test_dsprintf test/dsprintf.c)
target_include_directories(test_dsprintf PRIVATE common)
target_link_libraries(test_dsprintf common)
add_test(NAME dsprintf COMMAND test_dsprintf)
