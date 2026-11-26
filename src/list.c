/*
 * Functions for dealing with linked lists of goodies
 *
 * @(#)list.c	X.XX (Berkeley) XX/XX/XX
 */

#include "rogue.h"
#include "jec/language.h"

/*
 * detach:
 *	Takes an item out of whatever linked list it might be in
 */
int
_detach(struct linked_list **list, struct linked_list *item)
{
    if (*list == item)
	*list = next(item);
    if (prev(item) != NULL) item->l_prev->l_next = next(item);
    if (next(item) != NULL) item->l_next->l_prev = prev(item);
    item->l_next = NULL;
    item->l_prev = NULL;
    return 0;
}

/*
 * _attach:
 *	add an item to the head of a list
 */
int
_attach(struct linked_list **list, struct linked_list *item)
{
    if (*list != NULL)
    {
	item->l_next = *list;
	(*list)->l_prev = item;
    }
    else
    {
	item->l_next = NULL;
	item->l_prev = NULL;
    }

    *list = item;
    return 0;
}

/*
 * _free_list:
 *	Throw the whole blamed thing away
 */
int
_free_list(struct linked_list **ptr)
{
    register struct linked_list *item;

    while (*ptr != NULL)
    {
	item = *ptr;
	*ptr = next(item);
	discard(item);
    }
    return 0;
}

/*
 * discard:
 *	free up an item
 */
int
discard(struct linked_list *item)
{
    total -= 2;
    FREE(item->l_data);
    FREE(item);
    return 0;
}

/*
 * new_item
 *	get a new item with a specified size
 */
struct linked_list *
new_item(int size)
{
    register struct linked_list *item;

    if ((item = (struct linked_list *) new(sizeof *item)) == NULL)
	msg(JSTR("%d個のヘッダを確保した後、メモリが足りなくなった。", "Ran out of memory for header after %d items"), total);
    if ((item->l_data = new(size)) == NULL)
	msg(JSTR("%d個のデータを確保した後、メモリが足りなくなった。", "Ran out of memory for data after %d items"), total);
    item->l_next = item->l_prev = NULL;
    return item;
}

char *
new(int size)
{
    register char *space = ALLOC(size);

    if (space == NULL)
	fatal("Rogue ran out of memory.  Fatal error!");
    total++;
    return space;
}
