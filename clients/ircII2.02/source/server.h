/*
 * server.h: header for server.c 
 *
 * Written By Michael Sandrof <ms5n@andrew.cmu.edu> 
 *
 * Copyright(c) 1990 
 *
 * See the COPYRIGHT file, or do a HELP IRCII COPYRIGHT 
 */

extern void add_to_server_list();
extern void build_server_list();
extern void connect_to_server();
extern void get_connected();
extern int read_server_file();
extern void display_server_list();
extern char *current_server_name();
extern char *current_server_password();
extern int current_server_index();
extern int server_list_size();
extern void send_to_server();

extern int server_read, server_write;
