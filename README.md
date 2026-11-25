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

The `rogue.6` and `rogue.cat` manual pages are based on the 4.1BSD versions,
with minimal changes to match the JEC command-line and save-file behavior.

## Building

On macOS/Linux, the build uses the system C compiler (`cc`) by default.
Run `make` in `src/` to build `rogue`; no `./configure` step is required.
Windows builds use MSYS2 UCRT64, as described below.

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

### Windows (MSYS2 UCRT64)

Build in the MSYS2 UCRT64 shell with PDCursesMod VT and the required compiler installed:

```sh
cd src
make PROGRAM=rogue34
./rogue34.exe
```

To create the Windows binary distribution, install `zip` and UCRT64 binutils:

```sh
pacman -S --needed zip mingw-w64-ucrt-x86_64-binutils
make dist.win
```

The resulting `rogue34_jec_windows.zip` contains `rogue34.exe`,
Japanese/English READMEs, guides in both languages (TXT and HTML), and
`LICENSE.txt` inside `Rogue 3.4/`. Packaging checks the executable's DLL imports.
The Windows version supports UTF-8 filenames and an
80×24 display; when launched by double-click with a default save present, it offers a resume menu.

### Build-time executable and data-file names

Use `PROG` or `PROGRAM` to set the executable name (`PROG` wins if both are set).
The default save and score filenames are `$(PROG).save` and `$(PROG).scr`.

```sh
make PROGRAM=rogue34
make PROG=rogue34 SAVEFILE=custom.save SCOREFILE=custom.scr
make PROGRAM=rogue34 FILEDIR='~/Documents'
```

Save and score files normally go to the working directory at launch. `FILEDIR` sets a
shared directory; `SAVEDIR` and `SCOREDIR` override it individually. A leading `~/`
refers to the running user's home directory. Create any requested directory beforehand.
The save filename can also be changed at runtime; the full save path is limited to 511 bytes.

### Installing (macOS / Linux / Raspberry Pi OS)

From `src/`, install the executable, manual pages, and bilingual guides:

```sh
sudo make install
man rogue34
```

The installed command is `rogue34` by default (ordinary `make` still builds
`rogue`). `make install PROG=customname` or `make install PROGRAM=customname`
selects another name. `PREFIX` (default `/usr/local`), `BINDIR`, `MANDIR`,
`DOCDIR`, and `DESTDIR` can change the installation paths. The formatted
`rogue.cat` is installed under `share/man/cat6`, alongside the `man6` page.

To remove the installed executable and documentation, use `sudo make uninstall`
(specifying the same variables if they were changed during installation).
Save and score files are not removed. If an absolute `FILEDIR`, `SAVEDIR`, or
`SCOREDIR` is specified, `make install` creates that directory; relative or
`~/` paths are left for the player to create. On Windows use `make dist.win`
instead of `make install`.

The game requires a terminal at least **80 columns by 24 rows**. Its
internal dungeon remains 80×24 when the terminal is larger. When the
terminal becomes smaller during play, the game pauses until it is enlarged.

## Playing, saving and scores

For detailed instructions, see *A Guide to the Dungeons of Doom*:
[TXT](src/rogue_en.txt), [HTML](src/rogue_en.html), or
[Markdown](src/rogue_en.md).
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
- Save format: `R34SAV02`
- No separate lock file is created; the score file itself is locked.
- A successful restore displays the saved filename and modification time, then removes the save file.
- If the score file does not yet exist, `-s` produces no ranking header.
- Option screens show descriptive labels without the ROGUEOPTS keys in parentheses.

## Release contents and further documentation

- `src/`: game source, `Makefile`, bilingual guides, and JEC-adjusted manual pages
- `PORTING.md`: consolidated porting decisions, original-rules comparison,
  language/Unicode details and portability notes (Japanese)
- `LICENSE`: redistribution terms

`make clean` removes generated object files and `src/rogue`.
The original archive's unused `:ctfix` editor script is omitted from this
port distribution; it is not part of the build.

GitHub Actions builds macOS/Linux/Windows versions and validates the Windows ZIP,
available as the `rogue34-windows-ucrt64` artifact. It does not publish GitHub Releases.
