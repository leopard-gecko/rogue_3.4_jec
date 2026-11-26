/*
 * Portable save/restore for Rogue 3.4.
 *
 * R34SAV02, a fixed-width, length-delimited binary format with explicit
 * object identifiers, callbacks and character-only curses screens.  The
 * original raw process-memory save is intentionally not supported.
 *
 * All state is decoded from a bounded, checksummed buffer; no pointers,
 * native structures, FILEs or curses internals are written to disk.
 */
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include "windows/rogue_windows_files.h"
#endif
#ifndef _WIN32
#include <sys/file.h>
#include <unistd.h>
#endif
#include "rogue.h"
#include "jec/language.h"
#include "jec/game_text.h"
#include "jec/unicode_text.h"

/* R34SAV02 codec is identical on Windows, macOS and Linux. */

/* The portable format uses octets and 32-bit game integers.  Never
 * serialize native longs, pointer sizes, or curses WINDOW structures. */
_Static_assert(CHAR_BIT == 8, "R34SAV02 needs eight-bit bytes");
_Static_assert(sizeof(int) == 4, "R34SAV02 needs 32-bit int");
_Static_assert(sizeof(unsigned int) == 4, "Rogue RNG needs 32-bit unsigned int");

#define SAVE_MAGIC "R34SAV02"
#define SAVE_HEAD 16
#define SAVE_LIMIT (1024u * 1024u)
#define LIST_LIMIT 256
#define SCREEN_COUNT 3

struct savebuf {
    unsigned char *mem;
    size_t len, pos;
    int error;
};

static char saved_screen[SCREEN_COUNT][ROGUE_LINES][ROGUE_COLS];
static const struct {
    int (*func)(void);
} actions[] = {
    { NULL }, { doctor }, { runners }, { swander }, { rollwand },
    { hungry }, { weak }, { starve }, { faint }, { unconfuse },
    { unsee }, { nohaste }
};
#define ACTION_COUNT (sizeof actions / sizeof actions[0])

static unsigned int
checksum(const unsigned char *p, size_t n)
{
    uint32_t h = 2166136261u;
    while (n--) {
        h ^= *p++;
        h *= 16777619u;
    }
    return h;
}

static void
put_bytes(struct savebuf *s, const void *p, size_t n)
{
    if (s->error || n > SAVE_LIMIT - s->len) {
        s->error = 1;
        return;
    }
    memcpy(s->mem + s->len, p, n);
    s->len += n;
}

static void
put32(struct savebuf *s, uint32_t n)
{
    unsigned char b[4];
    b[0] = (unsigned char)n;
    b[1] = (unsigned char)(n >> 8);
    b[2] = (unsigned char)(n >> 16);
    b[3] = (unsigned char)(n >> 24);
    put_bytes(s, b, sizeof b);
}

static void
put64(struct savebuf *s, int64_t n)
{
    uint64_t bits = (uint64_t)n;
    put32(s, (uint32_t)bits);
    put32(s, (uint32_t)(bits >> 32));
}

static void
put_int(struct savebuf *s, int n)
{
    put32(s, (uint32_t)(int32_t)n);
}

static void
put_string(struct savebuf *s, const char *v)
{
    size_t n;
    if (v == NULL) {
        put32(s, UINT32_MAX);
        return;
    }
    n = strlen(v);
    if (n > 512) {
        s->error = 1;
        return;
    }
    put32(s, (uint32_t)n);
    put_bytes(s, v, n);
}

static void
get_bytes(struct savebuf *s, void *dest, size_t n)
{
    if (s->error || n > s->len - s->pos) {
        s->error = 1;
        memset(dest, 0, n);
        return;
    }
    memcpy(dest, s->mem + s->pos, n);
    s->pos += n;
}

static uint32_t
get32(struct savebuf *s)
{
    unsigned char b[4];
    get_bytes(s, b, sizeof b);
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) |
           ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

static int
get_int(struct savebuf *s)
{
    return (int)(int32_t)get32(s);
}

static int64_t
get64(struct savebuf *s)
{
    uint64_t lo = get32(s);
    uint64_t hi = get32(s);
    return (int64_t)(lo | (hi << 32));
}

static int
save_get_bool(struct savebuf *s)
{
    int value = get_int(s);
    if (value != 0 && value != 1)
        s->error = 1;
    return value;
}

/* For a restored heap string the process owns a private copy. */
static char *
get_string(struct savebuf *s, size_t maximum)
{
    uint32_t n = get32(s);
    char *v;
    if (n == UINT32_MAX)
        return NULL;
    if (n >= maximum || n > 512 || n > s->len - s->pos) {
        s->error = 1;
        return NULL;
    }
    v = (char *)malloc((size_t)n + 1);
    if (v == NULL) {
        s->error = 1;
        return NULL;
    }
    get_bytes(s, v, n);
    v[n] = '\0';
    if (memchr(v, '\0', n) != NULL)
        s->error = 1;
    return v;
}

static void
get_fixed_string(struct savebuf *s, char *dest, size_t size)
{
    char *v = get_string(s, size);
    if (v == NULL) {
        s->error = 1;
        dest[0] = '\0';
    } else {
        strcpy(dest, v);
        free(v);
    }
}

static void
put_coord(struct savebuf *s, coord v)
{
    put_int(s, v.x);
    put_int(s, v.y);
}

static coord
get_coord(struct savebuf *s)
{
    coord v;
    v.x = get_int(s);
    v.y = get_int(s);
    return v;
}

