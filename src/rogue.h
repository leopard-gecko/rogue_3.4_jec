#ifndef ROGUE34_H
#define ROGUE34_H

/* Public curses types (WINDOW and bool) are used by this header. */
#include <curses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#else
#include "windows/rogue_windows.h"
/* PDCursesMod's VT backend sets a blinking block cursor when endwin()
 * closes its screen.  Route every game's endwin() through a Windows-only
 * wrapper that restores the terminal's configured default cursor shape
 * AFTER the backend's shutdown.  The wrapper's source intentionally does
 * not include rogue.h, so it calls the real curses endwin() unchanged.
 * PDCursesMod's curses.h renames endwin to an ABI-specific symbol via a
 * macro (e.g. endwin_u64_4400).  Remove that alias before installing our
 * game-only wrapper; otherwise GCC warns about a macro redefinition. */
#ifdef endwin
#undef endwin
#endif
#define endwin() rogue_windows_endwin()
#endif
#include <time.h>
/* The Windows VT backend needs a supplementary-plane Unicode text shim.
 * Intercept curses text only in the Windows compilation unit; native
 * macOS/Linux calls and game rules are unchanged. The renderer itself
 * deliberately does not include rogue.h, avoiding recursive wrapping. */
#ifdef _WIN32
#include "jec/unicode_output.h"
#ifdef waddstr
#undef waddstr
#endif
#ifdef mvwaddstr
#undef mvwaddstr
#endif
#ifdef wprintw
#undef wprintw
#endif
#ifdef printw
#undef printw
#endif
#ifdef addstr
#undef addstr
#endif
#ifdef mvaddstr
#undef mvaddstr
#endif
#ifdef wclear
#undef wclear
#endif
#ifdef wclrtoeol
#undef wclrtoeol
#endif
#define waddstr(win, s) rogue_waddstr_utf8((win), (s))
#define mvwaddstr(win, y, x, s) rogue_mvwaddstr_utf8((win), (y), (x), (s))
#define addstr(s) rogue_waddstr_utf8(stdscr, (s))
#define mvaddstr(y, x, s) rogue_mvwaddstr_utf8(stdscr, (y), (x), (s))
#define wprintw(win, ...) rogue_wprintw_utf8((win), __VA_ARGS__)
#define printw(...) rogue_wprintw_utf8(stdscr, __VA_ARGS__)
#define wclear(win) rogue_wclear_utf8((win))
#define wclrtoeol(win) rogue_wclrtoeol_utf8((win))
#endif

/*
 * Rogue definitions and variable declarations
 *
 * @(#)rogue.h	X.XX (Berkeley) XX/XX/XX
 */

/*
 * Maximum number of different things
 */
#define ROGUE_LINES 24
#define ROGUE_COLS 80
#define ROGUE_PATH_CAP 512
#define MAXROOMS 9
#define MAXOBJ 9
#define MAXPACK 22
#define MAXTRAPS 10

/*
 * return values for get functions
 */
#define	NORM	0	/* normal exit */
#define	QUIT	1	/* quit option setting */
#define	MINUS	2	/* back up one option */

/*
 * All the fun defines
 */
#define next(ptr) (*ptr).l_next
#define prev(ptr) (*ptr).l_prev
#define ldata(ptr) (*ptr).l_data
#define inroom(rp, cp) (\
    (rp)->r_pos.x <= (cp)->x && (cp)->x <= (rp)->r_pos.x + (rp)->r_max.x - 1 \
 && (rp)->r_pos.y <= (cp)->y && (cp)->y <= (rp)->r_pos.y + (rp)->r_max.y - 1)
