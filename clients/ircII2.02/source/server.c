/*
 * server.c: Things dealing with server connections, etc. 
 *
 * Written By Michael Sandrof <ms5n@andrew.cmu.edu> 
 *
 * Copyright(c) 1990 
 *
 * See the COPYRIGHT file, or do a HELP IRCII COPYRIGHT 
 */
#include <stdio.h>
#include <signal.h>
#include <sys/file.h>
#include "irc.h"
#include "server.h"
#include "ircaux.h"
#include "whois.h"

static int using_server_process = 1;

int server_write = -1,
    server_read = -1;

/* Server: a structure for the server_list */
typedef struct {
    char *name;			/* the name of the server */
    char *password;		/* password for that server */
    int port;			/* port number on that server */
}      Server;

/* server_list: the list of servers that the user can connect to,etc */
static Server *server_list = null(Server *);

/* number_of_servers: in the server list */
static int number_of_servers = 0;

/* current_server: index of the current server in the server list */
static int current_server;

void close_server()
{
    if   (server_read != -1) {
	     new_close(server_read);
	server_read = -1;
    }
    if (server_write != -1) {
	new_close(server_write);
	server_write = -1;
    }
    clean_whois_queue();  /* make sure no leftover whois's are out there */
}

/*
 * add_to_server_list: adds the given server to the server_list.  If the
 * server is already in the server list it is not re-added... however, if the
 * overwrite flag is true, the port and passwords are updated to the values
 * passes.  If the server is not on the list, it is added to the end. In
 * either case, the server is made the current server. 
 */
void add_to_server_list(server, port, password, overwrite)
char *server;
int port;
char *password;
char overwrite;
{
    int i,
        len,
        len2;

    len = strlen(server);
    for (i = 0; i < number_of_servers; i++) {
	len2 = strlen(server_list[i].name);
	if (len2 > len) {
	    if (strnicmp(server, server_list[i].name, len) == 0) {
		if (overwrite) {
		    server_list[i].port = port;
		    if (password)
			malloc_strcpy(&(server_list[i].password), password);
		}
		current_server = i;
		return;
	    }
	} else {
	    if (strnicmp(server, server_list[i].name, len2) == 0) {
		if (overwrite) {
		    server_list[i].port = port;
		    if (password)
			malloc_strcpy(&(server_list[i].password), password);
		}
		malloc_strcpy(&(server_list[i].name), server);
		current_server = i;
		return;
	    }
	}
    }
    current_server = number_of_servers++;
    if (server_list)
	server_list = (Server *) new_realloc(server_list, number_of_servers * sizeof(Server));
    else
	server_list = (Server *) new_malloc(number_of_servers * sizeof(Server));
    server_list[current_server].name = null(char *);
    server_list[current_server].password = null(char *);
    malloc_strcpy(&(server_list[current_server].name), server);
    if (password)
	malloc_strcpy(&(server_list[current_server].password), password);
    server_list[current_server].port = port;
}

/*
 * build_server_list: given a whitespace separated list of server names this
 * builds a list of those servers using add_to_server_list().  Since
 * add_to_server_list() is used to added each server specification, this can
 * be called many many times to add more servers to the server list.  Each
 * element in the server list case have one of the following forms: 
 *
 * servername 
 *
 * servername:port 
 *
 * servername:port:password 
 *
 * servername::password 
 */
void build_server_list(servers)
char *servers;
{
    char *host,
        *password = null(char *),
        *port = null(char *);
    int port_num;

    port_num = irc_port;
    while (host = next_arg(servers, &servers)) {
	if (port = index(host, ':')) {
	    *(port++) = null(char);
	    if (password = index(port, ':')) {
		*(password++) = null(char);
		if (strlen(password) == 0)
		    password = null(char *);
	    }
	}
	if (port && strlen(port))
	    port_num = atoi(port);
	else
	    port_num = irc_port;
	add_to_server_list(host, port_num, password, 0);
    }
}

/*
 * connect_to_server_direct: handles the tcp connection to a server.  If
 * successful, the user is disconnected from any previously connected server,
 * the new server is added to the server list, and the user is registered on
 * the new server.  If connection to the server is not successful,  the
 * reason for failure is displayed and the previous server connection is
 * resumed uniterrupted. 
 *
 * This version of connect_to_server() connects directly to a server 
 *
 */
