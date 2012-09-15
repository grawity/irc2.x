/*
 * crypt.h: header for crypt.c 
 *
 * Written By Michael Sandrof <ms5n@andrew.cmu.edu> 
 *
 * Copyright(c) 1990 
 *
 * See the COPYRIGHT file, or do a HELP IRCII COPYRIGHT 
 */

extern char *crypt_msg();
extern void encrypt_cmd();
extern char *is_crypted();

#define CRYPT_HEADER ""
#define CRYPT_HEADER_LEN 5
