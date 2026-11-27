/*
 * File for the fun ends
 * Death or a total win
 *
 * @(#)rip.c	X.XX (Berkeley) XX/XX/XX
 */

#include <time.h>
#include <signal.h>
#include <ctype.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#ifdef _WIN32
#include "windows/rogue_windows_files.h"
#endif
#ifndef _WIN32
#include <sys/file.h>
#endif
#include <limits.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#include "rogue.h"
#include "jec/game_text.h"
#include "jec/language.h"
#include "jec/unicode_text.h"

static const char *const rip[] = {
"                       __________",
"                      /          \\",
"                     /    REST    \\",
"                    /      IN      \\",
"                   /     PEACE      \\",
"                  /                  \\",
"                  |                  |",
"                  |                  |",
"                  |   killed by a    |",
"                  |                  |",
"                  |       1980       |",
"                 *|     *  *  *      | *",
"         ________)/\\\\_//(\\/(/\\)/\\//\\/|_)_______",
    0
};

static const char *const rip_ja[] = {
"                       __________",
"                      /          \\",
"                     /            \\",
"                    /              \\",
"                   /                \\",
"                  /                  \\",
"                  |                  |",
"                  |                  |",
"                  |                  |",
"                  |                  |",
"                  |       1980       |",
"                 *|     *  *  *      | *",
"         ________)/\\\\_//(\\/(/\\)/\\//\\/|_)_______",
    0
};


/* Keep epitaphs between the stone's two sloping sides.  The centre
 * moves by one column on each of the upper rows.  Coordinates are
 * terminal cells, never UTF-8 byte positions.  In particular, do not
 * overwrite a wall cell when a Japanese name or monster is long. */
static void
grave_bounds(int row, int *left, int *right)
{
    switch (row) {
        case 9:  *left = 23; *right = 32; break;
        case 10: *left = 22; *right = 33; break;
        case 11: *left = 21; *right = 34; break;
        case 12: *left = 20; *right = 35; break;
        default: *left = 19; *right = 36; break;
    }
}

/* The displayed text is truncated only at complete UTF-8 grapheme
 * boundaries.  Never change whoami or the full killer name used by
 * scores or game state. */
static void
grave_text(int row, const char *text)
{
    int left, right, width, col, i;
    char clipped[80];

    grave_bounds(row, &left, &right);
    width = right - left + 1;
    rogue_text_copy(clipped, sizeof clipped, text,
                    sizeof clipped - 1, width);
    col = left + (width - rogue_text_width(clipped)) / 2;
    move(row, left);
    for (i = 0; i < width; ++i)
        addch(' ');
    mvaddstr(row, col, clipped);
}

/*
 * death:
 *	Do something really fun when he dies
 */
int
death(char monst)
{
    const char *const *dp = jrogue_lang_is_japanese ? rip_ja : rip;
    const char *killer;
    register struct tm *lt;
    time_t date;
    char buf[80];

    time(&date);
    lt = localtime(&date);
    clear();
    move(8, 0);
    while (*dp)
	printw("%s\n", *dp++);
    /* Follow the Rogue 3.6 JEC Japanese epitaph layout.  The English
     * REST / IN / PEACE and "killed by a" are already in rip[]. */
    if (jrogue_lang_is_japanese) {
        grave_text(11, "安らかに");
        grave_text(12, "眠れ");
    }
    grave_text(14, whoami);
    purse -= purse/10;
    snprintf(buf, sizeof buf, JSTR("＄%d", "%d Au"), purse);
    grave_text(15, buf);
    killer = killname(monst);
    if (jrogue_lang_is_japanese) {
        grave_text(16, killer);
        grave_text(17, isupper((unsigned char)monst)
                         ? "と戦いて死す" : "により死す");
    } else {
        grave_text(17, killer);
        /* Keep the historic English "killed by an" spelling. */
        if (*killer == 'a' || *killer == 'e' || *killer == 'i' ||
            *killer == 'o' || *killer == 'u')
            mvaddch(16, 33, 'n');
    }
    /* Replace all four digits, not only the "80" in the 1980 artwork. */
    snprintf(prbuf, sizeof prbuf, "%04d",
             lt != NULL ? lt->tm_year + 1900 : 1980);
    grave_text(18, prbuf);
    /* Keep the tombstone visible on the curses screen until Return.
       endwin() switches modern terminals out of their alternate screen. */
    score_wait_on_screen();
    endwin();
    score(purse, 0, monst);
    exit(0);
}

/*
 * Wait on the curses screen *before* leaving it.  On macOS Terminal and
 * other alternate-screen terminals, stdout prompts after endwin() would
 * appear over the shell's scrollback instead of the tombstone.
 */
