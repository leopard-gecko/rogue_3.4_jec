/*
 * global variable initializaton
 *
 * @(#)init.c	X.XX (Berkeley) XX/XX/XX
 */

#include <ctype.h>
#include "rogue.h"
#include "jec/language.h"
#include "jec/game_text.h"

bool playing = TRUE, running = FALSE, wizard = FALSE;
bool notify = TRUE, fight_flush = FALSE, terse = FALSE, door_stop = FALSE;
bool jump = FALSE, slow_invent = FALSE, throwing = FALSE, firstmove = FALSE, askme = FALSE;
bool amulet = TRUE, in_shell = FALSE;
struct linked_list *lvl_obj = NULL, *mlist = NULL;
struct linked_list *f_list = NULL, *d_list = NULL;
struct object *cur_weapon = NULL;
int mpos = 0, no_move = 0, no_command = 0, level = 1, purse = 0, inpack = 0;
int total = 0, no_food = 0, count = 0, fung_hit = 0, quiet = 0;

#define ___ 1
#define _x {1,1}
struct monster monsters[26] = {
	/* Name		 FLAG		str, exp, lvl, amr, hpt, dmg */
	{ "giant ant",	 ISMEAN,	{ _x, 10,   2,   3, ___, "1d6" } },
	{ "bat",	 0,		{ _x,  3,   1,   6, ___, "1d2" } },
	{ "centaur",	 0,		{ _x, 10,   4,   4, ___, "1d6/1d6" } },
	{ "dragon",	 ISGREED,	{ _x,9000, 10,  -1, ___, "1d8/1d8/3d10" } },
	{ "floating eye",0,		{ _x,  5,   1,   9, ___, "0d0" } },
	{ "violet fungi",ISMEAN,	{ _x, 85,   8,   3, ___, "000d0" } },
	{ "gnome",	 0,		{ _x,  8,   1,   5, ___, "1d6" } },
	{ "hobgoblin",	 ISMEAN,	{ _x,  3,   1,   5, ___, "1d8" } },
	{ "invisible stalker",ISINVIS,	{ _x,120,   8,   3, ___, "4d4" } },
	{ "jackal",	 ISMEAN,	{ _x,  2,   1,   7, ___, "1d2" } },
	{ "kobold",	 ISMEAN,	{ _x,  1,   1,   7, ___, "1d4" } },
	{ "leprechaun",	 0,		{ _x, 10,   3,   8, ___, "1d1" } },
	{ "mimic",	 0,		{ _x,140,   7,   7, ___, "3d4" } },
	{ "nymph",	 0,		{ _x, 40,   3,   9, ___, "0d0" } },
	{ "orc",	 ISBLOCK,	{ _x,  5,   1,   6, ___, "1d8" } },
	{ "purple worm", 0,		{ _x,7000, 15,   6, ___, "2d12/2d4" } },
	{ "quasit",	 ISMEAN,	{ _x, 35,   3,   2, ___, "1d2/1d2/1d4" } },
	{ "rust monster",ISMEAN,	{ _x, 20,   5,   2, ___, "0d0/0d0" } },
	{ "snake",	 ISMEAN,	{ _x,  3,   1,   5, ___, "1d3" } },
	{ "troll",	 ISREGEN|ISMEAN,{ _x, 55,   6,   4, ___, "1d8/1d8/2d6" } },
	{ "umber hulk",	 ISMEAN,	{ _x,130,   8,   2, ___, "3d4/3d4/2d5" } },
	{ "vampire",	 ISREGEN|ISMEAN,{ _x,380,   8,   1, ___, "1d10" } },
	{ "wraith",	 0,		{ _x, 55,   5,   4, ___, "1d6" } },
	{ "xorn",	 ISMEAN,	{ _x,120,   7,  -2, ___, "1d3/1d3/1d3/4d6" } },
	{ "yeti",	 0,		{ _x, 50,   4,   6, ___, "1d6/1d6" } },
	{ "zombie",	 ISMEAN,	{ _x,  7,   2,   8, ___, "1d8" } }
};
#undef ___

/*
 * init_player:
 *	roll up the rogue
 */
