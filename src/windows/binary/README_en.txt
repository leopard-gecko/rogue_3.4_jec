Rogue 3.4 JEC for Windows
=========================

Rogue 3.4 JEC is a Japanese-localized, modern Windows port based on a
reconstruction of the 1981 Berkeley Rogue 3.4 sources. It is not the
original 1981 source tree. The gameplay is based on Rogue 3.4.

PACKAGE CONTENTS
  rogue34.exe           Normal build (startup language follows OS locale)
  rogue34_original.exe  English-default build
  README_ja.txt         Japanese instructions
  README_en.txt         This file
  rogue_ja.txt          Japanese guide (plain text)
  rogue_ja.html         Japanese guide (HTML)
  rogue_en.txt          English guide (plain text)
  rogue_en.html         English guide (HTML)
  LICENSE.txt           Copyright and redistribution conditions

The English-default executable changes only the initial display language.
It does not revert gameplay or functionality to a separate historical
release. Both executables support changing the language in-game.

STARTING THE GAME
Double-click rogue34.exe or rogue34_original.exe on 64-bit Windows.
Windows Terminal and ConsoleHost are the intended console frontends.
The game uses an 80-column by 24-row dungeon. An Explorer-launched game
may adjust its console size; a game launched from an existing shell will
not forcibly resize the terminal. Displaying Japanese and emoji requires
suitable terminal support and fonts.

If there is no default save file, a new game starts directly.
If a default save file exists, a direct launch offers:
  R or Enter  Resume the default save
  N           Start a new adventure
  F           Choose a save file with the file picker
  Q           Quit

BASIC COMMANDS
  h / j / k / l  Move left/down/up/right
  y / u / b / n  Move diagonally
  i              Inventory
  o              Options, including display language
  ?              Command help
  S              Save and exit
  Q              Quit the game
  !              Open a temporary shell (type exit to return)

SAVE AND SCORE FILES
Both executables use identical default file names:
  rogue34.save  Saved game
  rogue34.scr   High scores
Their directory is the process's working directory at startup; this is
not guaranteed to be the directory containing the EXE. Executables run
with the same working directory share the same saves and scores. The
save format is R34SAV02.

From an existing terminal, you may also run:
  rogue34.exe -r             Resume the default save file
  rogue34.exe -s             Display high scores
  rogue34.exe "my save.save" Resume a specified save file

ROGUEOPTS can select language=ja or language=en; ROGUE_LANG is not used.
Only an Explorer-launched game waits for Enter after the final score
screen. A command-line launch does not add an exit pause.

DISTRIBUTION AND LICENSING
This Windows build is produced with MSYS2 UCRT64 and PDCursesMod VT.
The packaging target examines each EXE's directly imported DLLs and refuses
to create the ZIP if non-system dependencies are detected. MSYS2 itself
is not included. See LICENSE.txt for the original BSD conditions and
attribution/redistribution terms for the FreeSec DES crypt(3) code.

Reconstructed historical source:
https://github.com/rogueforge/rogue3.4
For full playing instructions, see the included guides. The HTML files can
be opened in a browser. The source distribution also includes Markdown guides.