void
score_wait_on_screen(void)
{
    const char *prompt = JSTR("［リターンを押してください］",
                               "[Press return to continue]");

    wmove(stdscr, ROGUE_LINES - 1, 0);
    wclrtoeol(stdscr);
    waddstr(stdscr, prompt);
    draw(stdscr);
    wait_for('\n');
}

/*
 * Versioned, fixed-layout scores.  Unlike the historical encrypted struct,
 * this representation is independent of pointer size, padding and byte order.
 * The 1981 gameplay scoring rules (top ten, strict greater-than insertion,
 * no entries after using wizard mode) are unchanged.
 *
 * All entries are parsed before updating.  A damaged or unknown version is
 * never overwritten.  The per-user file is advisory-locked for the entire
 * read/modify/write.  The score path is configured by the build options.
 */
#define SCORE_MAGIC "ROGUE34-SCORES-1\n"
#define SCORE_COUNT 10
#define SCORE_MAX_BYTES 4096

struct score_entry {
    int amount;
    int flags;
    int depth;
    unsigned char monster;
    char name[80];
};

static void
score_printable_name(char *dst, size_t cap, const char *src)
{
    /* Preserve UTF-8 names; score files still hex-encode every byte. */
    rogue_copy_option(dst, cap, src, strlen(src));
}

static int
score_decode_name(const char *hex, char *name)
{
    size_t i, n = strlen(hex);
    if (n == 0 || n % 2 != 0 || n > 158)
        return -1;
    for (i = 0; i < n / 2; i++) {
        unsigned int v;
        char pair[3] = {hex[2*i], hex[2*i+1], 0};
        if (!isxdigit((unsigned char)pair[0]) ||
            !isxdigit((unsigned char)pair[1]) ||
            sscanf(pair, "%2x", &v) != 1 || v < 32 || v == 127 || v > 255)
            return -1;
        name[i] = (char)v;
    }
    name[n/2] = '\0';
    /* Hex encoding was already used for all historic ASCII entries.
     * Accept well-formed Unicode names without changing the score file
     * version or granting control bytes access to the terminal. */
    for (i = 0; i < n / 2;) {
        size_t bytes = rogue_utf8_char_bytes(name + i, n / 2 - i);
        if (bytes == 0) return -1;
        i += bytes;
    }
    return 0;
}

static void
score_encode_name(const char *name, char hex[160])
{
    static const char digits[] = "0123456789abcdef";
    size_t i;
    for (i = 0; name[i] != '\0' && i < 79; i++) {
        unsigned char c = (unsigned char)name[i];
        hex[2*i] = digits[c >> 4];
        hex[2*i + 1] = digits[c & 15];
    }
    hex[2*i] = '\0';
}

/* Return 0 for a valid file (including a new empty file), -1 on corruption. */
static int
score_load(FILE *fp, struct score_entry entries[SCORE_COUNT], long long size)
{
    char line[512];
    int n = 0;
    memset(entries, 0, sizeof(*entries) * SCORE_COUNT);
    if (size == 0)
        return 0;
    if (size > SCORE_MAX_BYTES || !fgets(line, sizeof line, fp) ||
        strcmp(line, SCORE_MAGIC) != 0)
        return -1;
    while (fgets(line, sizeof line, fp)) {
        int amount, flags, depth, used = 0;
        unsigned int monster;
        char hex[160];
        struct score_entry *e;
        if (n >= SCORE_COUNT ||
            sscanf(line, "%d %d %d %u %159s %n",
                   &amount, &flags, &depth, &monster, hex, &used) != 5 ||
            used == 0 || line[used] != '\0' || amount <= 0 ||
            flags < 0 || flags > 2 || depth < 0 || depth > 1000000 ||
            monster > 255)
            return -1;
        e = &entries[n];
        if (score_decode_name(hex, e->name) != 0)
            return -1;
        e->amount = amount;
        e->flags = flags;
        e->depth = depth;
        e->monster = (unsigned char)monster;
        n++;
    }
    return ferror(fp) ? -1 : 0;
}

static int
score_store(FILE *fp, struct score_entry entries[SCORE_COUNT])
{
    int i;
#ifndef _WIN32
    int fd = fileno(fp);
    if (fflush(fp) != 0 || ftruncate(fd, 0) != 0 || fseek(fp, 0, SEEK_SET) != 0)
#else
    if (rogue_windows_score_truncate(fp) != 0)
#endif
        return -1;
    if (fputs(SCORE_MAGIC, fp) == EOF)
        return -1;
    for (i = 0; i < SCORE_COUNT; i++) {
        char hex[160];
        if (entries[i].amount <= 0)
            continue;
        score_encode_name(entries[i].name, hex);
        if (fprintf(fp, "%d %d %d %u %s\n",
                    entries[i].amount, entries[i].flags, entries[i].depth,
                    (unsigned int)entries[i].monster, hex) < 0)
            return -1;
    }
#ifndef _WIN32
    if (fflush(fp) != 0 || fsync(fd) != 0)
#else
    if (rogue_windows_score_flush(fp) != 0)
#endif
        return -1;
    return 0;
}