static int
valid_coord(coord p)
{
    return p.x >= 0 && p.x < ROGUE_COLS &&
           p.y >= 0 && p.y < ROGUE_LINES;
}

/* The recorded positions are used directly as curses and array indices.
 * Check their ranges before ever using a restored game state. */
static int
valid_room(const struct room *r)
{
    int i;
    if (!valid_coord(r->r_pos) || r->r_nexits < 0 || r->r_nexits > 4 ||
        r->r_goldval < 0)
        return 0;
    if (!(r->r_flags & ISGONE) &&
        (r->r_max.x < 4 || r->r_max.y < 4 ||
         r->r_max.x > ROGUE_COLS - r->r_pos.x ||
         r->r_max.y > ROGUE_LINES - r->r_pos.y))
        return 0;
    if (r->r_goldval > 0 && !valid_coord(r->r_gold))
        return 0;
    for (i = 0; i < r->r_nexits; i++)
        if (!valid_coord(r->r_exit[i]))
            return 0;
    return 1;
}

static void
put_stats(struct savebuf *s, const struct stats *st)
{
    put_int(s, st->s_str.st_str);
    put_int(s, st->s_str.st_add);
    put64(s, (int64_t)st->s_exp);
    put_int(s, st->s_lvl);
    put_int(s, st->s_arm);
    put_int(s, st->s_hpt);
}

static void
get_stats(struct savebuf *s, struct stats *st)
{
    int64_t xp;
    int strength = get_int(s), bonus = get_int(s);
    if (strength < SHRT_MIN || strength > SHRT_MAX ||
        bonus < SHRT_MIN || bonus > SHRT_MAX)
        s->error = 1;
    st->s_str.st_str = (short)strength;
    st->s_str.st_add = (short)bonus;
    xp = get64(s);
    if (xp < 0 || xp > LONG_MAX)
        s->error = 1;
    st->s_exp = (long)xp;
    st->s_lvl = get_int(s);
    st->s_arm = get_int(s);
    st->s_hpt = get_int(s);
    if (st->s_lvl < 1 || st->s_lvl > 1000000 ||
        st->s_hpt < -1000000 || st->s_hpt > 1000000)
        s->error = 1;
}

static void
put_object(struct savebuf *s, const struct object *o)
{
    put_int(s, o->o_type);
    put_coord(s, o->o_pos);
    put_int(s, (unsigned char)o->o_launch);
    put_int(s, o->o_count);
    put_int(s, o->o_which);
    put_int(s, o->o_hplus);
    put_int(s, o->o_dplus);
    put_int(s, o->o_ac);
    put_int(s, o->o_flags);
}

static void
get_object(struct savebuf *s, struct object *o)
{
    int type, which, launcher;
    type = get_int(s);
    o->o_pos = get_coord(s);
    launcher = get_int(s);
    o->o_launch = (char)launcher;
    o->o_count = get_int(s);
    which = get_int(s);
    o->o_hplus = get_int(s);
    o->o_dplus = get_int(s);
    o->o_ac = get_int(s);
    o->o_flags = get_int(s);
    if (!valid_coord(o->o_pos) || o->o_count < 0 ||
        launcher < 0 || launcher > UCHAR_MAX ||
        (type == WEAPON && (which < 0 || which >= MAXWEAPONS)) ||
        (type == ARMOR && (which < 0 || which >= MAXARMORS)) ||
        (type == POTION && (which < 0 || which >= MAXPOTIONS)) ||
        (type == SCROLL && (which < 0 || which >= MAXSCROLLS)) ||
        (type == FOOD && (which < 0 || which > 2)) ||
        (type == AMULET && which != 0) ||
        (type != WEAPON && type != ARMOR && type != POTION &&
         type != SCROLL && type != FOOD && type != AMULET)) {
        s->error = 1;
        return;
    }
    o->o_type = type;
    o->o_which = which;
    /* Rebuild stable historical damage-string pointers from the item type. */
    if (type == WEAPON) {
        struct object template_obj;
        unsigned int saved_seed = seed;
        memset(&template_obj, 0, sizeof template_obj);
        /* init_weapon rolls a random stack size for missiles.  Rebuilding
         * damage pointers on restore must not consume random numbers, or
         * the resumed game follows a different random sequence. */
        init_weapon(&template_obj, (char)which);
        seed = saved_seed;
        o->o_damage = template_obj.o_damage;
        o->o_hurldmg = template_obj.o_hurldmg;
    } else {
        o->o_damage = "0d0";
        o->o_hurldmg = "0d0";
    }
}

static int
list_count(struct linked_list *l)
{
    int n = 0;
    for (; l; l = next(l)) {
        if (++n > LIST_LIMIT) return -1;
    }
    return n;
}

static void
put_objects(struct savebuf *s, struct linked_list *l)
{
    int n = list_count(l);
    if (n < 0) {
        s->error = 1;
        return;
    }
    put_int(s, n);
    for (; l; l = next(l))
        put_object(s, (struct object *)ldata(l));
}

static struct linked_list *
get_objects(struct savebuf *s)
{
    struct linked_list *head = NULL, *tail = NULL, *node;
    int n = get_int(s);
    if (n < 0 || n > LIST_LIMIT) {
        s->error = 1;
        return NULL;
    }
    while (n-- > 0 && !s->error) {
        node = new_item(sizeof(struct object));
        get_object(s, (struct object *)ldata(node));
        if (tail != NULL) {
            next(tail) = node;
            prev(node) = tail;
        } else head = node;
        tail = node;
    }
    return head;
}

