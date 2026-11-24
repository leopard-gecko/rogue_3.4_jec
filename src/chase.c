/*
 * Code for one object to chase another
 *
 * @(#)chase.c	X.XX (Berkeley) XX/XX/XX
 */

#include "rogue.h"
#include "jec/language.h"

coord ch_ret;				/* Where chasing takes you */

/*
 * runners:
 *	Make all the running monsters move.
 */
int
runners(void)
{
    register struct linked_list *item, *next_item;
    register struct thing *tp;

    /* A leprechaun or nymph can vanish during attack().  Save the next
       node before do_chase() can free the current monster's list node. */
    for (item = mlist; item != NULL; item = next_item)
    {
        next_item = next(item);
	tp = (struct thing *) ldata(item);
	if (off(*tp, ISHELD) && on(*tp, ISRUN))
	    do_chase(tp);
    }
    return 0;
}

/*
 * do_chase:
 *	Make one thing chase another.
 */
int
do_chase(struct thing *th)
{
    register struct room *rer, *ree;	/* room of chaser, room of chasee */
    register int mindist = 32767, i, dist;
    register bool stoprun = FALSE;	/* TRUE means we are there */
    coord this;				/* Temporary destination for chaser */
    char sch;

    rer = roomin(&th->t_pos);	/* Find room of chaser */
    ree = roomin(th->t_dest);	/* Find room of chasee */
    /*
     * We don't count doors as inside rooms for this routine
     */
    if (rogue_glyph(mvwinch(stdscr, th->t_pos.y, th->t_pos.x)) == DOOR)
	rer = NULL;
    this = *th->t_dest;
    /*
     * If the object of our desire is in a different room,
     * than we are and we ar not in a corridor, run to the
     * door nearest to our goal.
     */
    if (rer != NULL && rer != ree)
	for (i = 0; i < rer->r_nexits; i++)	/* loop through doors */
	{
	    dist = DISTANCE(th->t_dest->y, th->t_dest->x,
			    rer->r_exit[i].y, rer->r_exit[i].x);
	    if (dist < mindist)			/* minimize distance */
	    {
		this = rer->r_exit[i];
		mindist = dist;
	    }
	}
    /*
     * this now contains what we want to run to this time
     * so we run to it.  If we hit it we either want to fight it
     * or stop running
     */
    if (!chase(th, &this))
    {
	if (ce(this, hero))
	{
	    attack(th);
	    return 0;
	}
	else if (th->t_type != 'F')
	    stoprun = TRUE;
    }
    else if (th->t_type == 'F')
	return 0;
    if (rogue_glyph(mvwinch(cw, th->t_pos.y, th->t_pos.x)) != ' ')
    {
	sch = rogue_glyph(mvwinch(stdscr, th->t_pos.y, th->t_pos.x));
	switch (sch)
	{
	    case TRAP:
                {
                    int index = trap_at(th->t_pos.y, th->t_pos.x);
                    if (index < 0 || !(traps[index].tr_flags & ISFOUND))
                        sch = FLOOR;
                }
		break;
	    case SECRETDOOR:
		sch = secretdoor(th->t_pos.y, th->t_pos.x);
		break;
	    default:
		if (rer != NULL)
		{
		    if (rer == ree && !(rer->r_flags & ISDARK))
			break;
		    sch = (rer->r_flags & ISDARK) ? ' ' : FLOOR;
		}
		break;
	    case FLOOR:
		if (rer != NULL && (rer->r_flags & ISDARK))
		    sch = ' ';
		break;
	    case STAIRS:
		break;
	}
	mvwaddch(cw, th->t_pos.y, th->t_pos.x, sch);
    }

    if (cansee(unc(ch_ret)) && !on(*th, ISINVIS))
	mvwaddch(cw, ch_ret.y, ch_ret.x, th->t_type);
    mvwaddch(mw, th->t_pos.y, th->t_pos.x, ' ');
    mvwaddch(mw, ch_ret.y, ch_ret.x, th->t_type);
    th->t_pos = ch_ret;
    /*
     * And stop running if need be
     */
    if (stoprun && ce(th->t_pos, *(th->t_dest)))
	th->t_flags &= ~ISRUN;
    return 0;
}

/*
 * runto:
 *	Set a mosnter running after something
 *	or stop it from running (for when it dies)
 */
int
runto(coord *runner, coord *spot)
{
    register struct linked_list *item;
    register struct thing *tp;

    /*
     * If we couldn't find him, something is funny
     */
    if ((item = find_mons(runner->y, runner->x)) == NULL)
    {
        msg(JSTR("追跡者 '%s'", "CHASER '%s'\n"), rogue_unctrl(winat(runner->y, runner->x)));
        return 0;  /* No monster left at the requested position. */
    }
    tp = (struct thing *) ldata(item);
    /*
     * Start the beastie running
     */
    tp->t_dest = spot;
    tp->t_flags |= ISRUN;
    tp->t_flags &= ~ISHELD;
    return 0;
}

/*
 * chase:
 *	Find the spot for the chaser(er) to move closer to the
 *	chasee(ee).  Returns TRUE if we want to keep on chasing later
 *	FALSE if we reach the goal.
 */
