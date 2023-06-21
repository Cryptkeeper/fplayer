cmake_minimum_required(VERSION 3.24)
project(fplayer C)

set(CMAKE_C_STANDARD 99)

if (APPLE)
    include_directories(/usr/local/include)
    link_directories(/usr/local/lib)
endif ()

add_executable(fplayer src/main.c src/audio.c src/audio.h src/seq.c src/seq.h src/time.c src/time.h src/player.c src/player.h src/serial.c src/serial.h src/cmap.c src/cmap.h libtinyfseq/tinyfseq.h src/sleep.c src/sleep.h src/compress.c src/compress.h src/pump.c src/pump.h src/parse.c src/parse.h src/mem.h src/mem.c src/err.h src/err.c src/minifier.c src/minifier.h src/lor.h)

add_subdirectory(liblightorama)

target_link_libraries(fplayer lightorama alut serialport zstd)

if (APPLE)
    target_link_libraries(fplayer "-framework OpenAL")
endif ()

add_compile_options(-Wall -Wextra -pedantic)

add_compile_definitions(ENABLE_ZSTD)