/* Target zero = no target, one = hero, 2..10 = room gold coordinates. */
static int
dest_id(const coord *p)
{
    int i;
    if (p == NULL) return 0;
    if (p == &hero) return 1;
    for (i = 0; i < MAXROOMS; i++)
        if (p == &rooms[i].r_gold) return 2 + i;
    return -1;
}

static coord *
id_dest(int id)
{
    if (id == 0) return NULL;
    if (id == 1) return &hero;
    if (id >= 2 && id < MAXROOMS + 2) return &rooms[id - 2].r_gold;
    return NULL;
}

static void
save_put_monsters(struct savebuf *s)
{
    struct linked_list *l;
    struct thing *t;
    int n = list_count(mlist);
    if (n < 0) { s->error = 1; return; }
    put_int(s, n);
    for (l = mlist; l; l = next(l)) {
        t = (struct thing *)ldata(l);
        put_coord(s, t->t_pos);
        put_int(s, (unsigned char)t->t_type);
        put_int(s, (unsigned char)t->t_disguise);
        put_int(s, t->t_flags);
        put_stats(s, &t->t_stats);
        {
            int dest = dest_id(t->t_dest);
            if (dest < 0 || ((t->t_flags & ISRUN) != 0 && dest == 0))
                s->error = 1;
            put_int(s, dest);
        }
        put_objects(s, t->t_pack);
    }
}

static struct linked_list *
get_things(struct savebuf *s)
{
    struct linked_list *head = NULL, *tail = NULL, *node;
    struct thing *t;
    int n = get_int(s), dest, type, disguise, flags;
    if (n < 0 || n > LIST_LIMIT) { s->error = 1; return NULL; }
    while (n-- > 0 && !s->error) {
        node = new_item(sizeof(struct thing));
        t = (struct thing *)ldata(node);
        t->t_pos = get_coord(s);
        type = get_int(s);
        disguise = get_int(s);
        flags = get_int(s);
        t->t_type = (char)type;
        t->t_disguise = (char)disguise;
        t->t_flags = (short)flags;
        get_stats(s, &t->t_stats);
        dest = get_int(s);
        t->t_dest = id_dest(dest);
        /* Chase dereferences t_dest directly.  A running monster with no
           destination would be accepted by the checksum but crash when
           its daemon runs after restore.  Non-running monsters may
           legitimately have no destination. */
        if (dest < 0 || dest > MAXROOMS + 1 ||
            ((flags & ISRUN) != 0 && dest == 0) ||
            !valid_coord(t->t_pos) ||
            type < 'A' || type > 'Z' ||
            disguise < 0 || disguise > UCHAR_MAX ||
            flags < SHRT_MIN || flags > SHRT_MAX)
            s->error = 1;
        if (!s->error)
            t->t_stats.s_dmg = monsters[t->t_type - 'A'].m_stats.s_dmg;
        t->t_pack = get_objects(s);
        if (tail) { next(tail) = node; prev(node) = tail; }
        else head = node;
        tail = node;
    }
    return head;
}

static int
func_id(int (*func)(void))
{
    size_t i;
    for (i = 1; i < ACTION_COUNT; i++)
        if (actions[i].func == func) return (int)i;
    return -1;
}

static void
put_events(struct savebuf *s, struct linked_list *l, int timed)
{
    int n = list_count(l);
    if (n < 0) { s->error = 1; return; }
    put_int(s, n);
    for (; l; l = next(l)) {
        if (timed) {
            const struct fuse_action *v = (const struct fuse_action *)ldata(l);
            {
                int id = func_id(v->d_func);
                if (id < 0) s->error = 1;
                put_int(s, id);
            }
            put_int(s, v->d_arg);
            put_int(s, v->d_time);
            put_int(s, v->f_type);
        } else {
            const struct daemon_action *v = (const struct daemon_action *)ldata(l);
            {
                int id = func_id(v->d_func);
                if (id < 0) s->error = 1;
                put_int(s, id);
            }
            put_int(s, v->d_arg);
            put_int(s, v->d_type);
        }
    }
}

static struct linked_list *
get_events(struct savebuf *s, int timed)
{
    struct linked_list *head = NULL, *tail = NULL, *node;
    int n = get_int(s), id, arg, time = 0, type;
    if (n < 0 || n > LIST_LIMIT) { s->error = 1; return NULL; }
    while (n-- > 0 && !s->error) {
        id = get_int(s);
        arg = get_int(s);
        if (timed) time = get_int(s);
        type = get_int(s);
        if (id <= 0 || (size_t)id >= ACTION_COUNT ||
            (type != BEFORE && type != AFTER) ||
            (timed && (time < 1 || time > 100000000))) {
            s->error = 1;
            break;
        }
        node = new_item(timed ? sizeof(struct fuse_action) : sizeof(struct daemon_action));
        if (timed) {
            struct fuse_action *v = (struct fuse_action *)ldata(node);
            v->d_func = actions[id].func;
            v->d_arg = arg;
            v->d_time = time;
            v->f_type = (char)type;
        } else {
            struct daemon_action *v = (struct daemon_action *)ldata(node);
            v->d_func = actions[id].func;
            v->d_arg = arg;
            v->d_type = (char)type;
        }
        if (tail) { next(tail) = node; prev(node) = tail; }
        else head = node;
        tail = node;
    }
    return head;
}

