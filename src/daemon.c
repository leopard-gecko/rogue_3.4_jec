/*
 * Contains functions for dealing with things that happen in the
 * future.
 *
 * @(#)daemon.c X.XX (Berkeley) XX/XX/XX
 */

#include "rogue.h"

/*
 * daemon:
 *	Start a daemon, takes a function.
 */
int
start_daemon(int (*func)(void), int arg, int type)
{
    register struct linked_list *item;
    register struct daemon_action *dev;

    item = new_item(sizeof *dev);
    if (item == NULL)
	return 0;
    attach(d_list, item);
    dev = (struct daemon_action *) ldata(item);
    dev->d_func = func;
    dev->d_arg = arg;
    dev->d_type = type;
    return 0;
}

/*
 * kill_daemon:
 *	Remove a daemon from the list
 */
int
kill_daemon(int (*func)(void))
{
    register struct linked_list *item;
    register struct daemon_action *dev;

    for (item = d_list; item != NULL; item = next(item))
    {
	dev = (struct daemon_action *) ldata(item);
	if (dev->d_func == func)
	    break;
    }
    if (item != NULL)
    {
	detach(d_list, item);
	discard(item);
    }
    return 0;
}

/*
 * do_daemons:
 *	Run all the daemons that are active with the current flag,
 *	passing the argument to the function.
 */
int
do_daemons(int flag)
{
    register struct linked_list *item, *next_item;
    register struct daemon_action *dev;

    /* A callback (notably rollwand) can remove its own list node.
       Capture its successor before invoking the callback. */
    for (item = d_list; item != NULL; item = next_item)
    {
        next_item = next(item);
        dev = (struct daemon_action *) ldata(item);
        if (dev->d_type == flag)
            (*dev->d_func)();
    }
    return 0;
}

/*
 * fuse:
 *	Start a fuse to go off in a certain number of turns
 */
int
fuse(int (*func)(void), int arg, int time, int type)
{
    register struct linked_list *item;
    register struct fuse_action *wire;

    item = new_item(sizeof *wire);
    if (item == NULL)
	return 0;
    attach(f_list, item);
    wire = (struct fuse_action *) ldata(item);
    wire->d_func = func;
    wire->d_arg = arg;
    wire->d_time = time;
    wire->f_type = type;
    return 0;
}

/*
 * lengthen:
 *	Increase the time until a fuse goes off
 */
int
lengthen(int (*func)(void), int xtime)
{
    register struct linked_list *item;
    register struct fuse_action *wire;

    for (item = f_list; item != NULL; item = next(item))
    {
	wire = (struct fuse_action *) ldata(item);
	if (func == wire->d_func)
	    break;
    }
    if (item != NULL)
	wire->d_time += xtime;
    return 0;
}

/*
 * extinguish:
 *	Put out a fuse
 */
int
extinguish(int (*func)(void))
{
    register struct linked_list *item;
    register struct fuse_action *wire;

    for (item = f_list; item != NULL; item = next(item))
    {
	wire = (struct fuse_action *) ldata(item);
	if (func == wire->d_func)
	    break;
    }
    if (item != NULL)
    {
	detach(f_list, item);
	discard(item);
    }
    return 0;
}

/*
 * do_fuses:
 *	Decrement counters and start needed fuses
 */
int
do_fuses(int flag)
{
    register struct linked_list *item, *nitem;
    register struct fuse_action *wire;

    for (item = f_list; item != NULL; item = nitem)
    {
	wire = (struct fuse_action *) ldata(item);
	nitem = next(item);
	if (flag == wire->f_type && --wire->d_time <= 0)
	{
	    int (*func)(void) = wire->d_func;
	    detach(f_list, item);
	    discard(item);
	    (*func)();
	}
    }
    return 0;
}

