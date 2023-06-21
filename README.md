# fplayer

A work-in-progress C99 [fseq file](http://github.com/Cryptkeeper/fseq-file-format) player (like [xLights](http://github.com/smeighan/xLights) and [Falcon Player/fpp](https://github.com/FalconChristmas/fpp)) for [Light-O-Rama](https://lightorama.com) hardware. This project is a way for me to explore solutions to a better version of my previous project, [libreorama](https://github.com/Cryptkeeper/libreorama).

Use `fplayer -h` to show program usage and get started. You'll need an fseq file of your choosing, and its audio file (if any). You may attach a Light-O-Rama device (note its serial port device name), but it is not required for the program to playback a sequence. A default channel map is included (`channels.csv`) for use with `-c` (`-c=channels.csv`), but you will likely want to modify it in the future.

```
Usage: fplayer -f=FILE -c=FILE [options] ...

Options:

[Playback]
	-f <file>		FSEQ v2 sequence file path (required)
	-c <file>		Network channel map file path (required)
	-d <device name>	Device name for serial port connection
	-b <baud rate>		Serial port baud rate (defaults to 19200)

[Controls]
	-a <file>		Override audio with specified filepath
	-r <frame ms>		Override playback frame rate interval (in milliseconds)
	-w <seconds>		Playback start delay to allow connection setup

[CLI]
	-h			Print this message and exit
	-v			Print library versions and exit
```

## What's Included

- Precise frame timing with automatic frame loss recovery
- Protocol minifier for reduced bandwidth usage
- "Frame pump" mechanism for pre-buffering upcoming frames
- Support for zstd compressed sequences
- Options for modifying playback speed and audio

## Dependencies

fplayer offloads deserialization and encoding work to my other libraries,
[libtinyfseq](https://github.com/Cryptkeeper/libtinyfseq) and
[liblightorama](https://github.com/Cryptkeeper/liblightorama). Both are provided as git submodules and are built locally via the CMake build configuration. You do not need to install these.

You provide: OpenAL, an ALUT-compatible library (e.g. freealut), libserialport, and zstd (optional).

I have included a few package manager commands below to install the dependencies. You can definitely build it on other platforms, but you'll be responsible for ensuring the dependencies are found and linked.

### macOS (Homebrew)
```brew install libserialport freealut zstd```

### Ubuntu
```apt-get install -y libopenal-dev libalut-dev libserialport-dev libzstd-dev```

## Artifacts
macOS and Ubuntu build downloads are available as artifacts via [Actions](https://github.com/Cryptkeeper/fplayer/actions).

## Channel Maps
Somewhat unique to fplayer as a requirement is a "channel map". FSEQ files provide a 0-indexed array of frame data, each key a "channel", and the interpreting program is responsible for passing off its value ("brightness") to the corresponding device I/O driver. The main consumers of the fseq file format (e.g. xLights and Falcon Player) accomplish this via a lookup operation using additional context provided by your "show directory" files. 

fplayer could try to parse that data and re-use it, but I don't want the complexity or the hard dependency. Instead, you provide your own lookup map using basic annotations in a `.csv` file. As a bonus feature, this also means the value of a given FSEQ "channel" can be changed at program start to any value of your choice. This means you can rearrange existing sequences without modifying anything but the channel map.

### Example Syntax
Each channel map is saved in a `.csv` file (comma-seperated values) with 5 values on each row (you guessed it: seperated by commas). Any line beginning with a `#` is treated as a comment and ignored. Blank lines are ignored. Here, everything is shown as a table to focus on the values being configured:

| FSEQ Start Channel | FSEQ End Channel | LOR Unit | LOR Start Channel | LOR End Channel |
| --- | --- | --- | --- | --- |
| 0 | 15 | 1 | 1 | 16 |
| 16 | 19 | 2 | 1 | 4 |

This maps the first 16 FSEQ channels (0-15, remember the data is 0-indexed) to channels 1-16 (LOR channels are 1-indexed) on unit 1. In order to correctly map between the two ranges, the number of channels on each "side" must match. There are no restrictions on values besides basic limits checking, you are responsible for ensuring your values are correct.

The second row maps the next four FSEQ channels `16, 17, 18 & 19` to the first four channels of another unit, 2. These two rows have mapped a combined 20 channels. The fact they were defined sequentially, and without gaps, doesn't matter.

You are free to add as many mapping rows as you need to fully map the FSEQ channel space. Any channels that are not mapped will not have any data written to them at runtime, so you don't have to worry about deleting/blank the unused channels.

```
# redirect FSEQ channels 0,1,2 to LOR 1 to fix a mistake in my sequence
# this is a comment, they get ignored!
0,0,255,1,1
1,1,255,1,1
2,2,255,1,1
```

You can also "merge" channels by pointing them to a single output. In this case, FSEQ channels `0, 1 & 2` all output to LOR channel 1 on all connected units. A unit value of `255` indicates "all units" and can be used as a shortcut if you are only controlling one unit, or want all units to behave the same.

The included `channels.csv` default simply maps the first 16 FSEQ channels to the first 16 channels of any connected LOR unit. This is likely what most people with AC LOR units are looking for.

```
# FSEQ start, end, LOR unit, start, end
0,15,255,1,16
```