static void connect_to_server_direct(server_name, port)
char *server_name;
int port;
{
    int new_des;

    using_server_process = 0;
    oper_command = 0;
    if ((new_des = connect_by_number(port, server_name)) < 0) {
	put_it("*** Unable to connect to port %d of server %s: %s", port, server_name, errno ? sys_errlist[errno] : "unknown host");
	if (server_read != -1)
	    put_it("*** Connection to server %s resumed...", server_list[current_server].name);
	return;
    }
    operator = 0;
    update_all_status();
    if (server_read != -1) {
	send_to_server("QUIT");
	put_it("*** Closing connection to %s", server_list[current_server].name);
	close_server();
	sleep(3);		/* a little time for the quit to get around */
    }
    add_to_server_list(server_name, port, null(char *), 1);
    server_read = server_write = new_des;

    send_to_server("NICK %s", nickname);
    if (server_list[current_server].password)
	send_to_server("PASS %s", server_list[current_server].password);
    send_to_server("USER %s %s %s :%s", username, hostname, server_list[current_server].name, realname);
#ifdef put_this_here_when_2.6_is_everywhere
    if (irc2_6)
	reconnect_all_channels();
    else {
	if (current_channel(null(char *)))
	    send_to_server("CHANNEL %s", current_channel(null(char *)));
    }
    do_hook(CONNECT_LIST, server_name, empty_string);
#endif put_this_here_when_2.6_is_everywhere
    if (away_message)
	send_to_server("AWAY %s", away_message);
}

/*
 * connect_to_server_process: handles the tcp connection to a server.  If
 * successful, the user is disconnected from any previously connected server,
 * the new server is added to the server list, and the user is registered on
 * the new server.  If connection to the server is not successful,  the
 * reason for failure is displayed and the previous server connection is
 * resumed uniterrupted. 
 *
 * This version of connect_to_server() uses the ircserv process to talk to a
 * server 
 */
static void connect_to_server_process(server_name, port)
char *server_name;
int port;
{
    int write_des[2],
        read_des[2],
        c;
    char *path;

    path = IRCSERV_PATH;
    if (*path == null(char)) {
	connect_to_server_direct(server_name, port);
	return;
    }
    using_server_process = 1;
    oper_command = 0;
    write_des[0] = -1;
    write_des[1] = -1;
    if (pipe(write_des) || pipe(read_des)) {
	if (write_des[0] != -1) {
	    close(write_des[0]);
	    close(write_des[1]);
	}
	put_it("*** Couldn't start new process: %s", sys_errlist[errno]);
	connect_to_server_direct(server_name, port);
	return;
    }
    switch (fork()) {
	case -1:
	    put_it("*** Couldn't start new process: %s\n", sys_errlist[errno]);
	    return;
	case 0:
	    signal(SIGINT, SIG_IGN);
	    dup2(read_des[1], 1);
	    dup2(write_des[0], 0);
	    close(read_des[0]);
	    close(write_des[1]);
	    sprintf(buffer, "%u", port);
	    execl(path, "ircserv", server_name, buffer, null(char *));
	    printf("-5 0\n");	/* -1 thru -4 returned by connect_by_number() */
	    fflush(stdout);
	    _exit(1);
	default:
	    close(read_des[1]);
	    close(write_des[0]);
	    break;
    }
    c = dgets(buffer, BUFFER_SIZE, read_des[0]);
    if ((c == 0) || ((c = atoi(buffer)) != 0)) {
	if (c == -5) {
	    connect_to_server_direct(server_name, port);
	    return;
	} else {
	    char *ptr;

	    if (ptr = index(buffer, ' ')) {
		ptr++;
		if (atoi(ptr) > 0)
		    put_it("*** Unable to connect to port %d of server %s: %s", port, server_name, sys_errlist[atoi(ptr)]);
		else
		    put_it("*** Unable to connect to port %d of server %s: Unknown host", port, server_name);
	    } else
		put_it("*** Unable to connect to port %d of server %s: Unknown host", port, server_name);
	    if (server_read != -1)
		put_it("*** Connection to server %s resumed...", server_list[current_server].name);
	    return;
	}
    }
    operator = 0;
    update_all_status();
    if (server_read != -1) {
	send_to_server("QUIT");
	put_it("*** Closing connection to %s", server_list[current_server].name);
	close_server();
	sleep(3);		/* a little time for the quit to get around */
    }
    add_to_server_list(server_name, port, null(char *), 1);
    server_read = read_des[0];
    server_write = write_des[1];
    send_to_server("NICK %s", nickname);
    if (server_list[current_server].password)
	send_to_server("PASS %s", server_list[current_server].password);
    send_to_server("USER %s %s %s :%s", username, hostname, server_list[current_server].name, realname);
#ifdef put_this_here_when_2.6_is_everywhere
    if (irc2_6)
	reconnect_all_channels();
    else {
	if (current_channel(null(char *)))
	    send_to_server("CHANNEL %s", current_channel(null(char *)));
    }
    do_hook(CONNECT_LIST, server_name, empty_string);
#endif put_this_here_when_2.6_is_everywhere
    if (away_message)
	send_to_server("AWAY %s", away_message);
}