int
init_player(void)
{
    pstats.s_lvl = 1;
    pstats.s_exp = 0L;
    max_hp = pstats.s_hpt = 12;
    if (rnd(100) == 7)
    {
	pstats.s_str.st_str = 18;
	pstats.s_str.st_add = rnd(100) + 1;
    }
    else
    {
	pstats.s_str.st_str = 16;
	pstats.s_str.st_add = 0;
    }
    pstats.s_dmg = "1d4";
    pstats.s_arm = 10;
    max_stats = pstats;
    pack = NULL;
    return 0;
}

/*
 * Contains defintions and functions for dealing with things like
 * potions and scrolls
 */

static char rainbow[][16] = {
    "Red",
    "Blue",
    "Green",
    "Yellow",
    "Black",
    "Brown",
    "Orange",
    "Pink",
    "Purple",
    "Grey",
    "White",
    "Silver",
    "Gold",
    "Violet",
    "Clear",
    "Vermilion",
    "Ecru",
    "Turquoise",
    "Magenta",
    "Amber",
    "Topaz",
    "Plaid",
    "Tan",
};

#define NCOLORS (sizeof rainbow / sizeof rainbow[0])

static char *sylls[] = {
    "a", "ab", "ag", "aks", "ala", "an", "ankh", "app", "arg", "arze",
    "ash", "ban", "bar", "bat", "bek", "bie", "bin", "bit", "bjor",
    "blu", "bot", "bu", "byt", "comp", "con", "cos", "cre", "dalf",
    "dan", "den", "do", "e", "eep", "el", "eng", "er", "ere", "erk",
    "esh", "evs", "fa", "fid", "for", "fri", "fu", "gan", "gar",
    "glen", "gop", "gre", "ha", "he", "hyd", "i", "ing", "ion", "ip",
    "ish", "it", "ite", "iv", "jo", "kho", "kli", "klis", "la", "lech",
    "man", "mar", "me", "mi", "mic", "mik", "mon", "mung", "mur",
    "nej", "nelg", "nep", "ner", "nes", "nes", "nih", "nin", "o", "od",
    "ood", "org", "orn", "ox", "oxy", "pay", "pet", "ple", "plu", "po",
    "pot", "prok", "re", "rea", "rhov", "ri", "ro", "rog", "rok", "rol",
    "sa", "san", "sat", "see", "sef", "seh", "shu", "ski", "sna",
    "sne", "snik", "sno", "so", "sol", "sri", "sta", "sun", "ta",
    "tab", "tem", "ther", "ti", "tox", "trol", "tue", "turs", "u",
    "ulk", "um", "un", "uni", "ur", "val", "viv", "vly", "vom", "wah",
    "wed", "werg", "wex", "whon", "wun", "xo", "y", "yot", "yu",
    "zant", "zap", "zeb", "zim", "zok", "zon", "zum",
};

struct magic_item s_magic[MAXSCROLLS] = {
    { "monster confusion",	 8 },
    { "magic mapping",		13 },
    { "light",			24 },
    { "hold monster",		26 },
    { "sleep",			31 },
    { "gain armor",		39 },
    { "identify",		59 },
    { "scare monster",		63 },
    { "gold detection",		67 },
    { "teleportation",		73 },
    { "enchant a weapon",		83 },
    { "create a monster",		87 },
    { "remove curse",		97 },
    { "aggravate monsters",	100 },
};

struct magic_item p_magic[MAXPOTIONS] = {
    { "confusion",		 8 },
    { "paralysis",		18 },
    { "poison",			28 },
    { "gain strength",		48 },
    { "see invisible",		49 },
    { "healing",		64 },
    { "monster detection",	69 },
    { "magic detection",	75 },
    { "raise level",		77 },
    { "extra healing",		82 },
    { "haste self",		85 },
    { "restore strength",	100 },
};

int a_class[MAXARMORS] = {
    8,
    7,
    7,
    6,
    5,
    4,
    4,
    3,
};

char *a_names[MAXARMORS] = {
    "leather armor",
    "ring mail",
    "studded leather armor",
    "scale mail",
    "chain mail",
    "splint mail",
    "banded mail",
    "plate mail",
};

int a_chances[MAXARMORS] = {
    20,
    35,
    50,
    63,
    75,
    85,
    95,
    100
};