#define winat(y, x) rogue_glyph(mvwinch(mw,y,x)==' ' ? mvwinch(stdscr,y,x) : winch(mw))
#define debug if (wizard) msg
#define RN (((seed = seed * 11109u + 13849u) & 0x7fffu) >> 1)
#define unc(cp) (cp).y, (cp).x
#define cmov(xy) move((xy).y, (xy).x)
#define DISTANCE(y1, x1, y2, x2) ((x2 - x1)*(x2 - x1) + (y2 - y1)*(y2 - y1))
#define when break;case
#define otherwise break;default
#define until(expr) while(!(expr))
#define ce(a, b) ((a).x == (b).x && (a).y == (b).y)
#define draw(window) rogue_draw(window)
#define hero player.t_pos
#define pstats player.t_stats
#define pack player.t_pack
#define attach(a,b) _attach(&a,b)
#define detach(a,b) _detach(&a,b)
#define free_list(a) _free_list(&a)
#define max(a, b) ((a) > (b) ? (a) : (b))
#define on(thing, flag) (((thing).t_flags & flag) != 0)
#define off(thing, flag) (((thing).t_flags & flag) == 0)
#ifndef CTRL
#define CTRL(ch) ((ch) & 037)
#endif
#define ALLOC(x) calloc(1, (unsigned int) (x))
#define FREE(x) free((void *) (x))
#define GOLDCALC (rnd(50 + 10 * level) + 2)

/*
 * Things that appear on the screens
 */
#define PASSAGE '#'
#define DOOR '+'
#define FLOOR '.'
#define PLAYER '@'
#define TRAP '^'
#define TRAPDOOR '>'
#define ARROWTRAP '{'
#define SLEEPTRAP '$'
#define BEARTRAP '}'
#define TELTRAP '~'
#define DARTTRAP '`'
#define SECRETDOOR '&'
#define STAIRS '%'
#define GOLD '*'
#define POTION '!'
#define SCROLL '?'
#define MAGIC '$'
#define FOOD ':'
#define WEAPON ')'
#define ARMOR ']'
#define AMULET ','

int wizard_password_ok(const char *plain);

/*
 * Various constants
 */
#define	PASSWD "mTXoY2LoFzRmY"
#define BEARTIME 3
#define SLEEPTIME 5
#define HOLDTIME 2
#define WANDERTIME 70
#define BEFORE 1
#define AFTER 2
#define HUHDURATION 20
#define SEEDURATION 150
#define HUNGERTIME 1000
#define MORETIME 150
#define ESCAPE 27

/*
 * Save against things
 */
#define VS_POISON 00
#define VS_MAGIC  03

/*
 * Various flag bits
 */
#define ISDARK	0000001
#define ISCURSED 000001
#define ISGONE	0000002
#define ISKNOW  0000002
#define ISRUN	0000004
#define ISFOUND 0000010
#define ISINVIS 0000020
#define ISMEAN  0000040
#define ISGREED 0000100
#define ISBLOCK 0000200
#define ISHELD  0000400
#define ISHUH   0001000
#define ISREGEN 0002000
#define CANHUH  0004000
#define CANSEE  0010000
#define ISMISL  0020000
#define ISMANY  0040000
#define ISHASTE 0100000

/*
 * Potion types
 */
#define P_CONFUSE 0
#define P_PARALYZE 1
#define P_POISON 2
#define P_STRENGTH 3
#define P_SEEINVIS 4
#define P_HEALING 5
#define P_MFIND 6
#define P_TFIND 7
#define P_RAISE 8
#define P_XHEAL 9
#define P_HASTE 10
#define P_RESTORE 11
#define MAXPOTIONS 12

/*
 * Scroll types
 */
#define S_CONFUSE 0
#define S_MAP 1
#define S_LIGHT 2
#define S_HOLD 3
#define S_SLEEP 4
#define S_ARMOR 5
#define S_IDENT 6
#define S_SCARE 7
#define S_GFIND 8
#define S_TELEP 9
#define S_ENCH 10
#define S_CREATE 11
#define S_REMOVE 12
#define S_AGGR 13
#define MAXSCROLLS 14

/*
 * Weapon types
 */
#define MACE 0
#define SWORD 1
#define BOW 2
#define ARROW 3
#define DAGGER 4
#define ROCK 5
#define TWOSWORD 6
#define SLING 7
#define DART 8
#define CROSSBOW 9
#define BOLT 10
#define SPEAR 11
#define MAXWEAPONS 12

/*
 * Armor types
 */
#define LEATHER 0
#define RING_MAIL 1
#define STUDDED_LEATHER 2
#define SCALE_MAIL 3
#define CHAIN_MAIL 4
#define SPLINT_MAIL 5
#define BANDED_MAIL 6
#define PLATE_MAIL 7
#define MAXARMORS 8

/*
 * Now we define the structures and types
 */

/*
 * Help list
 */

