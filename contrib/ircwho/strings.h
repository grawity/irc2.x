/*
 * This program has been written by Kannan Varadhan and Jeff Trim.
 * You are welcome to use, copy, modify, or circulate as you please,
 * provided you do not charge any fee for any of it, and you do not
 * remove these header comments from any of these files.
 *
 * Jeff Trim wrote the socket code
 *
 * Kannan Varadhan wrote the string manipulation code
 *
 *		KANNAN		kannan@cis.ohio-state.edu
 *		Jeff		jtrim@orion.cair.du.edu
 *		Wed Aug 23 22:44:39 EDT 1989
 */

/*
 * \begin{strings}
 */
#include <strings.h>

#define Index		(char *) index
#define Strcpy		(void *) strcpy
#define	Strncpy		(void *) strncpy
#define Strcat		(void *) strcat
#define Strcmp		(int) strcmp
#define	Strncmp		(int) strncmp
#define	Strlen		(int) strlen

#define	Sprintf		(void) sprintf
#define	Sscanf		(void) sscanf
/*
 * \end{strings}
 */