/*
 * init_colors:
 *	Initialize the potion color scheme for this time
 */
int
init_colors(void)
{
    register int i;
    register char *str;

    for (i = 0; i < MAXPOTIONS; i++)
    {
	do
	    str = rainbow[rnd(NCOLORS)];
	until (isupper(*str));
	*str = tolower(*str);
	p_colors[i] = str;
	p_know[i] = FALSE;
	p_guess[i] = NULL;
    }
    return 0;
}

/*
 * init_names:
 *  Generate scroll titles with the same 40-byte upper bound used by
 *  Rogue 3.6/5.4 JEC, checking BOTH stored English and displayed Japanese.
 *  Keep the original English syllable data and the
 *  original distribution of requested words and syllables.
 */
int
init_names(void)
{
    int i, nwords, nsyl, word;
    char *cp;

    for (i = 0; i < MAXSCROLLS; i++)
    {
        size_t len = 0;
        bool full = FALSE;
        prbuf[0] = '\0';
        cp = prbuf;
        nwords = rnd(4) + 2;
        for (word = 0; word < nwords && !full; word++)
        {
            nsyl = rnd(3) + 1;
            while (nsyl-- > 0)
            {
                const char *sp = sylls[rnd(sizeof sylls / sizeof sylls[0])];
                size_t add = strlen(sp);

                if (len + add > JROGUE_SCROLL_TITLE_MAX_BYTES ||
                    len + add >= sizeof prbuf) {
                    full = TRUE;
                    break;
                }
                memcpy(cp, sp, add);
                cp += add;
                *cp = '\0';
                if (jrogue_scroll_title_ja_bytes(prbuf) >
                    JROGUE_SCROLL_TITLE_MAX_BYTES) {
                    cp -= add;
                    *cp = '\0';
                    full = TRUE;
                    break;
                }
                len += add;
            }
            if (full || word + 1 == nwords)
                break;
            if (len + 1 > JROGUE_SCROLL_TITLE_MAX_BYTES ||
                jrogue_scroll_title_ja_bytes(prbuf) + 1 >
                    JROGUE_SCROLL_TITLE_MAX_BYTES) {
                full = TRUE;
                break;
            }
            *cp++ = ' ';
            *cp = '\0';
            len++;
        }
        /* A title stopped on a byte limit must not end in a blank. */
        if (cp > prbuf && cp[-1] == ' ')
            *--cp = '\0';
        s_names[i] = (char *) new(strlen(prbuf) + 1);
        s_know[i] = FALSE;
        s_guess[i] = NULL;
        strcpy(s_names[i], prbuf);
    }
    return 0;
}

struct h_list helpstr[] = {
    { '?', "	prints help" },
    { '/', "	identify object" },
    { 'h', "	left" },
    { 'j', "	down" },
    { 'k', "	up" },
    { 'l', "	right" },
    { 'y', "	up & left" },
    { 'u', "	up & right" },
    { 'b', "	down & left" },
    { 'n', "	down & right" },
    { 'H', "	run left" },
    { 'J', "	run down" },
    { 'K', "	run up" },
    { 'L', "	run right" },
    { 'Y', "	run up & left" },
    { 'U', "	run up & right" },
    { 'B', "	run down & left" },
    { 'N', "	run down & right" },
    { 't', "<dir>	throw something" },
    { 'f', "<dir>	forward until find something" },
    { '>', "	go down a staircase" },
    { 's', "	search for trap/secret door" },
    { ' ', "	(space) rest for a while" },
    { 'i', "	inventory" },
    { 'I', "	inventory single item" },
    { 'q', "	quaff potion" },
    { 'r', "	read paper" },
    { 'e', "	eat food" },
    { 'w', "	wield a weapon" },
    { 'W', "	wear armor" },
    { 'T', "	take armor off" },
    { 'd', "	drop object" },
    { 'o', "	examine/set options" },
    { CTRL('L'), "	redraw screen" },
    { ESCAPE, "	cancel command" },
    { 'v', "	print program version number" },
    { 'R', "	repeat last message" },
    { '!', "	shell escape" },
    { 'S', "	save game" },
    { 'Q', "	quit" },
    { 0, 0 },
};
