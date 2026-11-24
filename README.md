# Rogue 3.4 JEC — macOS / Linux / Windows port

This project adapts the reconstructed 1981 Berkeley Rogue 3.4 source for
modern C11 compilers and terminal-based systems.

[日本語版 README](README_ja.md)

## Source code basis

This port is based on the reconstructed Rogue 3.4 source at
[rogueforge/rogue3.4](https://github.com/rogueforge/rogue3.4).
It reconstructs the 1981 game developed by Michael Toy, Kenneth Arnold,
and Glenn Wichman; it is not the original 1981 source code. According to
that project's README, building the reconstruction with a modified 2.8BSD
toolchain produces a byte-exact match to the CSRG Archive CD-ROM Disc 1
executable `2.8/usr/bin.v7/ucb/rogue`
(MD5: `88f791623b2a684d2555891c0f5b6735`).

The historical `rogue.doc` corresponds to `2.8/usr/doc/rogue.doc`
(MD5: `e7113c0e9aedca6edf47f2127cfad4f7`); the `rogue.6` manual page
is from 4.1BSD (MD5: `f84eef587bc38bc0823f2cd029df030b`).

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

### Other Linux distributions

Install a C11-capable C compiler, `make`, ncurses development headers/library,
and the libcrypt development library using your distribution's package manager.
Then run `make` in `src/`. The Makefile selects `-lcurses` on macOS and
`-lncursesw -lcrypt` on Linux, based on the host OS. If needed, explicitly
select a compiler with `make CC=clang` or `make CC=gcc`.

The game requires a terminal at least **80 columns by 24 rows**. Its
internal dungeon remains 80×24 when the terminal is larger. When the
terminal becomes smaller during play, the game pauses until it is enlarged.

## Language selection and Japanese localization

The startup display language follows the OS locale or `ROGUEOPTS`:

```sh
ROGUEOPTS='language=ja' ./rogue
ROGUEOPTS='language=en' ./rogue
ROGUEOPTS='language=auto' ./rogue
```

Use `o` during the game to change the language via `J` or `E`. **Language is
immediately above Save file**, as in Rogue 3.6 JEC. The options screen is
redrawn when switching languages. `make original` uses an English default,
while `make japanese` selects a Japanese default; neither changes game rules.

This port adopts Rogue 3.6 JEC's Japanese option labels and status labels,
including 階, 金塊, 体力, 強さ, 守備 and 経験, as well as its wording for selected
messages. Names, fruit and item call-names now accept UTF-8 and support
Backspace, cursor-left/right and Delete at grapheme boundaries. Display
clipping uses terminal columns, not UTF-8 bytes; score entries can retain
UTF-8 names in their existing hex-encoded format.

The Japanese localization extends the Rogue 3.6 JEC vocabulary to Rogue 3.4's
monster and item names, inventory, combat, exploration, help and game-over
messages. The original English data remains in the game state, and names
are translated only for display; switching languages does not affect the
underlying identification state. Unknown scroll titles are transliterated
from their saved English syllables into Japanese using the 3.6 syllable
table; the original scroll-title syllable boundaries are not stored in a
3.4 save, so this is a display-only approximation.

The game rules and timing are unchanged. New scroll
titles are bounded to 40 UTF-8 bytes in both English and Japanese, following
Rogue 3.6/5.4 JEC. This can change the number of random calls during title
generation. Saved titles must already satisfy the length restriction; older save files
are not supported. The Japanese pickup message puts the item letter before the
verb, e.g. `あなたは短弓 (o)を手に入れた。`, following 3.6 JEC.

See [PORTING.md](PORTING.md) for consolidated porting and localization notes (Japanese).

**Save files created by earlier development stages (`R34SAV01`) are intentionally unsupported.**

## Playing, saving and scores

For detailed instructions, see the original guide,
[A Guide to the Dungeons of Doom (rogue.doc)](src/rogue.doc).
It explains the goal of the game, screen symbols, commands, items, and combat.

| Action | Command |
| --- | --- |
| Start a new game | `./rogue` |
| Show in-game help | `?` |
| Move | `h`, `j`, `k`, `l` (diagonals: `y`, `u`, `b`, `n`) |
| Show inventory | `i` |
| Open options | `o` |
| Save and exit | `S`: `y` saves under the shown name; `n` prompts for another filename (Escape cancels) |
| Resume the default save | `./rogue -r` |
| Resume a named save | `./rogue /path/to/savefile` |
| Show scores | `./rogue -s` |
| Quit normally | `Q` |
| Suspend and resume | `Ctrl-Z`, then `fg` in the shell |
| Open an interactive shell | `!`, then `exit` to return |

The current defaults are:

- Save: `./rogue.save` (the working directory at launch)
- Scores: `./rogue.scr` (the working directory at launch; not a hidden file)
- Save format: versioned `R34SAV02` (not compatible with the historical
  process-memory save format)
- No separate lock file is created; the score file itself is locked.
- A successful restore displays the saved filename and its file modification time.
- If the score file does not yet exist, `-s` produces no ranking header.
- Option screens show descriptive labels without the ROGUEOPTS keys in parentheses.


### Build-time executable and data-file names

From `src/`, `PROG` and `PROGRAM` are interchangeable names for the executable
(`PROG` wins if both are explicitly set). The default save and score names
follow the selected executable name:

```sh
make PROGRAM=rogue34
# executable rogue34; ./rogue34.save and ./rogue34.scr

make PROG=rogue34 SAVEFILE=custom.save SCOREFILE=.custom.scr
# ./custom.save and ./.custom.scr (hidden score file)

make PROGRAM=rogue34 FILEDIR='~/Documents' \
    SAVEFILE=rogue34.save SCOREFILE=rogue34.scr
# files live in the RUNNING user's ~/Documents; create that directory first
```

| Make variable | Purpose | Default |
| --- | --- | --- |
| `PROG` / `PROGRAM` | Executable basename (`PROG` takes precedence) | `rogue` |
| `SAVEFILE` | Default save filename | `$(PROG).save` |
| `SCOREFILE` | Score filename | `$(PROG).scr` |
| `FILEDIR` | Shared save and score directory | unset (working directory) |
| `SAVEDIR` | Save directory, overrides `FILEDIR` | `FILEDIR` or `.` |
| `SCOREDIR` | Score directory, overrides `FILEDIR` | `FILEDIR` or `.` |

A score filename starting with `.` is hidden on macOS/Linux; otherwise it is
visible. By default, both data files are stored in the **working directory
when the game is launched**, not necessarily alongside the executable. To
save them under HOME instead, specify `FILEDIR='~'` or `FILEDIR='~/...'`.
Directory options beginning with `~/` expand using the HOME of the
**person running the executable**, never the machine that built it. Absolute
paths are supported; ordinary relative directories are based on the working
directory. The directories are not created automatically.

Changing Make options automatically recompiles objects whose embedded settings
must change. `make PROGRAM=rogue34 original` and the `japanese` target also
respect the selected names and directories. `SAVEFILE` is only the initial
save name and remains overridable by `ROGUEOPTS` and the `o` command; the
score filename is selected at build time.

The save filename and HOME directory can contain UTF-8 bytes (for example,
Japanese characters), spaces, and paths longer than the historical 80-byte
buffer. Save paths are limited to **511 bytes** in this port, including the
directory and filename.

For example, specify a filename under HOME with `ROGUEOPTS`:

```sh
ROGUEOPTS='file=~/mygame.save' ./rogue
```

A successful normal restore consumes the save file.

## Release contents and further documentation

- `src/`: game source, `Makefile`, and historical guide/manual files
- `PORTING.md`: consolidated porting decisions, original-rules comparison,
  language/Unicode details and portability notes (Japanese)
- `LICENSE`: redistribution terms

`make clean` removes generated object files and `src/rogue`.
The original archive's unused `:ctfix` editor script is omitted from this
port distribution; it is not part of the build.


## Windows port (stage 6-C: CI distribution verification)

Build with **MSYS2 UCRT64 and PDCursesMod VT** from `src/`:

```sh
make PROGRAM=rogue34 clean
make PROGRAM=rogue34
```

The Windows executable is `rogue34.exe`. Its default working-directory
save and score files are `rogue34.save` and `rogue34.scr`; all the build
variables (`PROG`/`PROGRAM`, `SAVEFILE`, `SCOREFILE`, `FILEDIR`,
`SAVEDIR`, `SCOREDIR`) continue to work as on macOS/Linux.

Windows saving, restoring and persistent high scores were added in stage 4
and tested on a real Windows system. Windows and Unix continue to use the
same `R34SAV02` save format and `ROGUE34-SCORES-1` score format. UTF-8
paths are converted to UTF-16 at the Windows API boundary. No extra lock
file is created.

**New in stage 5:**

- The restored file's local modification time is shown in Japanese in
  Japanese mode, e.g. `2026年11月23日（月）10:30:00`. English mode retains
  the historical `Mon Nov 23 10:30:00 2026` format. This applies to
  macOS/Linux as well as Windows. Long paths are abbreviated *only on screen*.
- A directly Explorer-launched executable shows a saved-game menu **only
  if the configured default save exists**: `R`/Enter resume; `N` new game;
  `F` choose a different save in the Unicode file picker; `Q` quit.
  With no default save, the game starts immediately.
- An existing shell and explicit `-r`/save filename arguments continue
  to bypass the menu and preserve the terminal's configured size.
- Windows wizard password checking works at startup and in-game with Ctrl-P.
  It uses the original hash and a FreeSec DES `crypt(3)` implementation;
  see `LICENSE` for this third-party code's license and attribution.
- Only an Explorer-launched console waits for Enter on the final score
  screen. A normal terminal session does not get an additional exit pause.

The 80x24 direct-launch sizing, UTF-8/emoji support, and Rogue 3.6 JEC
cursor-flicker workaround from previous stages are retained.

**The maintainer has confirmed stage-5 functionality on real Windows.**
Stage 6-A checks the combined implementation, including cross-language
save/restore, integrity checks, protection of damaged saves and scores,
UTF-8 score names, and concurrent score *reading*. Unix execution tests
and strict Windows-stub compilation checks passed; the GitHub Actions
Windows job also checks switching build names, configured score names,
and forced English/Japanese builds on native MSYS2 UCRT64.

**Still requiring real-device verification:** save files actually exchanged
between Windows and Unix, concurrent Windows score writers and restore
attempts, and long-running behavior in both Windows Terminal and Console
Host. Game rules, RNG, messages and disk formats are unchanged in stage
6-A. Stage 6-B adds `make dist.win` for release packaging.

In MSYS2 UCRT64, install the ZIP utility and binutils if necessary, then
run the distribution target from `src/`:

```sh
pacman -S --needed zip mingw-w64-ucrt-x86_64-binutils
cd src
make dist.win
```

The resulting `src/rogue34_jec_windows.zip` contains a `Rogue 3.4/`
folder with five files: `rogue34.exe`, `rogue34_original.exe`,
`README_ja.txt`, `README_en.txt`, and `LICENSE.txt`. The normal executable
uses the OS language by default; the `original` executable starts in English.
**Both use the same game rules and the same** `rogue34.save` and
`rogue34.scr` **names**. The packaging target builds with fixed distribution
options instead of inheriting individual `PROGRAM`/file-directory choices.

The packaging target inspects each PE executable's DLL imports and aborts
if it finds dependencies outside a set of Windows system libraries. It
therefore does not silently publish a ZIP with an unbundled MSYS2 runtime
or PDCurses DLL. The Windows cursor-shape fixes for quitting and returning
from a shell are preserved. Stage 6-C adds a Windows UCRT64 GitHub Actions job that builds this ZIP,
checks the exact five-file manifest and PE architecture, and runs
non-interactive score/language smoke tests on both *extracted* executables.
The ZIP is uploaded as the `rogue34-windows-ucrt64` workflow artifact
(available under Actions → Build Rogue 3.4 → selected run → Artifacts).
The downloadable GitHub artifact contains `rogue34_jec_windows.zip`;
no GitHub Release is automatically published. The CI job must still pass
on GitHub to establish an actual MSYS2 run; interactive Windows Terminal
and Console Host behavior needs a separate real-device check.
See [`PORTING.md`](PORTING.md) for further details.
