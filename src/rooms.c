/*
 * Draw the nine rooms on the screen
 *
 * @(#)rooms.c	X.XX (Berkeley) XX/XX/XX
 */

#include "rogue.h"

int
do_rooms(void)
{
    register int i;
    register struct room *rp;
    register struct linked_list *item;
    register int left_out;
    coord top;
    coord bsze;
    coord mp;

    /*
     * bsze is the maximum room size
     */
    bsze.x = ROGUE_COLS/3;
    bsze.y = ROGUE_LINES/3;
    /*
     * Clear things for a new level
     */
    for (i = 0, rp = rooms; i < MAXROOMS; rp++, i++)
	rp->r_nexits = rp->r_flags = 0;
    /*
     * Put the gone rooms, if any, on the level
     */
    left_out = rnd(4);
    for (i = 0; i < left_out; i++)
	rooms[rnd_room()].r_flags |= ISGONE;
    /*
     * dig and populate all the rooms on the level
     */
    for (i = 0, rp = rooms; i < MAXROOMS; rp++, i++)
    {
	/*
	 * Find upper left corner of box that this room goes in
	 */
	top.x = (i%3)*bsze.x + 1;
	top.y = i/3*bsze.y;
	rp->r_goldval = 0;
	if (rp->r_flags & ISGONE)
	{
	    /*
	     * Place a gone room.  Make certain that there is a blank line
	     * for passage drawing.
	     */
	    do
	    {
		rp->r_pos.x = top.x + rnd(bsze.x-2) + 1;
		rp->r_pos.y = top.y + rnd(bsze.y-2) + 1;
	    } until(rp->r_pos.y > 0 && rp->r_pos.y < ROGUE_LINES-1);
	    continue;
	}
	if (rnd(10) < level-1)
	    rp->r_flags |= ISDARK;
	/*
	 * Find a place and size for a random room
	 */
	do
	{
	    rp->r_max.x = rnd(bsze.x - 4) + 4;
	    rp->r_max.y = rnd(bsze.y - 4) + 4;
	    rp->r_pos.x = top.x + rnd(bsze.x - rp->r_max.x);
	    rp->r_pos.y = top.y + rnd(bsze.y - rp->r_max.y);
	} until (rp->r_pos.y != 0);
	/*
	 * Put the gold in
	 */
	if (rnd(100) < 50)
	{
	    rp->r_goldval = GOLDCALC;
	    rnd_pos(rp, &rp->r_gold);
	}
	draw_room(rp);
	/*
	 * Put the monster in
	 */
	if (rnd(100) < (rp->r_goldval > 0 ? 80 : 25))
	{
	    item = new_item(sizeof(struct thing));
            if (rogue_find_floor(&mp, i, TRUE))
                new_monster(item, randmonster(), &mp);
            else
                discard(item);
	}
    }
    return 0;
}

/*
 * Draw a box around a room
 */
int
draw_room(struct room *rp)
{
    register int j, k;

    move(rp->r_pos.y, rp->r_pos.x+1);
    vert(rp->r_max.y-2);				/* Draw left side */
    move(rp->r_pos.y+rp->r_max.y-1, rp->r_pos.x);
    horiz(rp->r_max.x);					/* Draw bottom */
    move(rp->r_pos.y, rp->r_pos.x);
    horiz(rp->r_max.x);					/* Draw top */
    vert(rp->r_max.y-2);				/* Draw right side */
    /*
     * Put the floor down
     */
    for (j = 1; j < rp->r_max.y-1; j++)
    {
	move(rp->r_pos.y + j, rp->r_pos.x+1);
	for (k = 1; k < rp->r_max.x-1; k++)
	    addch(FLOOR);
    }
    /*
     * Put the gold there
     */
    if (rp->r_goldval)
	mvaddch(rp->r_gold.y, rp->r_gold.x, GOLD);
    return 0;
}

/*
 * horiz:
 *	draw a horizontal line
 */
int
horiz(int cnt)
{
    while (cnt--)
	addch('-');
    return 0;
}

/*
 * vert:
 *	draw a vertical line
 */
int
vert(int cnt)
{
    register int x, y;

    while (cnt--) {
	getyx(stdscr, y, x);
	move(y + 1, x - 1);
	addch('|');
    }
    return 0;
}

/*
 * rogue_find_floor:
 * Preserve the original rejection-sampling sequence for as long as it
 * can produce a different candidate.  The 1981 RNG discards all but 15
 * state bits after each draw.  Thus, if the state at the START of an
 * attempt repeats, the same sequence of room/position draws will repeat
 * forever without finding a floor.  Only then use a deterministic scan.
 *
 * An arbitrary retry limit (256 in stages 13-17) was too aggressive:
 * a valid empty square may first be sampled after more than 256 attempts.
 * Keeping track of the 15-bit state avoids changing such legitimate
 * outcomes or their subsequent RNG stream.
 *
 * A negative room reselects a random room on each attempt.  When
 * only_room is set, the fallback stays inside the originating room.
 */
int
rogue_find_floor(coord *cp, int room, bool only_room)
{
    unsigned char seen[1u << 15] = { 0 };
    unsigned int state;
    int i, idx, y, x;
    struct room *rp;

    for (;;)
    {
        /* RN is determined entirely by these 15 bits, even if the initial
         * seed contains higher bits that do not affect RN. */
        state = seed & 0x7fffu;
        if (seen[state])
            break;
        seen[state] = 1;

        idx = room < 0 ? rnd_room() : room;
        rp = &rooms[idx];
        rnd_pos(rp, cp);
        if (winat(cp->y, cp->x) == FLOOR)
            return TRUE;
    }

    /* No future draw can reach a new position; scan without using RNG. */
    for (i = 0; i < (only_room ? 1 : MAXROOMS); i++)
    {
        idx = room < 0 ? i : (room + i) % MAXROOMS;
        rp = &rooms[idx];
        if (rp->r_flags & ISGONE)
            continue;
        for (y = rp->r_pos.y + 1; y < rp->r_pos.y + rp->r_max.y - 1; y++)
            for (x = rp->r_pos.x + 1; x < rp->r_pos.x + rp->r_max.x - 1; x++)
                if (winat(y, x) == FLOOR)
                {
                    cp->y = y;
                    cp->x = x;
                    return TRUE;
                }
    }
    return FALSE;
}

/*
 * rnd_pos:
 *	pick a random spot in a room
 */
int
rnd_pos(struct room *rp, coord *cp)
{
    cp->x = rp->r_pos.x + rnd(rp->r_max.x-2) + 1;
    cp->y = rp->r_pos.y + rnd(rp->r_max.y-2) + 1;
    return 0;
}
