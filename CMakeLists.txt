cmake_minimum_required(VERSION 3.24)
project(fplayer C)

set(CMAKE_C_STANDARD 99)

if (APPLE)
    include_directories(/usr/local/include)
    link_directories(/usr/local/lib)
endif ()

file(GLOB_RECURSE SRC_FILES "src/*.[ch]")
file(GLOB_RECURSE TEST_FILES "test/*.c")

file(GLOB_RECURSE SDS_FILES "sds/*.c")

add_executable(fplayer ${SRC_FILES} ${TEST_FILES} ${SDS_FILES})

enable_testing()

add_subdirectory(liblightorama)

target_include_directories(fplayer PRIVATE sds libtinyfseq)

target_link_libraries(fplayer lightorama m alut serialport zstd)

if (APPLE)
    target_link_libraries(fplayer "-framework OpenAL")
else ()
    target_link_libraries(fplayer openal)
endif ()

add_compile_options(-Wall -Wextra -pedantic)

add_compile_definitions(ENABLE_ZSTD)

# silence macOS OpenAL deprecation warning messages
if (APPLE)
    add_compile_definitions(OPENAL_DEPRECATED=)
endif ()
