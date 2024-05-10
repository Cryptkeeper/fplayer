cmake_minimum_required(VERSION 3.24)
project(fplayer C)

# use strict C99
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror -Wextra -Wno-unused-value -pedantic")

enable_testing()

# these search paths don't link by default on macOS or FreeBSD
if (APPLE OR (CMAKE_HOST_SYSTEM MATCHES "FreeBSD-*"))
    include_directories(/usr/local/include)
    link_directories(/usr/local/lib)
endif ()

if (APPLE)
    # silence macOS OpenAL deprecation warning messages
    add_compile_definitions(OPENAL_DEPRECATED=)
endif ()

add_subdirectory(dep/liblorproto)

file(GLOB_RECURSE COMMON_FILES "common/*.[ch]")
add_library(common STATIC ${COMMON_FILES})
target_include_directories(common PUBLIC "dep/libtinyfseq" "dep/stb")
target_link_libraries(common PUBLIC lorproto)

option(BUILD_MFTOOL "enable building mftool executable" true)
if (BUILD_MFTOOL)
    file(GLOB_RECURSE MFTOOL_FILES "tool/mftool/*.c")
    add_executable(mftool ${MFTOOL_FILES})
    target_include_directories(mftool PRIVATE common)
    target_link_libraries(mftool common)
endif ()

option(BUILD_GENTOOL "enable building gentool executable" true)
if (BUILD_GENTOOL)
    file(GLOB_RECURSE GENTOOL_FILES "tool/gentool/*.c")
    add_executable(gentool ${GENTOOL_FILES})
    target_include_directories(gentool PRIVATE common)
    target_link_libraries(gentool common)
endif ()

file(GLOB_RECURSE SRC_FILES "src/*.[ch]")
add_executable(fplayer ${SRC_FILES})

# compiling on FreeBSD without explicitly linking libusb (used by libserialport) results in a build error
if (CMAKE_HOST_SYSTEM MATCHES "FreeBSD-*")
    target_link_libraries(fplayer usb)
endif ()

target_include_directories(fplayer PRIVATE common)
target_link_libraries(fplayer m pthread common serialport)

# OpenAL
if (APPLE)
    target_link_libraries(fplayer "-framework OpenAL" alut)
else ()
    target_link_libraries(fplayer openal alut)
endif ()

# zstd
find_package(PkgConfig REQUIRED)
pkg_check_modules(ZSTD REQUIRED libzstd)

target_include_directories(fplayer PRIVATE ${ZSTD_INCLUDE_DIRS})
target_link_directories(fplayer PRIVATE ${ZSTD_LIBRARY_DIRS})
target_link_libraries(fplayer ${ZSTD_LIBRARIES})

target_include_directories(gentool PRIVATE ${ZSTD_INCLUDE_DIRS})
target_link_directories(gentool PRIVATE ${ZSTD_LIBRARY_DIRS})
target_link_libraries(gentool ${ZSTD_LIBRARIES})

# Testing
add_executable(test_dsprintf test/dsprintf.c)
target_include_directories(test_dsprintf PRIVATE common)
target_link_libraries(test_dsprintf common)
add_test(NAME dsprintf COMMAND test_dsprintf)

# Fuzzing (libFuzzer)
if (ENABLE_FUZZING)
    add_executable(fuzz_cmap_parse_csv_line fuzz/cmap_parse_csv_line.c src/cmap.c)
    target_include_directories(fuzz_cmap_parse_csv_line PRIVATE common "src")
    target_link_libraries(fuzz_cmap_parse_csv_line common "-fsanitize=fuzzer")
    target_compile_options(fuzz_cmap_parse_csv_line PRIVATE -fsanitize=fuzzer)

    add_executable(fuzz_cmap_parse_csv fuzz/cmap_parse_csv.c src/cmap.c)
    target_include_directories(fuzz_cmap_parse_csv PRIVATE common "src")
    target_link_libraries(fuzz_cmap_parse_csv common "-fsanitize=fuzzer")
    target_compile_options(fuzz_cmap_parse_csv PRIVATE -fsanitize=fuzzer)
endif ()
