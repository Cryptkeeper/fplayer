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

add_subdirectory(liblightorama)

file(GLOB_RECURSE SDS_FILES "sds/*.c")

option(BUILD_MFTOOL "enable building mftool executable" true)

if (BUILD_MFTOOL)
    file(GLOB_RECURSE MFTOOL_FILES "mftool-src/*.c")
    set_source_files_properties(${MFTOOL_FILES} PROPERTIES COMPILE_FLAGS ${PEDANTIC_COMPILER_FLAGS})

    add_executable(mftool ${MFTOOL_FILES} ${SDS_FILES})

    target_include_directories(mftool PRIVATE libtinyfseq sds stb)
endif ()

option(BUILD_GENTOOL "enable building gentool executable" true)

if (BUILD_GENTOOL)
    file(GLOB_RECURSE GENTOOL_FILES "gentool-src/*.c")
    set_source_files_properties(${GENTOOL_FILES} PROPERTIES COMPILER_FLAGS ${PEDANTIC_COMPILER_FLAGS})

    add_executable(gentool ${GENTOOL_FILES} ${SDS_FILES})

    target_include_directories(gentool PRIVATE libtinyfseq sds stb)
    target_link_libraries(gentool lightorama)
endif ()

file(GLOB_RECURSE SRC_FILES "src/*.[ch]")
set_source_files_properties(${SRC_FILES} PROPERTIES COMPILE_FLAGS ${PEDANTIC_COMPILER_FLAGS})

add_executable(fplayer ${SRC_FILES} ${SDS_FILES})

target_include_directories(fplayer PRIVATE libtinyfseq sds stb)
target_link_libraries(fplayer lightorama m serialport)

option(USE_OPENAL "enable audio playback via alut+openal" true)

if (USE_OPENAL)
    target_link_libraries(fplayer alut)

    if (APPLE)
        target_link_libraries(fplayer "-framework OpenAL")
    else ()
        target_link_libraries(fplayer openal)
    endif ()

    add_compile_definitions(ENABLE_OPENAL)
endif ()

option(USE_ZSTD "enable zstd decompression at playback" true)

if (USE_ZSTD)
    target_link_libraries(fplayer zstd)
    add_compile_definitions(ENABLE_ZSTD)
endif ()
