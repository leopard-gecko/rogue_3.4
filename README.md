# Rogue 3.4 — macOS / Linux port

This project adapts the reconstructed 1981 Berkeley Rogue 3.4 source for
modern C11 compilers and terminal-based systems. It aims to preserve the
original game's rules and ASCII presentation, while correcting portability
and memory-safety issues. It is **not** a byte-identical reproduction of the
1981 executable.

[日本語版 README](README_ja.md)

## Historical source

The reference source reconstructs Rogue 3.4 (Michael Toy, Kenneth Arnold and
Glenn Wichman). The original project's README describes a byte-exact match,
when built with its modified 2.8BSD toolchain, to the CSRG Archive CD-ROM
Disc 1 executable `2.8/usr/bin.v7/ucb/rogue` (MD5
`88f791623b2a684d2555891c0f5b6735`). The historical `rogue.doc`
corresponds to `2.8/usr/doc/rogue.doc` (MD5
`e7113c0e9aedca6edf47f2127cfad4f7`); the `rogue.6` manual page is
from 4.1BSD (MD5 `f84eef587bc38bc0823f2cd029df030b`).

Those historical documents are included **unchanged** for reference. In
particular, old save and score filenames mentioned in `rogue.6` describe
historical systems, not this port. Use the paths listed below.

## Building

The build uses the system C compiler (`cc`) by default. From the extracted
project directory, `make` builds the executable in `src/`; no `./configure`
step or explicit `CC=clang` setting is required.

### macOS

Install Apple's Command Line Tools if they are not already available:

```sh
xcode-select --install
```

Then build and run:

```sh
cd src
make clean
make
./rogue
```

On macOS, the build uses the system compiler and curses library without
Homebrew dependencies.

### Linux (Debian / Ubuntu)

Install the compiler, make, and development libraries:

```sh
sudo apt update
sudo apt install build-essential libncurses-dev libcrypt-dev
```

Then, from the extracted project directory:

```sh
cd src
make clean
make
./rogue
```

`libncurses-dev` provides curses headers and the ncurses link library;
`libcrypt-dev` is needed for the game's `crypt()` usage and `-lcrypt` link.

### Raspberry Pi OS

Raspberry Pi OS is Debian-based and uses the same build dependencies:

```sh
sudo apt update
sudo apt install build-essential libncurses-dev libcrypt-dev
```

Build and run in the same way:

```sh
cd src
make clean
make
./rogue
```

Raspberry Pi OS (including 32-bit and 64-bit installations) has **not yet
been tested on Raspberry Pi hardware**; these are the intended build steps,
not a claim of verified operation on either architecture.

### Other Linux distributions

Install a C11-capable C compiler, `make`, ncurses development headers/library,
and the libcrypt development library using your distribution's package manager.
Then run `make` in `src/`. The Makefile selects `-lcurses` on macOS and
`-lncurses -lcrypt` on Linux, based on the host OS. If needed, explicitly
select a compiler with `make CC=clang` or `make CC=gcc`.

**Validation status:** Native macOS compilation and game operation have
been confirmed by the maintainer. On Linux, Clang/GCC C11 builds and automated
terminal tests have passed using a test-only curses declaration header and
the system ncurses runtime; a normal Linux build with `libncurses-dev` and
Raspberry Pi hardware operation have not been separately verified here.

The game requires a terminal at least **80 columns by 24 rows**. Its
internal dungeon remains 80×24 when the terminal is larger. When the
terminal becomes smaller during play, the game pauses until it is enlarged.

## Playing, saving and scores

| Action | Command |
| --- | --- |
| Start a new game | `./rogue` |
| Show in-game help | `?` |
| Move | `h`, `j`, `k`, `l` (diagonals: `y`, `u`, `b`, `n`) |
| Show inventory | `i` |
| Open options | `o` |
| Save and exit | `S`, then confirm with `y` |
| Resume the default save | `./rogue -r` |
| Resume a named save | `./rogue /path/to/savefile` |
| Show scores | `./rogue -s` |
| Quit normally | `Q` |
| Suspend and resume | `Ctrl-Z`, then `fg` in the shell |
| Open an interactive shell | `!`, then `exit` to return |

The current defaults are:

- Save: `~/rogue.save`
- Scores: `~/.rogue34.scr`
- Save format: versioned `R34SAV01` (not compatible with the historical
  process-memory save format)

The save filename and HOME directory can contain UTF-8 bytes (for example,
Japanese characters), spaces, and paths longer than the historical 80-byte
buffer. Save paths are limited to **511 bytes** in this port, including the
directory and filename. `ROGUEOPTS=file=~/保存.save` selects a filename under
HOME without converting its UTF-8 bytes. If the path cannot fit, startup
reports an error rather than silently cutting it off. The options screen
remains 80 columns wide and may abbreviate a long path for display; this
visual limit does not truncate the actual saved filename. Other game text
remains limited to the historical single-byte presentation.

A successful normal restore consumes the save file; an invalid save is
retained. No automatic conversion of older save or score files is performed.
Only `S` saves; SIGHUP and SIGTERM exit without saving. Do not restore
save files obtained from untrusted sources.

On death, an ASCII tombstone stays visible until Return is pressed; then
the score ranking appears. The game retains the original ability to hold
non-weapon items with the `w` command. The terminal and game messages remain
English-only in this port.

Thrown items are briefly displayed in flight (approximately 30 ms per
visible square). Continuous movement (uppercase direction keys or numeric
repeated movement) pauses about 30 ms between visible steps. Ordinary
single-step movement and monster turns are not delayed. When `jump` is
enabled, continuous movement retains the original suppressed intermediate
draws and incurs no extra delay. The startup greeting is displayed for
about 1000 ms before entering the game screen. After a successful `S` save,
`File name: ...` remains visible for about 1000 ms before the game exits.
Save-confirmation and restore messages keep their existing input behavior;
normal in-game messages and `--More--` are not delayed.

## Release contents and further documentation

- `src/`: game source, `Makefile`, and historical guide/manual files
- `PORTING.md`: technical history, [original-rules comparison](PORTING.md#original-rules-audit),
  and portability notes
- `LICENSE`: redistribution terms

No executable binaries or test-only curses headers are included in the
source archive. `make clean` removes generated object files and `src/rogue`.
The original archive's unused `:ctfix` editor script is omitted from this
port distribution; it is not part of the build.
