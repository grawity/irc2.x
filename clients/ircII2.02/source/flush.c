/*
 * Flush: A little program that tricks another program into line buffering
 * its output. 
 *
 * By Michael Sandrof 
 */
#include <stdio.h>
#include <sys/file.h>
#include <signal.h>

#define BUFFER_SIZE 1024

/* descriptors of the tty and pty */
int master,
    slave;

void death()
{
    close(0);
}

/*
 * setup_master_slave: this searches for an open tty/pty pair, opening the
 * pty as the master device and the tty as the slace device 
 */
void setup_master_slave()
{
    char line[11];
    char linec;
    int linen;

    for (linec = 'p'; linec <= 's'; linec++) {
	sprintf(line, "/dev/pty%c0", linec);
	if (access(line, 0) != 0)
	    break;
	for (linen = 0; linen < 16; linen++) {
	    sprintf(line, "/dev/pty%c%1x", linec, linen);
	    if ((master = open(line, O_RDWR)) >= 0) {
		sprintf(line, "/dev/tty%c%1x", linec, linen);
		if (access(line, R_OK | W_OK) == 0) {
		    if ((slave = open(line, O_RDWR)) >= 0)
			return;
		}
		close(master);
	    }
	}
    }
    fprintf(stderr, "flush: Can't find a pty\n");
    exit(0);
}

/*
 * What's the deal here?  Well, it's like this.  First we find an open
 * tty/pty pair.  Then we fork three processes.  The first reads from stdin
 * and sends the info to the master device.  The next process reads from the
 * master device and sends stuff to stdout.  The last processes is the rest
 * of the command line arguments exec'd.  By doing all this, the exec'd
 * process is fooled into flushing each line of output as it occurs.  
 */
main(argc, argv)
int argc;
char **argv;
{
    char buffer[BUFFER_SIZE];
    int cnt,
        pid;

    if (argc < 2) {
	fprintf(stderr, "Usage: flush [program] [arguments to program]\n");
	exit(1);
    }
    setup_master_slave();
    switch (pid = fork()) {
	case -1:
	    fprintf(stderr, "flush: Unable to fork process!\n");
	    exit(1);
	case 0:
	    switch (pid = fork()) {
		case -1:
		    fprintf(stderr, "flush: Unable to fork process!\n");
		    exit(1);
		case 0:
		    dup2(slave, 0);
		    dup2(slave, 1);
		    dup2(slave, 2);
		    close(master);
		    execvp(argv[1], &(argv[1]));
		    fprintf(stderr, "flush: Error exec'ing process!\n");
		    exit(1);
		    break;
		default:
		    close(slave);
		    close(0);
		    while ((cnt = read(master, buffer, BUFFER_SIZE)) > 0)
			write(1, buffer, cnt);
		    close(master);
		    kill(pid,SIGKILL);
		    wait(0);
		    exit(0);
		    break;
	    }
	    break;
	default:
	    signal(SIGCHLD,death);
	    close(slave);
	    while ((cnt = read(0, buffer, BUFFER_SIZE)) > 0)
		write(master, buffer, cnt);
	    close(master);
	    kill(pid, SIGKILL);
	    wait(0);
	    exit(0);
	    break;
    }
}