static int
pack_index(struct object *o)
{
    struct linked_list *l;
    int i = 0;
    if (o == NULL) return -1;
    for (l = pack; l; l = next(l), i++)
        if (o == (struct object *)ldata(l)) return i;
    return -2; /* dangling pointer: refuse to create an unusable save */
}

static struct object *
pack_at(int index)
{
    struct linked_list *l;
    for (l = pack; l && index > 0; l = next(l), index--)
        ;
    return index == 0 && l != NULL ? (struct object *)ldata(l) : NULL;
}

static void
put_game(struct savebuf *s)
{
    int i, j, y, x;
    WINDOW *windows[SCREEN_COUNT];
    int values[] = { level, purse, mpos, no_move, no_command, inpack,
        max_hp, lastscore, total, no_food, dnum, count, fung_hit,
        quiet, max_level, (unsigned char)take, (unsigned char)runch,
        rogue_rollwand_between, (unsigned char)rogue_countch,
        (unsigned char)rogue_direction, rogue_newcount, running, playing,
        wizard, after, notify, fight_flush, terse, door_stop, jump,
        slow_invent, throwing, firstmove, waswizard, askme, amulet, in_shell };
    for (i = 0; i < (int)(sizeof values / sizeof values[0]); i++)
        put_int(s, values[i]);
    put32(s, seed);
    put_coord(s, oldpos);
    put_coord(s, hero);
    put_int(s, (unsigned char)player.t_type);
    put_int(s, (unsigned char)player.t_disguise);
    put_int(s, player.t_flags);
    put_stats(s, &pstats);
    put_stats(s, &max_stats);
    put_string(s, whoami);
    put_string(s, fruit);
    put_string(s, huh);
    put_string(s, file_name);
    for (i = 0; i < MAXROOMS; i++) {
        struct room *r = &rooms[i];
        put_coord(s, r->r_pos);
        put_coord(s, r->r_max);
        put_coord(s, r->r_gold);
        put_int(s, r->r_goldval);
        put_int(s, r->r_flags);
        put_int(s, r->r_nexits);
        for (j = 0; j < 4; j++) put_coord(s, r->r_exit[j]);
    }
    put_int(s, ntraps);
    for (i = 0; i < MAXTRAPS; i++) {
        put_coord(s, traps[i].tr_pos);
        put_int(s, (unsigned char)traps[i].tr_type);
        put_int(s, traps[i].tr_flags);
    }
    for (i = 0; i < MAXSCROLLS; i++) {
        put_int(s, s_know[i]);
        put_string(s, s_names[i]);
        put_string(s, s_guess[i]);
    }
    for (i = 0; i < MAXPOTIONS; i++) {
        put_int(s, p_know[i]);
        put_string(s, p_colors[i]);
        put_string(s, p_guess[i]);
    }
    {
        int weapon_id = pack_index(cur_weapon);
        int armor_id = pack_index(cur_armor);
        if (weapon_id < -1 || armor_id < -1) s->error = 1;
        put_int(s, weapon_id);
        put_int(s, armor_id);
    }
    put_objects(s, pack);
    put_objects(s, lvl_obj);
    save_put_monsters(s);
    put_events(s, d_list, 0);
    put_events(s, f_list, 1);
    windows[0] = stdscr;
    windows[1] = cw;
    windows[2] = mw;
    for (i = 0; i < SCREEN_COUNT; i++)
        for (y = 0; y < ROGUE_LINES; y++)
            for (x = 0; x < ROGUE_COLS; x++) {
                char ch = rogue_glyph(mvwinch(windows[i], y, x));
                if (ch == '\0') ch = ' ';
                put_bytes(s, &ch, 1);
            }
}

