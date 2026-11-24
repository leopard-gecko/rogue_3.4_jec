/* Single definitions for Rogue 3.4 globals (portability). */
#include "rogue.h"

struct room rooms[MAXROOMS];
struct thing player;
struct stats max_stats;
struct object *cur_armor;
int ntraps;
int max_hp;
unsigned int seed;
int dnum;
int max_level;
char take;
char prbuf[512];
char outbuf[BUFSIZ];
char runch;
char *s_names[MAXSCROLLS];
char *p_colors[MAXPOTIONS];
char whoami[80];
char fruit[80];
char huh[80];
char *s_guess[MAXSCROLLS];
char *p_guess[MAXPOTIONS];
char file_name[ROGUE_PATH_CAP];
char home[ROGUE_PATH_CAP];
WINDOW *cw;
WINDOW *hw;
WINDOW *mw;
bool after;
bool waswizard;
bool s_know[MAXSCROLLS];
bool p_know[MAXPOTIONS];
coord oldpos;

struct trap traps[MAXTRAPS];
int lastscore;
