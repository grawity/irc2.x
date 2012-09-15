/*
 * newio.c: This is some handy stuff to deal with file descriptors in a way
 * much like stdio's FILE pointers 
 *
 * IMPORTANT NOTE:  If you use the routines here-in, you shouldn't switch to
 * using normal reads() on the descriptors cause that will cause bad things
 * to happen.  If using any of these routines, use them all 
 *
 * Written By Michael Sandrof <ms5n@andrew.cmu.edu> 
 *
 * Copyright(c) 1990 
 *
 * See the COPYRIGHT file, or do a HELP IRCII COPYRIGHT 
 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/time.h>
#ifdef ISC22
#include <sys/bsdtypes.h>
#endif

#define null(foo) (foo) 0
#define BUFFER_SIZE 512

typedef struct myio_struct {
    char buffer[BUFFER_SIZE];
    int read_pos,
        write_pos;
}           MyIO;

static struct timeval right_away = {0L, 0L};
static MyIO *io_rec[FD_SETSIZE];
static char first = 1;

/*
 * dgets: works just like fgets. 
 */
int dgets(str, len, des)
char *str;
int len,
    des;
{
    char *ptr;
    char done = 0;
    int cnt = 0,
        c;

    if (first) {
	for (c = 0; c < FD_SETSIZE; c++)
	    io_rec[c] = null(MyIO *);
	first = 0;
    }
    if (io_rec[des] == null(MyIO *)) {
	io_rec[des] = (MyIO *) new_malloc(sizeof(MyIO));
	io_rec[des]->read_pos = 0;
	io_rec[des]->write_pos = 0;
    }
    while (!done) {
	if (io_rec[des]->read_pos == io_rec[des]->write_pos) {
	    io_rec[des]->read_pos = 0;
	    io_rec[des]->write_pos = 0;
	    c = read(des, io_rec[des]->buffer, BUFFER_SIZE);
	    if (!(c > 0))
		return (c);
	    io_rec[des]->write_pos += c;
	}
	ptr = io_rec[des]->buffer;
	while (io_rec[des]->read_pos < io_rec[des]->write_pos) {
	    if (((str[cnt++] = ptr[(io_rec[des]->read_pos)++]) == '\n') ||
		(cnt == len)) {
		done = 1;
		break;
	    }
	}
    }
    str[cnt] = null(char);
    return (cnt);
}

/*
 * new_select: works just like select(), execpt I trimmed out the excess
 * parameters I didn't need.  
 */
int new_select(rd, wd, timeout)
fd_set *rd,
      *wd;
struct timeval *timeout;
{
    int i,
        set = 0;
    fd_set new;

    FD_ZERO(&new);
    for (i = 0; i < FD_SETSIZE; i++) {
	if (io_rec[i]) {
	    if (io_rec[i]->read_pos < io_rec[i]->write_pos) {
		FD_SET(i, &new);
		set = 1;
	    }
	}
    }
    if (set) {
	set = 0;
	if (!(select(FD_SETSIZE, rd, wd, 0, &right_away) > 0))
	    FD_ZERO(rd);
	for (i = 0; i < FD_SETSIZE; i++) {
	    if ((FD_ISSET(i, rd)) || (FD_ISSET(i, &new))) {
		set++;
		FD_SET(i, rd);
	    } else
		FD_CLR(i, rd);
	}
	return (set);
    }
    return (select(FD_SETSIZE, rd, wd, 0, timeout));
}

/* new_close: works just like close */
void new_close(des)
int des;
{
    new_free(&(io_rec[des]));
    close(des);
}
