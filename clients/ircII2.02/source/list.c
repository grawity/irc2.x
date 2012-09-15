/*
 * list.c: some generic linked list managing stuff 
 *
 *
 * Written By Michael Sandrof <ms5n@andrew.cmu.edu> 
 *
 * Copyright(c) 1990 
 *
 * See the COPYRIGHT file, or do a HELP IRCII COPYRIGHT 
 */
#include "irc.h"
#include "list.h"

/*
 * add_to_list: This will add an element to a list.  The requirements for the
 * list are that the first element in each list structure be a pointer to the
 * next element in the list, and the second element in the list structure be
 * a pointer to a character (char *) which represents the sort key.  For
 * example 
 *
 * struct my_list{ struct my_list *next; char *name; <whatever else you want>}; 
 *
 * The parameters are:  "list" which is a pointer to the head of the list. "add"
 * which is a pre-allocated element to be added to the list.  "size" which is
 * the size of your list structure (in the above example, sizeof(struct
 * my_list)). 
 */
void add_to_list(list, add, size)
List **list;
List *add;
int size;
{
    List *tmp,
        *last;

    last = null(List *);
    for (tmp = *list; tmp; tmp = tmp->next) {
	if (strcmp(tmp->name, add->name) > 0)
	    break;
	last = tmp;
    }
    if (last)
	last->next = add;
    else
	*list = add;
    add->next = tmp;
}


/*
 * find_in_list: This looks up the given name in the given list.  List and
 * name are as described above.  If wild is true, each name in the list is
 * used as a wild card expression to match name... otherwise, normal matching
 * is done 
 */
List *find_in_list(list, name, wild)
List **list;
char *name;
int wild;
{
    List *tmp;

    if (wild) {
	List *match = null(List *);

	for (tmp = *list; tmp; tmp = tmp->next) {
	    if (wild_match(tmp->name, name))
		match = tmp;
	}
	return (match);
    } else {
	for (tmp = *list; tmp; tmp = tmp->next) {
	    if (stricmp(tmp->name, name) == 0)
		return (tmp);
	}
    }
    return (null(List *));
}

/*
 * remove_from_list: this remove the given name from the given list (again as
 * described above).  If found, it is removed from the list and returned
 * (memory is not deallocated).  If not found, null is returned. 
 */
List *remove_from_list(list, name)
List **list;
char *name;
{
    List *tmp,
        *last;

    last = null(List *);
    for (tmp = *list; tmp; tmp = tmp->next) {
	if (stricmp(tmp->name, name) == 0) {
	    if (last)
		last->next = tmp->next;
	    else
		*list = tmp->next;
	    return (tmp);
	}
	last = tmp;
    }
    return (null(List *));
}

/*
 * list_lookup: this routine just consolidates remove_from_list and
 * find_in_list.  I did this cause it fit better with some alread existing
 * code 
 */
List *list_lookup(list, name, wild, delete)
List **list;
char *name;
int wild;
int delete;
{
    List *tmp;

    if (delete)
	tmp = remove_from_list(list, name);
    else
	tmp = find_in_list(list, name, wild);
    return (tmp);
}
