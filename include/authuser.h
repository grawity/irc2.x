#ifndef AUTHUSER_H
#define AUTHUSER_H

extern unsigned short auth_tcpport;
extern char *auth_xline();
extern int auth_fd();
extern int auth_fd2();
extern int auth_tcpsock();
extern char *auth_tcpuser();
extern char *auth_tcpuser2();
extern char *auth_tcpuser3();
extern char *auth_sockuser();

#endif

