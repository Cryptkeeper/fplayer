cmake_minimum_required(VERSION 3.24)
project(fplayer C)

set(CMAKE_C_STANDARD 99)

if (APPLE)
    include_directories(/usr/local/include)
    link_directories(/usr/local/lib)
endif ()

add_executable(fplayer src/main.c src/audio.c src/audio.h src/error.h)

if (APPLE)
    target_link_libraries(fplayer libalut.a)
    target_link_libraries(fplayer "-framework OpenAL")
endif ()

add_compile_options(-Wall -Wextra -pedantic)
