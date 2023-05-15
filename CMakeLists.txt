cmake_minimum_required(VERSION 3.24)
project(fplayer C)

set(CMAKE_C_STANDARD 99)

if (APPLE)
    include_directories(/usr/local/include)
    link_directories(/usr/local/lib)
endif ()

add_executable(fplayer src/main.c src/audio.c src/audio.h src/seq.c src/seq.h src/time.c src/time.h src/player.c src/player.h src/serial.c src/serial.h src/cmap.c src/cmap.h libtinyfseq/tinyfseq.h src/sleep.c src/sleep.h src/err.h src/compress.c src/compress.h src/pump.c src/pump.h)

add_subdirectory(liblightorama)

target_link_libraries(fplayer lightorama)

if (APPLE)
    target_link_libraries(fplayer libalut.a libserialport.a)
    target_link_libraries(fplayer "-framework OpenAL")
    target_link_libraries(fplayer zstd)
endif ()

add_compile_options(-Wall -Wextra -pedantic)
