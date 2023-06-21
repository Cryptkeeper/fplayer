# fplayer

A work-in-progress C99 [fseq file](http://github.com/Cryptkeeper/fseq-file-format) player (like [xLights](http://github.com/smeighan/xLights) and [Falcon Player/fpp](https://github.com/FalconChristmas/fpp)) for [Light-O-Rama](https://lightorama.com) hardware. This project is a way for me to explore solutions to a better version of my previous project, [libreorama](https://github.com/Cryptkeeper/libreorama).

## What's included

- Precise frame timing with automatic frame loss recovery
- Protocol minifier for reduced bandwidth usage
- "Frame pump" mechanism for pre-buffering upcoming frames
- Support for zstd compressed sequences
- Options for modifying playback speed and audio

## Dependencies

fplayer offloads deserialization and encoding work to my other libraries,
[libtinyfseq](https://github.com/Cryptkeeper/libtinyfseq) and
[liblightorama](https://github.com/Cryptkeeper/liblightorama). Both are provided as git submodules and are built locally via the CMake build configuration.

fplayer requires OpenAL, an ALUT-compatible library (e.g. freealut), libserialport, and zstd (optional).