void connect_to_server(server_name, port)
char *server_name;
int port;
{
    put_it("*** Connecting to port %d of server %s", port, server_name);
    if (using_server_process)
	connect_to_server_process(server_name, port);
    else
	connect_to_server_direct(server_name, port);
}

/*
 * get_connected: using the connect_to_server() function, this attempts to
 * connect to the server indicated by the server parameter, which is an index
 * into the server list.  If connection is a success, all is well (see
 * connect_to_server()).  If connection fails and the user was previously
 * connected to a server, connection is resumed.  If connection fails and the
 * user has no current connection (server_read == -1), this will try each
 * server in the server list until one is found to which a connection can be
 * established. 
 */
void get_connected(server)
int server;
{
    int s;

    if (server == number_of_servers)
	server = 0;
    else if (server < 0)
	server = number_of_servers - 1;
    s = server;
    connect_to_server(server_list[server].name,
		      server_list[server].port);
    if (server_read == -1) {
	while (server_read == -1) {
	    server++;
	    if (server == number_of_servers)
		server = 0;
	    if (server == s) {
		put_it("*** Use /SERVER to connect to server");
		break;
	    }
	    connect_to_server(server_list[server].name,
			      server_list[server].port);
	}
	current_server = server;
    }
}

/*
 * read_server_file: reads hostname:portnum:password server information from
 * a file and adds this stuff to the server list.  See build_server_list()/ 
 */
int read_server_file(name)
char *name;
{
    FILE *fp;
    char format[11];

    sprintf(format, "%%%ds", BUFFER_SIZE);
    if (fp = fopen(name, "r")) {
	while (fscanf(fp, format, buffer) != EOF)
	    build_server_list(buffer);
	fclose(fp);
	return (0);
    }
    return (1);
}

/* display_server_list: just guess what this does */
void display_server_list()
{
    int i;

    put_it("*** Current server: %s %d", server_list[current_server].name,
	   server_list[current_server].port);
    put_it("*** Server list:");
    for (i = 0; i < number_of_servers; i++)
	put_it("***\t%d) %s %d", i, server_list[i].name, server_list[i].port);
}

/*
 * current_server_name: If name is not null, the current server name is
 * changed to name and the new name is returned as the function value.  If
 * name is null, the current server name is returned as the function value 
 */
char *current_server_name(name)
char *name;
{
    if (name)
	malloc_strcpy(&(server_list[current_server].name), name);
    return (server_list[current_server].name);
}

/*
 * current_server_password: If password is not null, the current servers
 * password is changed to the new password and the new password is returned
 * as the function value.  If password is null, the old password is returned
 * as the function balue 
 */
char *current_server_password(password)
char *password;
{
    if (password)
	malloc_strcpy(&(server_list[current_server].password), password);
    return (server_list[current_server].password);
}

/*
 * current_server_index: returns the index of the current server in the
 * server list.  Used with get_connected() 
 */
int current_server_index()
{
        return (current_server);
}

/* server_list_size: returns the number of servers in the server list */
int server_list_size()
{
        return (number_of_servers);
}

/* send_to_server: sends the given info the the server */
void send_to_server(format, arg1, arg2, arg3, arg4, arg5,
		         arg6, arg7, arg8, arg9, arg10)
char *format;
int arg1,
    arg2,
    arg3,
    arg4,
    arg5,
    arg6,
    arg7,
    arg8,
    arg9,
    arg10;
{
    char buffer[BUFFER_SIZE + 1];	/* make this buffer *much* bigger
					 * than needed */

    if (server_write != -1) {
	sprintf(buffer, format, arg1, arg2, arg3, arg4, arg5,
		arg6, arg7, arg8, arg9, arg10);
	strmcat(buffer, "\n", BUFFER_SIZE);
	write(server_write, buffer, strlen(buffer));
    } else
	put_it("*** You are not connected to a server, use /SERVER to connect.");
}