struct h_list {
    char h_ch;
    char *h_desc;
};
extern struct h_list helpstr[];

/*
 * Coordinate data type
 */
typedef struct {
    int x;
    int y;
} coord;

typedef struct {
    short st_str;
    short st_add;
} str_t;

/*
 * Linked list data type
 */
/* Portable save identifiers refer to these callbacks, never raw addresses. */
struct daemon_action {
    int (*d_func)(void);
    int d_arg;
    char d_type;
};
struct fuse_action {
    int (*d_func)(void);
    int d_arg;
    int d_time;
    char f_type;
};

struct linked_list {
    struct linked_list *l_next;
    struct linked_list *l_prev;
    char *l_data;			/* Various structure pointers */
};

/*
 * Stuff about magic items
 */

struct magic_item {
    char *mi_name;
    int mi_prob;
};

/*
 * Room structure
 */
struct room {
    coord r_pos;			/* Upper left corner */
    coord r_max;			/* Size of room */
    coord r_gold;			/* Where the gold is */
    int r_goldval;			/* How much the gold is worth */
    int r_flags;			/* Info about the room */
    int r_nexits;			/* Number of exits */
    coord r_exit[4];			/* Where the exits are */
};

/*
 * Array of all traps on this level
 */
struct trap {
    coord tr_pos;			/* Where trap is */
    char tr_type;			/* What kind of trap */
    int tr_flags;			/* Info about trap (i.e. ISFOUND) */
};
extern struct trap traps[MAXTRAPS];

/*
 * Structure describing a fighting being
 */
struct stats {
    str_t s_str;			/* Strength */
    long s_exp;				/* Experience */
    int s_lvl;				/* Level of mastery */
    int s_arm;				/* Armor class */
    int s_hpt;				/* Hit points */
    char *s_dmg;			/* String describing damage done */
};

/*
 * Structure for monsters and player
 */
struct thing {
    coord t_pos;			/* Position */
    char t_type;			/* What it is */
    char t_disguise;			/* What mimic looks like */
    coord *t_dest;			/* Where it is running to */
    short t_flags;			/* State word */
    struct stats t_stats;		/* Physical description */
    struct linked_list *t_pack;		/* What the thing is carrying */
};

/*
 * Array containing information on all the various types of mosnters
 */
struct monster {
    char *m_name;			/* What to call the monster */
    short m_flags;			/* Things about the monster */
    struct stats m_stats;		/* Initial stats */
};

/*
 * Structure for a thing that the rogue can carry
 */

struct object {
    int o_type;				/* What kind of object it is */
    coord o_pos;			/* Where it lives on the screen */
    char *o_text;			/* What it says if you read it */
    char o_launch;			/* What you need to launch it */
    char *o_damage;			/* Damage if used like sword */
    char *o_hurldmg;			/* Damage if thrown */
    int o_count;			/* Count for plural objects */
    int o_which;			/* Which object of a type it is */
    int o_hplus;			/* Plusses to hit */
    int o_dplus;			/* Plusses to damage */
    int o_ac;				/* Armor class */
    int o_flags;			/* Information about objects */
};

/*
 * Now all the global variables
 */

