#! /bin/sh
# /*
#  * This program has been written by Kannan Varadhan and Jeff Trim.
#  * You are welcome to use, copy, modify, or circulate as you please,
#  * provided you do not charge any fee for any of it, and you do not
#  * remove these header comments from any of these files.
#  *
#  * Jeff Trim wrote the socket code
#  *
#  * Kannan Varadhan wrote the string manipulation code
#  *
#  *		KANNAN		kannan@cis.ohio-state.edu
#  *		Jeff		jtrim@orion.cair.du.edu
#  *		Wed Aug 23 22:44:39 EDT 1989
#  */

# This is basically a huge awk script that runs Ircwho, and formats
# the output.  It was a test code that preceeded ircw.  Use it with no
# guarantees. Write your own tool to post process Ircwho's output
# as you wish.
#
# Change the IRCWHOBIN variable such that it points to the location of
# the Ircwho program, and try...Enjoy

PATH="/bin:/usr/ucb:/usr/bin"
export  PATH

IRCWHOBIN=/usr/local/bin/Ircwho

case $# in
0)	$IRCWHOBIN | awk '{
		eaddr = sprintf ("%s@%s", $3, $4) ;
		eaddr = substr (eaddr, 1, 30);
		colon = index($0, ":");
		name = substr ($0, colon + 1, 24);
		printf "%-9s\t%-30s\ton%4s\t%s\n", $6, eaddr, $2, name
		}' ;;
*)	case $1 in
	-s)	$IRCWHOBIN | awk '
		/Nickname/	{ next ; }
		{ print $6; }' | sort | fmt
		;;
	*)	$IRCWHOBIN "$@" | awk '
/ERROR/		{
		printf "%s is not on right now, sorry\n", $1;
		next;
		}
/NOTICE/	{
		if ($0 !~ /channel/)
		  next;
		nick = substr ($2, 2);
		if ($0 ~ /private/)
		  printf "%s %s@%s is on a private channel on IRC\n", nick, $4, $6
		else
		  printf "%s %s@%s is on channel %d on IRC\n", nick, $4, $6, $NF
		next;
		}
/401/		{
		printf "%s is not on right now, sorry\n", $3;
		next;
		}
/311/		{
		line = substr ($0, 2);
		name = substr (line, index (line, ":") + 1);
		if ($0 ~ /\*/)
		  printf "%s %s@%s (%s) is on a private channel on IRC\n", $3, $4, $5, name
		else
		  printf "%s %s@%s (%s) is on channel %s on IRC\n", $3, $4, $5, name, $6
		next;
		}
		{ next; }'
	;;
	esac
esac
exit