int
chase(struct thing *tp, coord *ee)
{
    register int x, y;
    register int dist, thisdist;
    register struct linked_list *item;
    register struct object *obj;
    register coord *er = &tp->t_pos;
    register char ch;
    coord mv;

    dist = DISTANCE(er->y, er->x, ee->y, ee->x);
    mv = *er;
    /*
     * If the thing is confused, let it move randomly. Invisible
     * Stalkers are slightly confused all of the time, and bats are
     * quite confused all the time
     */
    if ((on(*tp, ISHUH) && rnd(10) < 8)
	|| (tp->t_type == 'B' && rnd(10) < 2))
    {
	/*
	 * get a valid random move
	 */
	mv = *rndmove(tp);
	diag_ok(er, &mv);
	dist = DISTANCE(mv.y, mv.x, ee->y, ee->x);
	/*
	 * Small chance that it will become un-confused
	 */
	if (rnd(1000) < 50)
	    tp->t_flags &= ~ISHUH;
    }
    /*
     * Otherwise, find the empty spot next to the chaser that is
     * closest to the chasee.
     */
    else
    {
	/*
	 * This will eventually hold where we move to get closer
	 * If we can't find an empty spot, we stay where we are.
	 */

	for (x = er->x - 1; x <= er->x + 1; x++)
	    for (y = er->y - 1; y <= er->y + 1; y++)
	    {
		coord tryp, dst;

                if (x < 0 || x >= ROGUE_COLS ||
                    y < 0 || y >= ROGUE_LINES)
                    continue;
		tryp.x = x;
		tryp.y = y;
		diag_ok(er, &tryp);
		dst.y = tryp.y;
		dst.x = tryp.x;
		ch = winat(dst.y, dst.x);
		if (step_ok(ch))
		{
		    /*
		     * If it is a scroll, it might be a scare monster scroll
		     * so we need to look it up to see what type it is.
		     */
		    item = NULL;
		    if (ch == SCROLL)
		    {
			for (item = lvl_obj; item != NULL; item = next(item))
			{
			    obj = (struct object *) ldata(item);
			    if (dst.y == obj->o_pos.y && dst.x == obj->o_pos.x)
				break;
			}
		    }
		    if (item != NULL && obj->o_which == S_SCARE)
			continue;
		    /*
		     * If we didn't find any scrolls at this place or it
		     * wasn't a scare scroll, then this place counts
		     */
		    thisdist = DISTANCE(dst.y, dst.x, ee->y, ee->x);
		    if (thisdist < dist)
		    {
			mv = dst;
			dist = thisdist;
		    }
		}
	    }
    }
    ch_ret = mv;
    return (dist != 0);
}

/*
 * roomin:
 *	Find what room some coordinates are in. NULL means they aren't
 *	in any room.
 */
struct room *
roomin(coord *cp)
{
    if (!(rooms[0].r_flags & ISGONE) && inroom(&rooms[0], cp))
	return &rooms[0];
    if (!(rooms[1].r_flags & ISGONE) && inroom(&rooms[1], cp))
	return &rooms[1];
    if (!(rooms[2].r_flags & ISGONE) && inroom(&rooms[2], cp))
	return &rooms[2];
    if (!(rooms[3].r_flags & ISGONE) && inroom(&rooms[3], cp))
	return &rooms[3];
    if (!(rooms[4].r_flags & ISGONE) && inroom(&rooms[4], cp))
	return &rooms[4];
    if (!(rooms[5].r_flags & ISGONE) && inroom(&rooms[5], cp))
	return &rooms[5];
    if (!(rooms[6].r_flags & ISGONE) && inroom(&rooms[6], cp))
	return &rooms[6];
    if (!(rooms[7].r_flags & ISGONE) && inroom(&rooms[7], cp))
	return &rooms[7];
    if (!(rooms[8].r_flags & ISGONE) && inroom(&rooms[8], cp))
	return &rooms[8];
    return NULL;
}

/*
 * find_mons:
 *	Find the monster from his corrdinates
 */
struct linked_list *
find_mons(int y, int x)
{
    register struct linked_list *item;
    register struct thing *th;

    for (item = mlist; item != NULL; item = next(item))
    {
	th = (struct thing *) ldata(item);
	if (th->t_pos.y == y && th->t_pos.x == x)
	    return item;
    }
    return NULL;
}

/*
 * diag_ok:
 *	Check to see if the move is legal if it is diagonal
 */
int
diag_ok(coord *sp, coord *ep)
{
    if (ep->y == sp->y || ep->x == sp->x)
	return 0;
    if (step_ok(rogue_glyph(mvinch(ep->y, sp->x))) && step_ok(rogue_glyph(mvinch(sp->y, ep->x))))
	return 0;
    *ep = *sp;
    return 0;
}

/*
 * cansee:
 *	returns true if the hero can see a certain coordinate.
 */
int
cansee(int y, int x)
{
    register struct room *rer;
    coord tp;

    tp.y = y;
    tp.x = x;
    rer = roomin(&tp);
    /*
     * We can only see if the hero in the same room as
     * the coordinate and the room is lit or if it is close.
     */
    return (rer != NULL && rer == roomin(&hero) && !(rer->r_flags&ISDARK)) ||
	    DISTANCE(y, x, hero.y, hero.x) < 3;
}