static void
get_game(struct savebuf *s)
{
    int i, j, y, x, cur_w, cur_a, saved_total;
    int v[37];
    struct room *r;
    WINDOW *windows[SCREEN_COUNT];
    for (i = 0; i < 37; i++) v[i] = get_int(s);
    level = v[0]; purse = v[1]; mpos = v[2]; no_move = v[3];
    no_command = v[4]; inpack = v[5]; max_hp = v[6]; lastscore = v[7];
    saved_total = v[8]; no_food = v[9]; dnum = v[10]; count = v[11];
    fung_hit = v[12]; quiet = v[13]; max_level = v[14];
    take = (char)v[15]; runch = (char)v[16];
    rogue_rollwand_between = v[17];
    rogue_countch = (char)v[18]; rogue_direction = (char)v[19];
    rogue_newcount = (char)v[20]; running = (bool)v[21]; playing = (bool)v[22];
    wizard = (bool)v[23]; after = (bool)v[24]; notify = (bool)v[25];
    fight_flush = (bool)v[26]; terse = (bool)v[27]; door_stop = (bool)v[28];
    jump = (bool)v[29]; slow_invent = (bool)v[30]; throwing = (bool)v[31];
    firstmove = (bool)v[32]; waswizard = (bool)v[33]; askme = (bool)v[34];
    amulet = (bool)v[35]; in_shell = (bool)v[36];
    if (level < 1 || level > 1000000 || max_level < level ||
        inpack < 0 || inpack > MAXPACK || max_hp <= 0 ||
        no_move < 0 || no_command < 0 || fung_hit < 0 ||
        rogue_rollwand_between < 0 || rogue_rollwand_between > 4 ||
        mpos < 0 || mpos >= ROGUE_COLS ||
        v[21] < 0 || v[21] > 1 || v[22] != 1 ||
        v[23] < 0 || v[23] > 1 || v[24] < 0 || v[24] > 1 ||
        v[25] < 0 || v[25] > 1 || v[26] < 0 || v[26] > 1 ||
        v[27] < 0 || v[27] > 1 || v[28] < 0 || v[28] > 1 ||
        v[29] < 0 || v[29] > 1 || v[30] < 0 || v[30] > 1 ||
        v[31] < 0 || v[31] > 1 || v[32] < 0 || v[32] > 1 ||
        v[33] < 0 || v[33] > 1 || v[34] < 0 || v[34] > 1 ||
        v[35] < 0 || v[35] > 1 || v[36] < 0 || v[36] > 1)
        s->error = 1;
    seed = get32(s);
    oldpos = get_coord(s);
    hero = get_coord(s);
    {
        int type = get_int(s), disguise = get_int(s), flags = get_int(s);
        if (type < 0 || type > UCHAR_MAX ||
            disguise < 0 || disguise > UCHAR_MAX ||
            flags < SHRT_MIN || flags > SHRT_MAX)
            s->error = 1;
        player.t_type = (char)type;
        player.t_disguise = (char)disguise;
        player.t_flags = (short)flags;
    }
    get_stats(s, &pstats);
    get_stats(s, &max_stats);
    if (pstats.s_hpt <= 0 || pstats.s_hpt > max_hp ||
        max_stats.s_hpt <= 0 || max_stats.s_hpt > max_hp)
        s->error = 1;
    pstats.s_dmg = "1d4";
    max_stats.s_dmg = "1d4";
    get_fixed_string(s, whoami, sizeof whoami);
    get_fixed_string(s, fruit, sizeof fruit);
    get_fixed_string(s, huh, sizeof huh);
    get_fixed_string(s, file_name, sizeof file_name);
    if (!valid_coord(oldpos) || !valid_coord(hero)) s->error = 1;
    for (i = 0; i < MAXROOMS; i++) {
        r = &rooms[i];
        r->r_pos = get_coord(s);
        r->r_max = get_coord(s);
        r->r_gold = get_coord(s);
        r->r_goldval = get_int(s);
        r->r_flags = get_int(s);
        r->r_nexits = get_int(s);
        for (j = 0; j < 4; j++) r->r_exit[j] = get_coord(s);
        if (!valid_room(r))
            s->error = 1;
    }
    ntraps = get_int(s);
    if (ntraps < 0 || ntraps > MAXTRAPS) s->error = 1;
    for (i = 0; i < MAXTRAPS; i++) {
        traps[i].tr_pos = get_coord(s);
        traps[i].tr_type = (char)get_int(s);
        traps[i].tr_flags = get_int(s);
        if (i < ntraps &&
            (!valid_coord(traps[i].tr_pos) ||
             (traps[i].tr_type != TRAPDOOR &&
              traps[i].tr_type != ARROWTRAP &&
              traps[i].tr_type != SLEEPTRAP &&
              traps[i].tr_type != BEARTRAP &&
              traps[i].tr_type != TELTRAP &&
              traps[i].tr_type != DARTTRAP)))
            s->error = 1;
    }
    for (i = 0; i < MAXSCROLLS; i++) {
        s_know[i] = (bool)save_get_bool(s);
        s_names[i] = get_string(s, JROGUE_SCROLL_TITLE_MAX_BYTES + 1);
        s_guess[i] = get_string(s, 80);
        if (s_names[i] == NULL ||
            jrogue_scroll_title_ja_bytes(s_names[i]) > JROGUE_SCROLL_TITLE_MAX_BYTES)
            s->error = 1;
    }
    for (i = 0; i < MAXPOTIONS; i++) {
        p_know[i] = (bool)save_get_bool(s);
        p_colors[i] = get_string(s, 80);
        p_guess[i] = get_string(s, 80);
        if (p_colors[i] == NULL) s->error = 1;
    }
    cur_w = get_int(s);
    cur_a = get_int(s);
    if (cur_w < -1 || cur_w >= MAXPACK || cur_a < -1 || cur_a >= MAXPACK)
        s->error = 1;
    rogue_restore_fungi();
    pack = get_objects(s);
    lvl_obj = get_objects(s);
    mlist = get_things(s);
    d_list = get_events(s, 0);
    f_list = get_events(s, 1);
    cur_weapon = pack_at(cur_w);
    cur_armor = pack_at(cur_a);
    if (cur_w == -1) cur_weapon = NULL;
    if (cur_a == -1) cur_armor = NULL;
    /* The historical 'w' command may hold any object, including armor.
       pack_at() and get_object() already validate membership and type;
       only worn armor must specifically be ARMOR. */
    if ((cur_w >= 0 && cur_weapon == NULL) ||
        (cur_a >= 0 && (cur_armor == NULL || cur_armor->o_type != ARMOR)) ||
        list_count(pack) != inpack)
        s->error = 1;
    get_bytes(s, saved_screen, sizeof saved_screen);
    if (s->error || s->pos != s->len) { s->error = 1; return; }
    if (LINES < ROGUE_LINES || COLS < ROGUE_COLS) {
        s->error = 1;
        return;
    }
    cw = newwin(ROGUE_LINES, ROGUE_COLS, 0, 0);
    mw = newwin(ROGUE_LINES, ROGUE_COLS, 0, 0);
    hw = newwin(ROGUE_LINES, ROGUE_COLS, 0, 0);
    if (!cw || !mw || !hw) {
        if (hw != NULL) delwin(hw);
        if (mw != NULL) delwin(mw);
        if (cw != NULL) delwin(cw);
        cw = mw = hw = NULL;
        s->error = 1;
        return;
    }
    scrollok(cw, FALSE);
    scrollok(mw, FALSE);
    scrollok(hw, FALSE);
    scrollok(stdscr, FALSE);
    windows[0] = stdscr;
    windows[1] = cw;
    windows[2] = mw;
    for (i = 0; i < SCREEN_COUNT; i++) {
        werase(windows[i]);
        for (y = 0; y < ROGUE_LINES; y++)
            for (x = 0; x < ROGUE_COLS; x++)
                if (saved_screen[i][y][x] != '\0')
                    mvwaddch(windows[i], y, x, (unsigned char)saved_screen[i][y][x]);
    }
    /* The status cache belongs to this newly started process; the first
       playit() iteration forces a correct status redraw if needed. */
    wmove(cw, hero.y, hero.x);
    clearok(cw, TRUE);
    touchwin(cw);
    total = saved_total;
}

