/*
 * All the daemon and fuse functions are in here
 *
 * @(#)daemons.c	X.XX (Berkeley) XX/XX/XX
 */

#include "rogue.h"
#include "jec/language.h"

int rogue_rollwand_between = 0;

/*
 * doctor:
 *	A healing daemon that restors hit points after rest
 */
int
doctor(void)
{
    register int lv, ohp;

    lv = pstats.s_lvl;
    ohp = pstats.s_hpt;
    quiet++;
    if (lv < 8)
    {
	if (quiet > 20 - lv*2)
	    pstats.s_hpt++;
    }
    else
	if (quiet >= 3)
	    pstats.s_hpt += rnd(lv - 7)+1;
    if (ohp != pstats.s_hpt)
    {
	if (pstats.s_hpt > max_hp)
	    pstats.s_hpt = max_hp;
	quiet = 0;
    }
    return 0;
}

/*
 * Swander:
 *	Called when it is time to start rolling for wandering monsters
 */
int
swander(void)
{
    start_daemon(rollwand, 0, BEFORE);
    return 0;
}

/*
 * rollwand:
 *	Called to roll to see if a wandering monster starts up
 */
int
rollwand(void)
{


    if (++rogue_rollwand_between >= 4)
    {
	if (roll(1, 6) == 4)
	{
	    wanderer();
	    kill_daemon(rollwand);
	    fuse(swander, 0, WANDERTIME, BEFORE);
	}
	rogue_rollwand_between = 0;
    }
    return 0;
}

/*
 * unconfuse:
 *	Release the poor player from his confusion
 */
int
unconfuse(void)
{
    player.t_flags &= ~ISHUH;
    msg(JSTR("ようやく混乱がおさまってきた。", "You feel less confused now"));
    return 0;
}


/*
 * unsee:
 *	He lost his see invisible power
 */
int
unsee(void)
{
    player.t_flags &= ~CANSEE;
    return 0;
}

/*
 * hungry:
 *	The hero is getting hungry — first stage of the hunger chain.
 *	Announces hunger and sets up the next fuse (weak).
 */
int
hungry(void)
{
    if (!terse)
	msg(JSTR("そろそろ空腹になってきた。", "You are starting to get hungry"));
    else
	msg(JSTR("空腹になってきた。", "Getting hungry"));
    fuse(weak, 0, MORETIME, AFTER);
    return 0;
}

/*
 * weak:
 *	The hero is getting weak from hunger — second stage.
 *	Announces weakness and sets up the next fuse (starve).
 */
int
weak(void)
{
    if (!terse)
	msg(JSTR("空腹のあまり力が弱くなってきた。", "You are so hungry, you are starting to feel weak"));
    else
	msg(JSTR("空腹で力が出ない。", "You are weak from hunger"));
    fuse(starve, 0, MORETIME, AFTER);
    return 0;
}

/*
 * starve:
 *	The hero is about to start fainting — third stage.
 *	Starts the faint daemon.
 */
int
starve(void)
{
    start_daemon(faint, 0, AFTER);
    return 0;
}

/*
 * faint:
 *	The hero is fainting — runs each turn and randomly
 *	causes the hero to pass out.
 */
int
faint(void)
{
    if (no_command == 0 && rnd(100) <= 20)
    {
	no_command = rnd(8) + 4;
	if (!terse)
	    msg(JSTR("食べていないので体が弱りきっている。気を失った。", "You feel so weak from lack of food.  You faint."));
	else
	    msg(JSTR("気を失った。", "You faint"));
	running = FALSE;
	count = 0;
    }
    return 0;
}

/*
 * nohaste:
 *	End the hasting
 */
int
nohaste(void)
{
    player.t_flags &= ~ISHASTE;
    msg(JSTR("素早くなる薬の効き目がなくなった。", "You feel yourself slowing down."));
    return 0;
}
