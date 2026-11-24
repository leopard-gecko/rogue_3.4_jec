# Native Windows (MSYS2 UCRT64) uses its own build file; all other
# platforms keep the existing macOS/Linux build path unchanged.
ifeq ($(MSYSTEM),UCRT64)
include Makefile.win
else ifeq ($(OS),Windows_NT)
include Makefile.win
else
# Rogue 3.4 JEC -- macOS / Unix
CC ?= cc
CFLAGS ?= -O2
# Both aliases are supported. An explicit PROG= takes precedence if both are set.
PROG ?= rogue
ifneq ($(strip $(PROGRAM)),)
ifneq ($(origin PROG),command line)
override PROG := $(PROGRAM)
endif
endif

# The executable name determines the default save and score filenames.
# By default both files are created in the RUNNING process's working directory.
# FILEDIR overrides both directories; explicit SAVEDIR/SCOREDIR override
# FILEDIR separately. A leading ~/ in an explicit path uses the running user's
# home directory, not the machine used for the build.
FILEDIR ?=
SAVEFILE ?= $(PROG).save
SCOREFILE ?= $(PROG).scr
SAVEDIR ?= $(if $(strip $(FILEDIR)),$(FILEDIR),.)
SCOREDIR ?= $(if $(strip $(FILEDIR)),$(FILEDIR),.)
DEFAULT_LANGUAGE ?= 0

# Keep C11 and warnings even when callers override CFLAGS.
PORT_CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -Wmissing-prototypes
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
PLATFORM_CPPFLAGS = -D_DARWIN_C_SOURCE
LDLIBS += -lcurses
else
PLATFORM_CPPFLAGS = -D_DEFAULT_SOURCE
LDLIBS += -lncursesw -lcrypt
endif

# Quote each -D as one shell argument, including names/paths with spaces or
# apostrophes. Macro contents are C string literals.
unix_shell_quote = '$(subst ','"'"',$(1))'
BUILD_CPPFLAGS = $(PLATFORM_CPPFLAGS) -DDEFAULT_LANGUAGE=$(DEFAULT_LANGUAGE) \
  $(call unix_shell_quote,-DROGUE_SAVEFILE="$(SAVEFILE)") \
  $(call unix_shell_quote,-DROGUE_SAVEDIR="$(SAVEDIR)") \
  $(call unix_shell_quote,-DROGUE_SCOREFILE="$(SCOREFILE)") \
  $(call unix_shell_quote,-DROGUE_SCOREDIR="$(SCOREDIR)")

include Makefile.sources
SRCS = $(GAME_SRCS)
OBJS = $(SRCS:.c=.o)

.PHONY: all clean original english japanese FORCE_BUILD_OPTIONS
all: $(PROG)
$(PROG): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

# The configuration stamp changes only when effective build options change.
# Switching PROG/PROGRAM, SAVEFILE, SCOREFILE, directories, compiler, or
# initial language rebuilds the objects automatically (no make clean needed).
FORCE_BUILD_OPTIONS:
.build-options: FORCE_BUILD_OPTIONS
	@printf '%s\n' \
	  $(call unix_shell_quote,CC=$(CC)) \
	  $(call unix_shell_quote,CPPFLAGS=$(CPPFLAGS) $(BUILD_CPPFLAGS)) \
	  $(call unix_shell_quote,CFLAGS=$(CFLAGS) $(PORT_CFLAGS)) \
	  $(call unix_shell_quote,LDFLAGS=$(LDFLAGS)) \
	  $(call unix_shell_quote,LDLIBS=$(LDLIBS)) > $@.tmp
	@if test -f $@ && cmp -s $@.tmp $@; then rm -f $@.tmp; else mv $@.tmp $@; fi

$(OBJS): .build-options Makefile.sources rogue.h jec/language.h jec/unicode_text.h jec/game_text.h
%.o: %.c
	$(CC) $(CPPFLAGS) $(BUILD_CPPFLAGS) $(CFLAGS) $(PORT_CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(PROG) rogue .build-options .build-options.tmp

# Keep the selected names/paths when using the language targets.
original english:
	$(MAKE) clean PROG="$(PROG)"
	$(MAKE) all PROG="$(PROG)" DEFAULT_LANGUAGE=2

japanese:
	$(MAKE) clean PROG="$(PROG)"
	$(MAKE) all PROG="$(PROG)" DEFAULT_LANGUAGE=1

endif # Windows versus Unix