#ifndef _WIN32
/* Persist the directory entry after an atomic rename where the filesystem
 * supports directory fsync.  The file itself was already fsynced.  Do not
 * misreport a successful rename as a failed save on filesystems that reject
 * directory fsync (notably some macOS-mounted volumes). */
static void
sync_parent_directory(const char *path)
{
    char dir[sizeof file_name];
    const char *slash = strrchr(path, '/');
    int fd;
    if (slash == NULL) {
        strcpy(dir, ".");
    } else if (slash == path) {
        strcpy(dir, "/");
    } else {
        size_t n = (size_t)(slash - path);
        if (n >= sizeof dir) return;
        memcpy(dir, path, n);
        dir[n] = '\0';
    }
    fd = open(dir, O_RDONLY);
    if (fd < 0) return;
    (void)fsync(fd);
    close(fd);
}

/* Compare the opened save against the path that would be consumed.  A save
 * replaced while it was being read must not cause us to delete the new file. */
static int
same_save_file(const char *path, int fd, const struct stat *original)
{
    struct stat opened, current;
    if (fstat(fd, &opened) != 0 || lstat(path, &current) != 0)
        return 0;
    return S_ISREG(current.st_mode) &&
           opened.st_dev == original->st_dev &&
           opened.st_ino == original->st_ino &&
           opened.st_size == original->st_size &&
           current.st_dev == opened.st_dev &&
           current.st_ino == opened.st_ino &&
           current.st_size == opened.st_size;
}

static int
write_save(const char *path, const unsigned char *data, size_t size)
{
    char temp[sizeof file_name + sizeof ".XXXXXX"];
    FILE *fp;
    int fd, okay = 0;
    if (snprintf(temp, sizeof temp, "%s.XXXXXX", path) >= (int)sizeof temp)
        return -1;
    fd = mkstemp(temp);
    if (fd < 0) return -1;
    fp = fdopen(fd, "wb");
    if (fp != NULL) {
        if (fwrite(data, 1, size, fp) == size &&
            fflush(fp) == 0 && fsync(fd) == 0)
            okay = 1;
        if (fclose(fp) != 0) okay = 0;
    } else close(fd);
    if (okay && rename(temp, path) == 0) {
        sync_parent_directory(path);
        return 0;
    }
    {
        int saved_errno = errno;
        unlink(temp);
        errno = saved_errno;
    }
    return -1;
}

#endif /* Unix save persistence */

/* Show the end of long UTF-8 paths without breaking a grapheme.  The
 * underlying pathname is never shortened; only its on-screen label is. */
static void
save_display_path(char *out, size_t capacity, const char *path, int cells)
{
    size_t start;
    int suffix_cells;

    if (rogue_text_width(path) <= cells) {
        rogue_text_copy(out, capacity, path, strlen(path), cells);
        return;
    }
    start = strlen(path);
    suffix_cells = cells - 3; /* width of "..." */
    if (suffix_cells < 1)
        suffix_cells = 1;
    while (start > 0) {
        size_t previous = rogue_text_prev(path, start);
        if (rogue_text_width(path + previous) > suffix_cells)
            break;
        start = previous;
    }
    snprintf(out, capacity, "...%s", path + start);
}

