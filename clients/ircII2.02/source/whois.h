/*
 * whois.h: header for whois.c 
 *
 *
 * Written By Michael Sandrof <ms5n@andrew.cmu.edu> 
 *
 * Copyright(c) 1990 
 *
 * See the COPYRIGHT file, or do a HELP IRCII COPYRIGHT 
 */

extern void add_to_whois_queue();
extern void whois_name();
extern void whowas_name();
extern void whois_server();
extern void whois_oper();
extern void whois_lastcom();
extern void whois_ignore_msgs();
extern void whois_ignore_notices();
extern void whois_ignore_walls();
extern void whois_ignore_wallops();
extern void whois_ignore_invites();
extern void whois_join();
extern void whois_privmsg();
extern void whois_notify();
extern void whois_new_wallops();
extern void clean_whois_queue();