extern struct room rooms[MAXROOMS];		/* One for each room -- A level */
extern struct linked_list *mlist;		/* List of monsters on the level */
extern struct linked_list *f_list;		/* List of active fuses */
extern struct linked_list *d_list;	/* List of active daemons */
extern struct thing player;			/* The rogue */
extern struct stats max_stats;			/* The maximum for the player */
extern struct monster monsters[26];		/* The initial monster states */
extern struct linked_list *lvl_obj;		/* List of objects on this level */
extern struct object *cur_weapon;		/* Which weapon he is weilding */
extern struct object *cur_armor;		/* What a well dresssed rogue wears */
extern struct magic_item s_magic[MAXSCROLLS];	/* Names and chances for scrolls */
extern struct magic_item p_magic[MAXPOTIONS];	/* Names and chances for potions */
extern int level;				/* What level rogue is on */
extern int purse;				/* How much gold the rogue has */
extern int mpos;				/* Where cursor is on top line */
extern int ntraps;				/* Number of traps on this level */
extern int no_move;				/* Number of turns held in place */
extern int no_command;				/* Number of turns asleep */
extern int inpack;				/* Number of things in pack */
extern int max_hp;				/* Player's max hit points */
extern int a_chances[MAXARMORS];		/* Probabilities for armor */
extern int a_class[MAXARMORS];			/* Armor class for various armors */
extern int lastscore;				/* Score before this turn */
extern int total;				/* Total dynamic memory chunks */
extern int no_food;				/* Number of levels without food */
extern unsigned int seed;				/* Random number seed */
extern int dnum;				/* Dungeon number (display copy of seed) */
extern int count;				/* Number of times to repeat command */
extern int fung_hit;				/* Number of time fungi has hit */
extern int quiet;				/* Number of quiet turns */
extern int max_level;				/* Deepest player has gone */
extern char take;				/* Thing the rogue is taking */
extern char prbuf[512];				/* Buffer for sprintfs */
extern char outbuf[BUFSIZ];			/* Output buffer for stdout */
extern char runch;				/* Direction player is running */
extern char *s_names[MAXSCROLLS];		/* Names of the scrolls */
extern char *p_colors[MAXPOTIONS];		/* Colors of the potions */
extern char *w_names[MAXWEAPONS];		/* Names of the various weapons */
extern char *a_names[MAXARMORS];		/* Names of armor types */
extern char *release;				/* Release number of rogue */
extern char whoami[80];			/* Name of player */
extern char fruit[80];				/* Favorite fruit */
extern char huh[80];				/* The last message printed */
extern char *s_guess[MAXSCROLLS];		/* Players guess at what scroll is */
extern char *p_guess[MAXPOTIONS];		/* Players guess at what potion is */
extern char file_name[ROGUE_PATH_CAP];			/* Save file name */
extern char home[ROGUE_PATH_CAP];				/* User's home directory */
extern WINDOW *cw;				/* Window that the player sees */
extern WINDOW *hw;				/* Used for the help command */
extern WINDOW *mw;				/* Used to store mosnters */
extern bool running;				/* True if player is running */
extern bool playing;				/* True until he quits */
extern bool wizard;				/* True if allows wizard commands */
extern bool after;				/* True if we want after daemons */
extern bool notify;				/* True if player wants to know */
extern bool fight_flush;			/* True if toilet input */
extern bool terse;				/* True if we should be short */
extern bool door_stop;				/* Stop running when we pass a door */
extern bool jump;				/* Show running as series of jumps */
extern bool slow_invent;			/* Inventory one line at a time */
extern bool throwing;				/* Throw direction dispatch flag */
extern bool firstmove;				/* First move after setting door_stop */
extern bool waswizard;				/* Was a wizard sometime */
extern bool askme;				/* Ask about unidentified things */
extern bool s_know[MAXSCROLLS];		/* Does he know what a scroll does */
extern bool p_know[MAXPOTIONS];		/* Does he know what a potion does */
extern bool amulet;				/* He found the amulet */
extern bool in_shell;				/* True if executing a shell */
extern coord oldpos;
extern int rogue_rollwand_between;
extern char rogue_countch, rogue_direction, rogue_newcount;

/* curses cell values are not characters; ignore attributes and ERR. */
char rogue_glyph(chtype cell);

