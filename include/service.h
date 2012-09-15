/*
 * $Id: service.h,v 1.2 90/07/26 16:23:14 gruner Exp $
 */

#define SERVICE_ALL 	0	/* used to catch all services -- be careful */
#define SERVICE_NICK	1
#define SERVICE_NOTE	2
#define SERVICE_TIME	3
#define SERVICE_NETCONF	4	/* Some predefined services ... */

#define LINK_LOCAL	0
#define LINK_REMOTE	1

#define SERVICE_WANT_USER	0x00001L
#define SERVICE_WANT_NICK	0x00002L
#define SERVICE_WANT_SERVER	0x00004L
#define SERVICE_WANT_QUIT	0x00008L
#define SERVICE_WANT_KILL	0x00010L
#define SERVICE_WANT_SQUIT	0x00020L
#define SERVICE_WANT_OPER	0x00040L
#define SERVICE_WANT_SERVNOTE	0x00080L
#define SERVICE_WANT_WALLOP	0x00100L
#define SERVICE_WANT_SERVICE	0x00200L

typedef struct Service	
    {
	char	dist[HOSTLEN+1];
	char	server[HOSTLEN+1];
	char	passwd[PASSWDLEN+1];
	int	type;
	long	wanted; /* Maybe we need less ... more ??  -Armin */
    } aService;