/*
 * Print game-over scores on a clean normal terminal screen after endwin().
 * The standalone -s command never clears an existing terminal screen, and
 * redirected Unix output must not contain terminal control sequences.
 * Windows retains its direct-launch-only console clearing implementation.
 */
static void
score_clear_plain_screen(int flags)
{
    if (flags == -1)
        return;
#ifdef _WIN32
    rogue_windows_clear_direct_launch_screen();
#else
    if (isatty(fileno(stdout))) {
        fputs("\033[0m\033[H\033[2J", stdout);
        fflush(stdout);
    }
#endif
}

int
score(int amount, int flags, char monst)
{
    static const char *reason[] = {"killed", "quit", "A total winner"};
    struct score_entry top_ten[SCORE_COUNT];
#ifndef _WIN32
    struct stat st;
    int fd = -1;
#else
    long long file_length = 0;
#endif
    int i, j, valid = 1;
    int writing = (flags != -1 && !waswizard && amount > 0);
    int exists = 1;
    FILE *fp = NULL;
    const char *path = rogue_score_path();

    if (path == NULL) {
        fprintf(stderr, "Rogue 3.4: configured score path is too long or invalid.\n");
        return -1;
    }
    /* A read-only -s does not create or rewrite a score file. */
#ifdef _WIN32
    if (rogue_windows_score_open(path, writing, &fp, &file_length, &exists) != 0)
        valid = 0;
    if (valid && exists && score_load(fp, top_ten, file_length) != 0)
        valid = 0;
    if (valid && !exists)
        memset(top_ten, 0, sizeof top_ten);
#else
    fd = open(path, writing ? (O_RDWR | O_CREAT | O_NOFOLLOW) : (O_RDONLY | O_NOFOLLOW), 0600);
    if (fd < 0 && errno == ENOENT && !writing)
        exists = 0;
    else if (fd < 0)
        valid = 0;
    if (valid && exists) {
        /* Check size only *after* acquiring the lock; another score writer
           may have changed the file while we waited for the lock. */
        if (flock(fd, writing ? LOCK_EX : LOCK_SH) != 0 ||
            fstat(fd, &st) != 0 || !S_ISREG(st.st_mode))
            valid = 0;
        else {
            fp = fdopen(fd, writing ? "r+" : "r");
            if (fp == NULL || score_load(fp, top_ten, st.st_size) != 0)
                valid = 0;
        }
    } else if (valid) {
        memset(top_ten, 0, sizeof top_ten);
    }
#endif
    if (valid && writing && fp) {
        for (i = 0; i < SCORE_COUNT; i++)
            if (amount > top_ten[i].amount)
                break;
        if (i < SCORE_COUNT) {
            for (j = SCORE_COUNT - 1; j > i; j--)
                top_ten[j] = top_ten[j-1];
            top_ten[i].amount = amount;
            top_ten[i].flags = flags;
            top_ten[i].depth = flags == 2 ? max_level : level;
            top_ten[i].monster = (unsigned char)monst;
            score_printable_name(top_ten[i].name, sizeof top_ten[i].name, whoami);
            if (top_ten[i].name[0] == '\0')
                strcpy(top_ten[i].name, "Unknown");
            if (score_store(fp, top_ten) != 0)
                valid = 0;
        }
    }
    /* Finish all file access before waiting for the player's input. */
#ifdef _WIN32
    rogue_windows_score_close(fp);
#else
    if (fp) fclose(fp);
    else if (fd >= 0) close(fd);
#endif

    /* All callers have already ended curses (or have not started it for -s).
       Clear the ordinary terminal screen before printing the scoreboard. */
    score_clear_plain_screen(flags);
    if (!valid) {
        fprintf(stderr, "\nCannot read or update the Rogue 3.4 scores at %s.\n"
                "The file was left unchanged if its format is unknown.\n", path);
#ifdef _WIN32
        rogue_windows_wait_for_score_exit(JSTR(
            "［リターンを押すと終了します］", "[Press Enter to exit]"));
#endif
        return -1;
    }
    /* Match the original: no ranking header when -s is used before any
     * score file exists.  Never create an empty file for a read-only -s. */
    if (!exists && !writing) {
#ifdef _WIN32
        rogue_windows_wait_for_score_exit(JSTR(
            "［リターンを押すと終了します］", "[Press Enter to exit]"));
#endif
        return 0;
    }
    printf("%s", JSTR("\nトップ10の冒険者:\n    得点 名前\n",
                         "\nTop Ten Adventurers:\nRank\tScore\tName\n"));
    for (i = 0; i < SCORE_COUNT; i++) {
        const struct score_entry *e = &top_ten[i];
        const char *killer;
        if (e->amount <= 0)
            continue;
        if (jrogue_lang_is_japanese) {
            if (e->flags == 2)
                printf("%2d %5d %s: 完全勝利者。", i + 1, e->amount, e->name);
            else if (e->flags == 1)
                printf("%2d %5d %s: 地下%d階にて逃亡す。", i + 1,
                       e->amount, e->name, e->depth);
            else
                printf("%2d %5d %s: 地下%d階にて", i + 1,
                       e->amount, e->name, e->depth);
        } else {
            printf("%d\t%d\t%s: %s on level %d", i+1, e->amount,
                   e->name, reason[e->flags], e->depth);
        }
        if (e->flags == 0) {
            killer = killname((char)e->monster);
            if (jrogue_lang_is_japanese)
                printf("%s%s。", killer,
                    e->monster >= 'A' && e->monster <= 'Z'
                    ? "と戦いて死す" : "により死す");
            else
                printf(" by a%s %s", strchr("aeiou", *killer) ? "n" : "", killer);
        }
        printf("%s\n", jrogue_lang_is_japanese ? "" : ".");
    }
    fflush(stdout);
#ifdef _WIN32
    rogue_windows_wait_for_score_exit(JSTR(
        "［リターンを押すと終了します］", "[Press Enter to exit]"));
#endif
    return 0;
}