/* Function prototypes for the macOS compatibility build. */
int wear(void);
int take_off(void);
int waste_time(void);
int runners(void);
int do_chase(struct thing *th);
int runto(coord *runner, coord *spot);
int chase(struct thing *tp, coord *ee);
struct room * roomin(coord *cp);
struct linked_list * find_mons(int y, int x);
int diag_ok(coord *sp, coord *ep);
int cansee(int y, int x);
int command(void);
int quit(void);
int search(void);
const char * tr_name(char ch);
int help(void);
int identify(void);
int look(void);
int d_level(void);
int u_level(void);
int secretdoor(int y, int x);
int shell(void);
int passwd(void);
int do_run(char ch);
int start_daemon(int (*func)(void), int arg, int type);
int kill_daemon(int (*func)(void));
int do_daemons(int flag);
int fuse(int (*func)(void), int arg, int time, int type);
int lengthen(int (*func)(void), int xtime);
int extinguish(int (*func)(void));
int do_fuses(int flag);
int doctor(void);
int swander(void);
int rollwand(void);
int unconfuse(void);
int unsee(void);
int hungry(void);
int weak(void);
int starve(void);
int faint(void);
int nohaste(void);
int fight(coord *mp, char mn, struct object *weap, bool thrown);
int attack(struct thing *mp);
int swing(int at_lvl, int op_arm, int wplus);
int check_level(void);
int roll_em(struct stats *att, struct stats *def, struct object *weap, bool hurl);
char * prname(const char *who, bool upper);
int hit(const char *er, const char *ee);
int miss(const char *er, const char *ee);
int save_throw(int which);
int str_plus(str_t *str);
int add_dam(str_t *str);
int raise_level(void);
int thunk(struct object *weap, const char *mname);
int bounce(struct object *weap, const char *mname);
int remove_monster(coord *mp, struct linked_list *item);
int is_magic(struct object *obj);
int init_player(void);
int init_colors(void);
int init_names(void);
int step_ok(int ch);
int readchar(void);
char * rogue_unctrl(int ch);
int status(void);
int wait_for(char ch);
int show_win(WINDOW *scr, const char *message);
int _detach(struct linked_list **list, struct linked_list *item);
int _attach(struct linked_list **list, struct linked_list *item);
int _free_list(struct linked_list **ptr);
int discard(struct linked_list *item);
struct linked_list * new_item(int size);
char * new(int size);
int endit(void);
int fatal(char *s);
int rnd(int range);
int roll(int number, int sides);
int setup(void);
int playit(void);
int tstp(void);
int randmonster(void);
int randwand(void);
int new_monster(struct linked_list *item, char type, coord *cp);
int wanderer(void);
int do_move(int dy, int dx);
int light(coord *cp);
int show(int y, int x);
int be_trapped(coord *tc);
int trap_at(int y, int x);
coord * rndmove(struct thing *who);
int option(void);
void put_bool(void *value);
void put_str(void *value);
int get_bool(void *value, WINDOW *win);
int get_str(void *value, WINDOW *win);
int parse_opts(char *str);
int rogue_set_save_path(const char *path, size_t length);
void rogue_copy_option(char *dest, size_t capacity, const char *source, size_t len);
int do_passages(void);
int conn(char rm_ch, char direc);
int door(struct room *rm, coord *cp);
int quaff(void);
int read_scroll(void);
char * num(int n1, int n2);
int death(char monst);
void score_wait_on_screen(void);
int score(int amount, int flags, char monst);
int total_winner(void);
const char * killname(char monst);
int do_rooms(void);
int draw_room(struct room *rp);
int horiz(int cnt);
int vert(int cnt);
int rnd_pos(struct room *rp, coord *cp);
int rogue_find_floor(coord *cp, int room, bool only_room);


int new_level(void);
int rnd_room(void);
char * inv_name(struct object *obj, bool drop);
int pick_up(char ch);
int money(void);
int add_pack(void);
int put_things(void);
int inventory(struct linked_list *list, int type);
int drop(void);
struct linked_list * find_obj(int y, int x);
struct linked_list * get_item(char *purpose, int type);
int picky_inven(void);
int missile(int ydelta, int xdelta);
int fall(struct linked_list *item, bool pr);
int init_weapon(struct object *weap, char type);
int hit_monster(int y, int x, struct object *obj);
int whatis(void);
int eat(void);
int create_obj(void);
int wield(void);
int chg_str(int amt);
int teleport(void);
void msg(const char *fmt, ...);
int save_game(void);
int restore(char *file, char **envp);
void rogue_restore_fungi(void);
int rogue_build_data_path(char *result, size_t cap,
                          const char *directory, const char *filename);
int rogue_default_save_path(char *result, size_t cap);
const char *rogue_score_path(void);
int rogue_erase_char(void);
int rogue_kill_char(void);
int rogue_install_signal_handlers(void);
void rogue_check_signals(void);
void rogue_resume_screen(void);
void rogue_check_terminal_size(void);
int rogue_draw(WINDOW *window);
/* Visual timing only: no game turn or RNG consumption. */
#define ROGUE_VISUAL_STEP_DELAY_MS 30u
#define ROGUE_TEXT_NOTICE_DELAY_MS 1000u
void rogue_visual_delay(unsigned int milliseconds);



#endif /* ROGUE34_H */