int
save_game(void)
{
    struct savebuf s;
    char original_name[sizeof file_name];
    char dest[sizeof file_name];
    char display[sizeof file_name];
    int c, ask_for_name, show_retry_error = FALSE;

    /* The original prompt uses Y for the configured path and N to enter a
     * different filename.  Keep the UTF-8-aware editor and atomic save. */
    memcpy(original_name, file_name, sizeof original_name);
    ask_for_name = file_name[0] == '\0';
    if (!ask_for_name) {
        mpos = 0;
        save_display_path(display, sizeof display, file_name,
                          ROGUE_COLS - 2 - rogue_text_width(
                              JSTR("セーブファイル（\"\"）？",
                                   "Save file (\"\")? ")));
        msg(JSTR("セーブファイル（\"%s\"）？", "Save file (\"%s\")? "),
            display);
        do {
            c = readchar();
        } while (c != 'n' && c != 'N' && c != 'y' && c != 'Y');
        ask_for_name = c == 'n' || c == 'N';
    }

    for (;;) {
        if (ask_for_name) {
            /* get_str() accepts a full-width path only when passed
             * file_name, and handles UTF-8, '~', Backspace and Escape. */
            file_name[0] = '\0';
            if (!show_retry_error)
                mpos = 0;
            /* After an I/O error, retaining mpos produces the original
             * --More-- acknowledgement before replacing the error text. */
            msg(JSTR("ファイル名： ", "File name: "));
            show_retry_error = FALSE;
            mpos = 0;
            c = get_str(file_name, cw);
            if (c == QUIT) {
                memcpy(file_name, original_name, sizeof file_name);
                msg("");
                return FALSE;
            }
            if (c == MINUS || file_name[0] == '\0') {
                beep();
                continue;
            }
        }
        memcpy(dest, file_name, sizeof dest);
        memset(&s, 0, sizeof s);
        s.mem = (unsigned char *)malloc(SAVE_LIMIT);
        if (s.mem == NULL) {
            msg(JSTR("セーブに必要なメモリがありません。",
                     "Out of memory saving game."));
            return FALSE;
        }
        s.len = SAVE_HEAD;
        /* Do not serialize the confirmation prompt into message history. */
        mpos = 0;
        put_game(&s);
        if (!s.error) {
            uint32_t hash;
            memcpy(s.mem, SAVE_MAGIC, 8);
            s.mem[8] = (unsigned char)(s.len - SAVE_HEAD);
            s.mem[9] = (unsigned char)((s.len - SAVE_HEAD) >> 8);
            s.mem[10] = (unsigned char)((s.len - SAVE_HEAD) >> 16);
            s.mem[11] = (unsigned char)((s.len - SAVE_HEAD) >> 24);
            hash = checksum(s.mem + SAVE_HEAD, s.len - SAVE_HEAD);
            s.mem[12] = (unsigned char)hash;
            s.mem[13] = (unsigned char)(hash >> 8);
            s.mem[14] = (unsigned char)(hash >> 16);
            s.mem[15] = (unsigned char)(hash >> 24);
        }
        if (s.error) {
            free(s.mem);
            msg(JSTR("ゲームの状態をセーブできません。",
                     "Unable to save game state."));
            return FALSE;
        }
        if (
#ifdef _WIN32
            rogue_windows_write_save(dest, s.mem, s.len)
#else
            write_save(dest, s.mem, s.len)
#endif
            == 0) {
            free(s.mem);
            mpos = 0;
            save_display_path(display, sizeof display, dest,
                              ROGUE_COLS - 2 - rogue_text_width(
                                  JSTR("ファイル名： ", "File name: ")));
            msg(JSTR("ファイル名： %s", "File name: %s"), display);
            rogue_visual_delay(ROGUE_TEXT_NOTICE_DELAY_MS);
            return TRUE;
        }
        {
            int saved_errno = errno;
            free(s.mem);
            msg(JSTR("%sにセーブできません：%s",
                     "Unable to save to %s: %s"),
                dest, strerror(saved_errno));
        }
        /* The original keeps asking for another name on an I/O failure.
         * Escape from the editor cancels; a failed write is never accepted. */
        ask_for_name = TRUE;
        show_retry_error = TRUE;
    }
}

/* An opened restore file remains locked through validation and consumption.
 * Windows saves use the native file identity and delete-by-HANDLE APIs;
 * Unix keeps fstat/flock/unlink and the same R34SAV02 codec. */
struct save_source {
    FILE *fp;
    size_t length;
    time_t mtime;
#ifdef _WIN32
    struct rogue_windows_save_file *win;
#else
    int fd;
    struct stat st;
#endif
};

static int
save_source_open(struct save_source *source, const char *path)
{
    memset(source, 0, sizeof *source);
#ifdef _WIN32
    source->win = rogue_windows_open_save(path);
    if (source->win == NULL) return -1;
    source->fp = rogue_windows_save_stream(source->win);
    source->length = rogue_windows_save_size(source->win);
    source->mtime = rogue_windows_save_mtime(source->win);
#else
    source->fd = open(path, O_RDONLY
#ifdef O_NOFOLLOW
                      | O_NOFOLLOW
#endif
                      );
    if (source->fd < 0) return -1;
    if (fstat(source->fd, &source->st) != 0 ||
        !S_ISREG(source->st.st_mode) ||
        source->st.st_uid != getuid() || source->st.st_nlink != 1 ||
        source->st.st_size < SAVE_HEAD || source->st.st_size > SAVE_LIMIT ||
        flock(source->fd, LOCK_EX | LOCK_NB) != 0) {
        close(source->fd);
        return -1;
    }
    source->fp = fdopen(source->fd, "rb");
    if (source->fp == NULL) { close(source->fd); return -1; }
    source->length = (size_t)source->st.st_size;
    source->mtime = source->st.st_mtime;
#endif
    return 0;
}

static int
save_source_unchanged(struct save_source *source, const char *path)
{
#ifdef _WIN32
    return rogue_windows_save_unchanged(source->win, path);
#else
    return same_save_file(path, source->fd, &source->st);
#endif
}

static int
save_source_consume(struct save_source *source, const char *path)
{
#ifdef _WIN32
    return rogue_windows_consume_save(source->win, path);
#else
    return same_save_file(path, source->fd, &source->st) && unlink(path) == 0
           ? 0 : -1;
#endif
}

static void
save_source_close(struct save_source *source)
{
#ifdef _WIN32
    rogue_windows_close_save(source->win);
    source->win = NULL;
#else
    if (source->fp != NULL) fclose(source->fp);
#endif
    source->fp = NULL;
}

/* Match Rogue 3.6 JEC's Japanese restore timestamp without depending on
 * a Japanese system locale.  The English display retains the original ctime
 * order/spelling.  Both use the *saved file's* local modification time. */
