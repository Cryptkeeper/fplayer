cmake_minimum_required(VERSION 3.24)
project(fplayer C)

set(CMAKE_C_STANDARD 99)

if (APPLE)
    include_directories(/usr/local/include)
    link_directories(/usr/local/lib)
endif ()

file(GLOB_RECURSE SDS_FILES "sds/*.c")

option(BUILD_MFTOOL "enable building mftool executable" true)

if (BUILD_MFTOOL)
    add_executable(mftool "mftool/main.c" ${SDS_FILES})

    target_include_directories(mftool PRIVATE libtinyfseq sds stb)
endif ()

file(GLOB_RECURSE SRC_FILES "src/*.[ch]")
file(GLOB_RECURSE TEST_FILES "test/*.c")

add_executable(fplayer ${SRC_FILES} ${TEST_FILES} ${SDS_FILES})

enable_testing()

add_subdirectory(liblightorama)

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

option(USE_PTHREAD "enable pthread frame pre-buffering" true)

if (USE_PTHREAD)
    add_compile_definitions(ENABLE_PTHREAD)
endif ()

add_compile_options(-Wall -Wextra -pedantic)

# silence macOS OpenAL deprecation warning messages
if (APPLE)
    add_compile_definitions(OPENAL_DEPRECATED=)
endif ()
