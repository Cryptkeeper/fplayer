cmake_minimum_required(VERSION 3.24)
project(fplayer C)

set(CMAKE_C_STANDARD 99)

enable_testing()

if (APPLE)
    include_directories(/usr/local/include)
    link_directories(/usr/local/lib)

    # silence macOS OpenAL deprecation warning messages
    add_compile_definitions(OPENAL_DEPRECATED=)
endif ()

set(PEDANTIC_COMPILER_FLAGS "-Wall -Werror -Wextra -Wno-unused-value -pedantic")

add_subdirectory(dep/liblorproto)

file(GLOB_RECURSE COMMON_FILES "common/std/*.[ch]")

add_library(common STATIC ${COMMON_FILES})

target_include_directories(common PUBLIC "dep/libtinyfseq" "dep/stb")

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
    set_source_files_properties(${GENTOOL_FILES} PROPERTIES COMPILER_FLAGS ${PEDANTIC_COMPILER_FLAGS})

    add_executable(gentool ${GENTOOL_FILES})

    target_include_directories(gentool PRIVATE common)
    target_link_libraries(gentool common lorproto)
endif ()

file(GLOB_RECURSE SRC_FILES "src/*.[ch]")
set_source_files_properties(${SRC_FILES} PROPERTIES COMPILE_FLAGS ${PEDANTIC_COMPILER_FLAGS})

add_executable(fplayer ${SRC_FILES})

target_include_directories(fplayer PRIVATE common)
target_link_libraries(fplayer common lorproto m serialport)

# OpenAL
target_link_libraries(fplayer alut)

if (APPLE)
    target_link_libraries(fplayer "-framework OpenAL")
else ()
    target_link_libraries(fplayer openal)
endif ()

# zstd
target_link_libraries(fplayer zstd)
target_link_libraries(gentool zstd)

# Testing
add_executable(test_dsprintf test/dsprintf.c)

target_include_directories(test_dsprintf PRIVATE common)
target_link_libraries(test_dsprintf common)

add_test(NAME dsprintf COMMAND test_dsprintf)