int
total_winner(void)
{
    register struct linked_list *item;
    register struct object *obj;
    register int worth;

    for (item = pack; item != NULL; item = next(item))
    {
	obj = (struct object *) ldata(item);
	switch (obj->o_type)
	{
	    when FOOD:
		purse += 2;
		goto price_weapon; /* Preserve the historical FOOD fallthrough. */
	    case WEAPON:
	    price_weapon:
		switch (obj->o_which)
		{
		    when MACE: worth = 8;
		    when SWORD: worth = 15;
		    when BOW: worth = 75;
		    when ARROW: worth = 1;
		    when DAGGER: worth = 2;
		    when ROCK: worth = 1;
		    when TWOSWORD: worth = 30;
		    when SLING: worth = 1;
		    when DART: worth = 1;
		    when CROSSBOW: worth = 15;
		    when BOLT: worth = 1;
		    when SPEAR: worth = 2;
		    otherwise: worth = 0;
		}
		purse += worth * (1 + (10 * (obj->o_dplus + obj->o_hplus)));
	    when ARMOR:
		switch (obj->o_which)
		{
		    when LEATHER: worth = 5;
		    when RING_MAIL: worth = 30;
		    when STUDDED_LEATHER: worth = 15;
		    when SCALE_MAIL: worth = 3;
		    when CHAIN_MAIL: worth = 75;
		    when SPLINT_MAIL: worth = 80;
		    when BANDED_MAIL: worth = 90;
		    when PLATE_MAIL: worth = 400;
		    otherwise: worth = 0;
		}
		purse += worth * (1 + (10 * (a_class[obj->o_which] - obj->o_ac)));
	    when POTION:
	    case SCROLL:
		purse += rnd(100) + 50;
	    when AMULET:
		purse += 1000;
	}
    }
    wclear(cw);
    wmove(cw, ROGUE_LINES-1, 0);
    draw(cw);
    endwin();
    printf("%s", JSTR("おめでとう！ 君はようやく太陽の光のもとにたどりついた。\n",
                       "Congratulations, you have made it to the light of day!\n"));
    printf("%s", JSTR("そして、運命の洞窟から生きて戻った精鋭の一人となった。\n",
                       "You have joined the elite ranks of those who have escaped\n"));
    printf("%s", JSTR("君は故郷へ帰り、すべての戦利品を売って大金を手に入れ、\n",
                       "the Dungeons of Doom alive.  You journey home and sell all your\n"));
    printf("%s", JSTR("戦士ギルドに迎え入れられたということである・・・。\n",
                       "loot at a great profit and are admitted to the fighters guild.\n"));
    printf("%s", JSTR("［リターンを押してください］", "[Press return to continue]"));
    fflush(stdout);
#ifdef _WIN32
    rogue_windows_wait_for_plain_return();
#else
    wait_for('\n');
#endif
    score(purse, 2, 0);
    exit(0);
}

const char *
killname(char monst)
{
    if (monst >= 'A' && monst <= 'Z')
	return jrogue_text(monsters[monst-'A'].m_name);
    else if (monst == 'a')
	return jrogue_text("arrow");
    else if (monst == 'd')
        return JSTR("毒投げ矢", "dart");
    else
        return jrogue_text("dart");
}
