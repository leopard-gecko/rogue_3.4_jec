/*
 * File with various monster functions in it
 *
 * @(#)monsters.c	X.XX (Berkeley) XX/XX/XX
 */

#include "rogue.h"
#include "jec/game_text.h"
#include "jec/language.h"

/*
 * List of monsters in rough order of vorpalness
 */
static char *lvl_mons =  "JKBHSEAOZGLCRQNYTWFIXUMVDP";
static char *wand_mons = "JKBHS AOZG CRQ Y W IXU V  ";

/*
 * randmonster:
 *	Pick a monster to show up.  The lower the level,
 *	the meaner the monster.
 */
int
randmonster(void)
{
    register int d;

    d = rnd(10) + level - 5;
    if (d < 1)
	d = rnd(5) + 1;
    if (d > 26)
	d = rnd(5) + 22;
    return lvl_mons[d-1];
}

/*
 * randwand:
 *	Pick a random wandering monster
 */
int
randwand(void)
{
    register int d;

    do
    {
	d = rnd(10) + level - 5;
	if (d < 1)
	    d = rnd(5) + 1;
	if (d > 26)
	    d = rnd(5) + 22;
    } while (wand_mons[d-1] == ' ');
    return lvl_mons[d-1];
}

/*
 * new_monster:
 *	Pick a new monster and add it to the list
 */
int
new_monster(struct linked_list *item, char type, coord *cp)
{
    register struct thing *tp;
    register struct monster *mp;

    attach(mlist, item);
    tp = (struct thing *) ldata(item);
    tp->t_type = type;
    tp->t_pos = *cp;
    mvwaddch(mw, cp->y, cp->x, tp->t_type);
    mp = &monsters[tp->t_type-'A'];
    tp->t_stats.s_hpt = roll(mp->m_stats.s_lvl, 8);
    tp->t_stats.s_lvl = mp->m_stats.s_lvl;
    tp->t_stats.s_arm = mp->m_stats.s_arm;
    tp->t_stats.s_dmg = mp->m_stats.s_dmg;
    tp->t_stats.s_exp = mp->m_stats.s_exp;
    tp->t_stats.s_str.st_str = 10;
    tp->t_flags = mp->m_flags;
    tp->t_pack = NULL;
    if (type == 'M')
    {
	char mch = GOLD; /* rnd(6) always selects one of six disguises. */

	switch (rnd(6))
	{
	    when 0: mch = GOLD;
	    when 1: mch = POTION;
	    when 2: mch = SCROLL;
	    when 3: mch = STAIRS;
	    when 4: mch = WEAPON;
	    when 5: mch = ARMOR;
	}
	tp->t_disguise = mch;
    }
    return 0;
}

/*
 * wanderer:
 *	A wandering monster has awakened and is headed for the player
 */
int
wanderer(void)
{
    int i, x, y, found = FALSE;
    int ch;
    unsigned char seen[1u << 15] = { 0 };
    unsigned int state;
    struct room *rp, *hr = roomin(&hero);
    struct linked_list *item;
    struct thing *tp;
    coord cp = {0, 0};

    item = new_item(sizeof *tp);
    /* The original tested stdscr only, which ignored monsters in mw and
       could spawn two monsters at the same position.  Keep the corrected
       occupancy test, but do not abandon original rejection sampling at an
       arbitrary count.  Every draw depends only on the low 15 RNG bits;
       the same state before an attempt means future positions repeat. */
    for (;;)
    {
        state = seed & 0x7fffu;
        if (seen[state])
            break;
        seen[state] = 1;

        rp = &rooms[rnd_room()];
        if (rp == hr)
            continue;
        rnd_pos(rp, &cp);
        ch = winat(cp.y, cp.x);
        if (step_ok(ch))
        {
            found = TRUE;
            break;
        }
    }
    if (!found)
    {
        for (i = 0; i < MAXROOMS && !found; i++)
        {
            rp = &rooms[i];
            if (rp == hr || (rp->r_flags & ISGONE))
                continue;
            for (y = rp->r_pos.y + 1; y < rp->r_pos.y + rp->r_max.y - 1 && !found; y++)
                for (x = rp->r_pos.x + 1; x < rp->r_pos.x + rp->r_max.x - 1; x++)
                    if (step_ok(winat(y, x)))
                    {
                        cp.x = x;
                        cp.y = y;
                        found = TRUE;
                        break;
                    }
        }
    }
    if (!found)
    {
        discard(item);  /* No unoccupied destination exists. */
        return 0;
    }
    new_monster(item, randwand(), &cp);
    tp = (struct thing *) ldata(item);
    tp->t_flags |= ISRUN;
    tp->t_pos = cp;
    tp->t_dest = &hero;
    if (wizard)
        msg(JSTR("%sがさまよい始めた。", "Started a wandering %s"), jrogue_text(monsters[tp->t_type-'A'].m_name));
    return 0;
}
