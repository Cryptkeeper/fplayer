# fplayer

A cross-platform C99 [fseq file](http://github.com/Cryptkeeper/fseq-file-format) player (like [xLights](http://github.com/smeighan/xLights) and [Falcon Player/fpp](https://github.com/FalconChristmas/fpp)) for [Light-O-Rama](https://lightorama.com) hardware. This project is a way for me to explore solutions to a better version of my previous project, [libreorama](https://github.com/Cryptkeeper/libreorama).

Use `fplayer -h` to show program usage and get started.

You'll need an fseq file of your choosing, and its audio file (if any). You may attach a Light-O-Rama device (note its serial port device name or list available serial ports with `fplayer -l`), but it is not required for the program to playback a sequence. A default channel map is included (`channels.json`) for use with `-c` (`-c=channels.json`), but you will likely want to modify it in the future.

```
Usage: fplayer -f=FILE -c=FILE [options] ...

Options:

[Playback]
	-f <file>		FSEQ v2 sequence file path (required)
	-c <file>		Network channel map file path (required)
	-d <device name|stdout>	Device name for serial port connection
	-b <baud rate>		Serial port baud rate (defaults to 19200)

[Controls]
	-a <file>		Override audio with specified filepath
	-w <seconds>            Playback start delay to allow connection setup

[CLI]
	-t <file>		Test load channel map and exit
	-l			Print available serial port list and exit
	-h			Print this message and exit
```

## What's Included

- Precise frame timing with automatic frame loss recovery
- Protocol minifier for reduced bandwidth usage
- "Frame pump" mechanism for pre-buffering upcoming frames
- Support for zstd compressed sequences
- Options for modifying playback speed and audio

All in a small package. Most of fplayer's resource usage originates from audio file decoding and playback.

## Dependencies

fplayer offloads deserialization and encoding work to my other libraries,
[libtinyfseq](https://github.com/Cryptkeeper/libtinyfseq) and
[liblorproto](https://github.com/Cryptkeeper/liblorproto). Both are provided as git submodules and are built locally via the CMake build configuration. You do not need to install these.

You must provide: cJSON, libserialport, OpenAL, an ALUT-compatible library, and zstd.

I have included a few package manager commands below to install the dependencies. You can definitely build it on other platforms, but you'll be responsible for ensuring the dependencies are found and linked.

### macOS (Homebrew)
```brew install cjson libserialport freealut zstd```

### Ubuntu
```apt-get install -y libcjson-dev libopenal-dev libalut-dev libserialport-dev libzstd-dev```

### FreeBSD
```pkg install -y libcjson openal-soft freealut libserialport zstd```

## Setup

1. Clone the repository and its submodules: `git clone --recursive git@github.com:Cryptkeeper/fplayer.git`
2. Configure the CMake project with `cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebugInfo`
    - macOS users with the dependencies provided via Homebrew should append `-DCMAKE_PREFIX_PATH=$(brew --prefix)`
    - FreeBSD users may need to append `-DCMAKE_PREFIX_PATH=/usr/local` to correctly link with system libraries
3. Compile the project with `cmake --build build`
4. (Optional) Run CTests with `cd build && ctest .`

fplayer can be compiled using (at least) clang and gcc.

## Artifacts
macOS, Ubuntu and Windows build downloads are available as artifacts via [GitHub Actions](https://github.com/Cryptkeeper/fplayer/actions).

FreeBSD is generally supported, although the build is not automated due to GitHub Actions' lack of support in its Action Runner.

## Tests & Sanitizers
Test coverage is provided by CTest for components of fplayer and most of the libraries supporting it (libtinyfseq, liblorproto, and the repo-specific common library).

GitHub Actions workflows provide sanitizer coverage using [AddressSanitizer and UBSan](https://github.com/google/sanitizers) and/or [Valgrind](https://valgrind.org) (depending on the toolchain). You may enable any of the sanitizers in your CMake build using `-DUSE_ASAN=ON` and/or `-DUSE_UBSAN=ON`.

[libFuzzer](https://llvm.org/docs/LibFuzzer.html) is used to provide basic fuzzing coverage for the fseq file format parsing library used by fplayer, [libtinyfseq](https://github.com/Cryptkeeper/libtinyfseq).

## Channel Maps
In order to map the channel data from an fseq file to your real-life hardware network, fplayer requires you to configure a channel map: fseq files provide a 0-indexed array of frame data, each key a "channel", and the interpreting program is responsible for passing off its value ("brightness") to the corresponding device I/O driver. 

The main consumers of the fseq file format (e.g. xLights and Falcon Player) accomplish this via a lookup operation using additional context provided by your "show directory" files. fplayer could try to parse that data and re-use it, but I don't want the complexity or the hard dependency. Instead, you provide your own lookup map by writing a basic JSON structure and providing the path as an argument to fplayer. As a bonus feature, this also means the value of a given FSEQ "channel" can be changed at program start to any value of your choice. This means you can rearrange existing sequences without modifying anything but the channel map.

### Example
A channel map is saved in a `.json` file. For example, the following channel map defines 32 channels from the fseq file, mapping the output to two banks of 16 channels on two different LOR hardware units attached to the network. 

```json
[
   {
      "index": {
         "from": 0,
         "to": 15
      },
      "circuit": {
         "from": 1,
         "to": 16
      },
      "unit": 1
   },
   {
      "index": {
         "from": 16,
         "to": 31
      },
      "circuit": {
         "from": 17,
         "to": 32
      },
      "unit": 2
   }
]
```

The `index` and `circuit` objects define the range of channels to map from the FSEQ file to the LOR hardware. The `unit` value is the LOR hardware unit number to send the data to. The `from` and `to` values are inclusive, so the first row maps FSEQ channels 0-15 to LOR channels 1-16 on unit 1. The second row maps FSEQ channels 16-31 to LOR channels 17-32 on unit 2.

The length of each range must match, and fplayer will print an error at start if they do not. There is no requirement for mappings to be sequential, contiguous or cover the full fseq channel space. You can also map multiple fseq channels to the same LOR hardware channel. Any channels that are not mapped will not have any data written to them at runtime, so you don't have to worry about deleting/blank the unused channels. fplayer will print a status message when starting to notify you of any missing channel mappings.

The included `channels.json` default simply maps the first 16 FSEQ channels to the first 16 channels of any connected LOR unit. This is likely what most people with AC LOR units are looking for.