static int
save_mtime_display(char *dst, size_t cap, time_t modified)
{
    const char *date;
    if (cap == 0) return FALSE;
    dst[0] = '\0';
    if (jrogue_lang_is_japanese) {
        static const char *const weekdays[] = {
            "日", "月", "火", "水", "木", "金", "土"
        };
        struct tm *local = localtime(&modified);
        if (local != NULL && local->tm_wday >= 0 && local->tm_wday < 7) {
            int n = snprintf(dst, cap, "%d年%d月%d日（%s）%02d:%02d:%02d",
                             local->tm_year + 1900, local->tm_mon + 1,
                             local->tm_mday, weekdays[local->tm_wday],
                             local->tm_hour, local->tm_min, local->tm_sec);
            if (n >= 0 && (size_t)n < cap)
                return TRUE;
        }
    }
    date = ctime(&modified);
    if (date == NULL) return FALSE;
    {
        size_t len = strcspn(date, "\r\n");
        if (len >= cap) len = cap - 1;
        memcpy(dst, date, len);
        dst[len] = '\0';
    }
    return TRUE;
}

int
restore(char *file, char **envp)
{
    char load_path[sizeof file_name];
    const char *path = load_path;
    const char *requested = (strcmp(file, "-r") == 0) ? file_name : file;
    struct save_source source;
    struct savebuf s;
    unsigned char head[SAVE_HEAD];
    uint32_t n, expected;
    FILE *fp;
    int okay;
    (void)envp;
    if (snprintf(load_path, sizeof load_path, "%s", requested) >=
        (int)sizeof load_path) {
        fprintf(stderr, "Rogue 3.4: save path is too long.\n");
        return FALSE;
    }
    if (save_source_open(&source, path) != 0) {
        fprintf(stderr, "Rogue 3.4: cannot open or lock save file %s.\n", path);
        return FALSE;
    }
    fp = source.fp;
    if (source.length < SAVE_HEAD || source.length > SAVE_LIMIT) {
        fprintf(stderr, "Rogue 3.4: unsafe or invalid save file.\n");
        save_source_close(&source);
        return FALSE;
    }
    memset(&s, 0, sizeof s);
    s.mem = (unsigned char *)malloc(source.length);
    if (s.mem == NULL) { save_source_close(&source); return FALSE; }
    okay = fread(s.mem, 1, source.length, fp) == source.length;
    if (fgetc(fp) != EOF || ferror(fp)) okay = 0;
    if (!save_source_unchanged(&source, path)) okay = 0;
    if (!okay || memcmp(s.mem, SAVE_MAGIC, 8) != 0) {
        fprintf(stderr, "Rogue 3.4: unsupported or truncated save file.\n");
        free(s.mem);
        save_source_close(&source);
        return FALSE;
    }
    memcpy(head, s.mem, SAVE_HEAD);
    n = (uint32_t)head[8] | ((uint32_t)head[9] << 8) |
        ((uint32_t)head[10] << 16) | ((uint32_t)head[11] << 24);
    expected = (uint32_t)head[12] | ((uint32_t)head[13] << 8) |
        ((uint32_t)head[14] << 16) | ((uint32_t)head[15] << 24);
    if (n != source.length - SAVE_HEAD ||
        expected != checksum(s.mem + SAVE_HEAD, n)) {
        fprintf(stderr, "Rogue 3.4: save file failed integrity check.\n");
        free(s.mem);
        save_source_close(&source);
        return FALSE;
    }
    s.len = source.length;
    s.pos = SAVE_HEAD;
    if (initscr() == NULL) {
        fprintf(stderr, "Rogue 3.4: unable to initialize curses for restore.\n");
        free(s.mem);
        save_source_close(&source);
        return FALSE;
    }
#ifdef _WIN32
    /* Terminal may have become ready after the Windows Terminal handoff. */
    rogue_windows_retry_direct_launch_resize(0);
#endif
    if (LINES < ROGUE_LINES || COLS < ROGUE_COLS) {
        endwin();
        fprintf(stderr, "Rogue 3.4 requires a terminal of at least 80x24.\n");
        free(s.mem);
        save_source_close(&source);
        return FALSE;
    }
    get_game(&s);
    free(s.mem);
    if (s.error) {
        endwin();
        fprintf(stderr, "Rogue 3.4: invalid save state (file retained).\n");
        save_source_close(&source);
        return FALSE;
    }
    /* Consume only a validated, unchanged save; never remove a newly
     * replaced pathname.  Wizard restores intentionally retain their save. */
    if (!wizard && save_source_consume(&source, path) != 0) {
        endwin();
        fprintf(stderr, "Rogue 3.4: save file changed or could not be consumed.\n");
        save_source_close(&source);
        return FALSE;
    }
    {
        time_t restore_date = source.mtime;
        save_source_close(&source);
        if (strlen(path) < sizeof file_name)
            strcpy(file_name, path);
        setup();
        mpos = 0;
        /* Original display: filename and file modification time. */
        {
            char stamp[64];
            char visible_path[sizeof file_name];
            if (save_mtime_display(stamp, sizeof stamp, restore_date)) {
                int path_width = ROGUE_COLS - 4 - rogue_text_width(stamp);
                if (path_width < 1) path_width = 1;
                save_display_path(visible_path, sizeof visible_path,
                                  path, path_width);
                msg("%s: %s", visible_path, stamp);
            } else {
                msg("%s", path);
            }
        }
    }
    wmove(cw, hero.y, hero.x);
    draw(cw);
    return TRUE;
}
