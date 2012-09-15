/*
 * exec.c: handles exec'd process for IRCII 
 *
 * Written By Michael Sandrof <ms5n@andrew.cmu.edu> 
 *
 * Copyright(c) 1990 
 *
 * See the COPYRIGHT file, or do a HELP IRCII COPYRIGHT 
 */
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#ifdef ISC22
#include <sys/bsdtypes.h>
#endif
#include "irc.h"
#include "exec.h"
#include "vars.h"
#include "ircaux.h"
#include "edit.h"
#include "window.h"

/* Process: the structure that has all the info needed for each process */
typedef struct {
    char *name;			/* full process name */
    int pid;			/* process-id of process */
    int stdin;			/* stdin description for process */
    int stdout;			/* stdout descriptor for process */
    int stderr;			/* stderr descriptor for process */
    int counter;		/* output line counter for process */
    char *redirect;		/* redirection command (MSG, NOTICE) */
    unsigned int refnum;	/* a window for output to go to */
    char *who;			/* nickname used for redirection */
    char exited;		/* true if process has exited */
    char termsig;		/* The signal that terminated the process */
    char retcode;		/* return code of process */
}      Process;

static Process **process_list = null(Process **);
static int process_list_size = 0;

/*
 * A nice array of the possible signals.  Used by the coredump trapping
 * routines and in the exec.c package 
 */
char *signals[] =
{null(char *), "HUP", "INT", "QUIT",
 "ILL", "TRAP", "IOT", "EMT",
 "FPE", "KILL", "BUS", "SEGV",
 "SYS", "PIPE", "ALRM", "TERM",
 "URG", "STOP", "TSTP", "CONT",
 "CHLD", "TTIN", "TTOU", "IO",
 "XCPU", "XFSZ", "VTALRM", "PROF",
 "WINCH", "LOST", "USR1", "USR2"};

/*
 * exec_close: silly, eh?  Well, it makes the code look nicer.  Or does it
 * really?  No.  But what the hell 
 */
int exec_close(des)
int des;
{
    if (des != -1)
	new_close(des);
    return (-1);
}

/*
 * valid_process_index: checks to see if index refers to a valid running
 * process and returns true if this is the case.  Returns false otherwise 
 */
static int valid_process_index(index)
int index;
{
    if ((index < 0) || (index >= process_list_size))
	return (0);
    if (process_list[index])
	return (1);
    return (0);
}

/*
 * check_wait_status: This waits on child processes, reporting terminations
 * as needed, etc 
 */
void check_wait_status()
{
#ifndef ISC22
    union wait status;
#else
    int status;
#endif
    int pid,
        i;

#if defined(mips) || defined(MUNIX)
    if ((pid = wait2(&status, WNOHANG, 0)) > 0) {
#else
#ifdef ISC22
    if ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
#else
    if ((pid = wait3(&status, WNOHANG, 0)) > 0) {
#endif
#endif mips
	errno = 0;		/* reset errno, cause wait3 changes it */
	for (i = 0; i < process_list_size; i++) {
	    if (process_list[i] && (process_list[i]->pid == pid)) {
		process_list[i]->exited = 1;
#ifndef ISC22
		process_list[i]->termsig = status.w_T.w_Termsig;
		process_list[i]->retcode = status.w_T.w_Retcode;
#else
		process_list[i]->termsig = WTERMSIG(status);
		process_list[i]->retcode = WEXITSTATUS(status);
#endif
		if ((process_list[i]->stderr == -1) &&
		    (process_list[i]->stdout == -1))
		    delete_process(i);
		return;
	    }
	}
    }
}

/*
 * check_process_limits: checks each running process to see if it's reached
 * the user selected maximum number of output lines.  If so, the processes is
 * effectively killed 
 */
void check_process_limits()
{
    int limit;
    int i;

    if ((limit = get_int_var(SHELL_LIMIT_VAR)) && process_list) {
	for (i = 0; i < process_list_size; i++) {
	    if (process_list[i]) {
		if (process_list[i]->counter >= limit) {
		    process_list[i]->stdin = exec_close(process_list[i]->stdin);
		    process_list[i]->stdout = exec_close(process_list[i]->stdout);
		    process_list[i]->stderr = exec_close(process_list[i]->stderr);
		    if (process_list[i]->exited)
			delete_process(i);
		}
	    }
	}
    }
}

/*
 * do_processes: given a set of read-descriptors (as returned by a call to
 * select()), this will determine which of the process has produced output
 * and will hadle it appropriately 
 */
void do_processes(rd)
fd_set *rd;
{
    int i;
    char buffer[BUFFER_SIZE + 1],
        *nick;

    if (process_list == null(Process **))
	return;
    for (i = 0; i < process_list_size; i++) {
	if (process_list[i] && (process_list[i]->stdout != -1)) {
	    if (FD_ISSET(process_list[i]->stdout, rd)) {
		switch (dgets(buffer, BUFFER_SIZE, process_list[i]->stdout)) {
		    case -1:
			break;
		    case 0:
			if (process_list[i]->stderr == -1) {
			    process_list[i]->stdin = exec_close(process_list[i]->stdin);
			    process_list[i]->stdout = exec_close(process_list[i]->stdout);
			    if (process_list[i]->exited)
				delete_process(i);
			} else
			    process_list[i]->stdout = exec_close(process_list[i]->stdout);
			break;
		    default:
			message_to(process_list[i]->refnum);
			process_list[i]->counter++;
			if (*(buffer + strlen(buffer) - 1) == '\n')
			    *(buffer + strlen(buffer) - 1) = null(char);
			if (process_list[i]->redirect) {
			    if (process_list[i]->who)
				send_text(process_list[i]->who, buffer,
					  process_list[i]->redirect);
			    else {
				if ((nick = query_nick()) && (*nick == '%'))
				    send_text(null(char *), buffer, process_list[i]->redirect);
				else
				    send_text(query_nick(), buffer, process_list[i]->redirect);
			    }
			} else
			    put_it("%s", buffer);
			message_to(null(Window *));
			break;
		}
	    }
	}
	if (process_list && (i < process_list_size) &&
	    process_list[i] && (process_list[i]->stderr != -1)) {
	    if (FD_ISSET(process_list[i]->stderr, rd)) {
		switch (dgets(buffer, BUFFER_SIZE, process_list[i]->stderr)) {
		    case -1:
			break;
		    case 0:
			if (process_list[i]->stdout == -1) {
			    process_list[i]->stderr = exec_close(process_list[i]->stderr);
			    process_list[i]->stdin = exec_close(process_list[i]->stdin);
			    if (process_list[i]->exited)
				delete_process(i);
			} else
			    process_list[i]->stderr = exec_close(process_list[i]->stderr);
			break;
		    default:
			message_to(process_list[i]->refnum);
			(process_list[i]->counter)++;
			if (*(buffer + strlen(buffer) - 1) == '\n')
			    *(buffer + strlen(buffer) - 1) = null(char);
			if (process_list[i]->redirect) {
			    if (process_list[i]->who)
				send_text(process_list[i]->who, buffer, process_list[i]->redirect);
			    else {
				if ((nick = query_nick()) && (*nick == '%'))
				    send_text(null(char *), buffer, process_list[i]->redirect);
				else
				    send_text(query_nick(), buffer, process_list[i]->redirect);
			    }
			} else
			    put_it("%s", buffer);
			message_to(null(Window *));
			break;
		}
	    }
	}
    }
}

/*
 * set_process_bits: This will set the bits in a fd_set map for each of the
 * process descriptors. 
 */
void set_process_bits(rd)
fd_set *rd;
{
    int i;

    if (process_list) {
	for (i = 0; i < process_list_size; i++) {
	    if (process_list[i]) {
		if (process_list[i]->stdout != -1)
		    FD_SET(process_list[i]->stdout, rd);
		if (process_list[i]->stderr != -1)
		    FD_SET(process_list[i]->stderr, rd);
	    }
	}
    }
}

/*
 * list_processes: displays a list of all currently running processes,
 * including index number, pid, and process name 
 */
static void list_processes()
{
    int i;

    if (process_list) {
	put_it("*** Process List:");
	for (i = 0; i < process_list_size; i++) {
	    if (process_list[i])
		put_it("***\t%d: %s", i, process_list[i]->name);
	}
    } else
	put_it("*** No processes are running");
}

/*
 * delete_process: Removes the process specifed by index from the process
 * list.  The does not kill the process, close the descriptors, or any such
 * thing.  It only deletes it from the list.  If appropriate, this will also
 * shrink the list as needed 
 */
static int delete_process(index)
int index;
{
    if (process_list) {
	if (index >= process_list_size)
	    return (-1);
	if (process_list[index]) {
	    if (get_int_var(NOTIFY_ON_TERMINATION_VAR)) {
		if (process_list[index]->termsig)
		    put_it("*** Process %d (%s) terminated with signal %s (%d)",
			   index, process_list[index]->name,
			   signals[process_list[index]->termsig],
			   process_list[index]->termsig);
		else
		    put_it("*** Process %d (%s) terminated with return code %d",
			   index, process_list[index]->name,
			   process_list[index]->retcode);
	    }
	    new_free(&(process_list[index]->name));
	    new_free(&(process_list[index]->who));
	    new_free(&(process_list[index]));
	    process_list[index] = null(Process *);
	    if (index == (process_list_size - 1)) {
		int i;

		for (i = process_list_size - 1; process_list_size; process_list_size--, i--) {
		    if (process_list[i])
			break;
		}
		if (process_list_size)
		    process_list = (Process **) new_realloc(process_list, sizeof(Process *) * process_list_size);
		else {
		    new_free(&process_list);
		    process_list = null(Process **);
		}
	    }
	    return (0);
	}
    }
    return (-1);
}

/*
 * add_process: adds a new process to the process list, expanding the list as
 * needed.  It will first try to add the process to a currently unused spot
 * in the process table before it increases it's size. 
 */
static void add_process(name, pid, stdin, stdout, stderr, redirect, who, refnum)
char *name;
int pid,
    stdin,
    stdout,
    stderr;
char *redirect;
char *who;
unsigned int refnum;
{
    int i;

    if (process_list == null(Process **)) {
	process_list = (Process **) new_malloc(sizeof(Process *));
	process_list_size = 1;
	process_list[0] = null(Process *);
    }
    for (i = 0; i < process_list_size; i++) {
	if (process_list[i] == null(Process *)) {
	    process_list[i] = (Process *) new_malloc(sizeof(Process));
	    process_list[i]->name = null(char *);
	    malloc_strcpy(&(process_list[i]->name), name);
	    process_list[i]->pid = pid;
	    process_list[i]->stdin = stdin;
	    process_list[i]->stdout = stdout;
	    process_list[i]->stderr = stderr;
	    process_list[i]->refnum = refnum;
	    process_list[i]->redirect = null(char *);
	    if (redirect)
		malloc_strcpy(&(process_list[i]->redirect), redirect);
	    process_list[i]->counter = 0;
	    process_list[i]->exited = 0;
	    process_list[i]->termsig = 0;
	    process_list[i]->retcode = 0;
	    process_list[i]->who = null(char *);
	    if (who)
		malloc_strcpy(&(process_list[i]->who), who);
	    return;
	}
    }
    process_list_size++;
    process_list = (Process **) new_realloc(process_list, sizeof(Process *) * process_list_size);
    process_list[process_list_size - 1] = null(Process *);
    process_list[i] = (Process *) new_malloc(sizeof(Process));
    process_list[i]->name = null(char *);
    malloc_strcpy(&(process_list[i]->name), name);
    process_list[i]->pid = pid;
    process_list[i]->stdin = stdin;
    process_list[i]->stdout = stdout;
    process_list[i]->stderr = stderr;
    process_list[i]->refnum = refnum;
    process_list[i]->redirect = null(char *);
    if (redirect)
	malloc_strcpy(&(process_list[i]->redirect), redirect);
    process_list[i]->counter = 0;
    process_list[i]->exited = 0;
    process_list[i]->termsig = 0;
    process_list[i]->retcode = 0;
    process_list[i]->who = null(char *);
    if (who)
	malloc_strcpy(&(process_list[i]->who), who);
}

/*
 * kill_process: sends the given signal to the process specified by the given
 * index into the process table.  After the signal is sent, the process is
 * deleted from the process table 
 */
static void kill_process(index, signal)
int index,
    signal;
{
    if (process_list && (index < process_list_size) && process_list[index]) {
	put_it("*** Sending signal %s (%d) to process %d: %s",
	       signals[signal], signal, index, process_list[index]->name);
#ifdef mips
	kill(getpgrp(process_list[index]->pid), signal);
#else
#if defined(ISC22) || defined(MUNIX)
	kill(-getpgrp(process_list[index]->pid), signal);
#else
	killpg(getpgrp(process_list[index]->pid), signal);
#endif
#endif mips
    } else
	put_it("*** There is no process %d", index);
}

/*
 * start_process: Attempts to start the given process using the SHELL as set
 * by the user. 
 */
static void start_process(name, redirect, who, refnum)
char *name,
    *who,
    *redirect;
unsigned int refnum;
{

    int p0[2],
        p1[2],
        p2[2],
        pid;
    char *shell,
        *flag;

    p0[0] = p0[1] = -1;
    p1[0] = p1[1] = -1;
    p2[0] = p2[1] = -1;
    if (pipe(p0) || pipe(p1) || pipe(p2)) {
	put_it("*** Unable to start new process: %s", sys_errlist[errno]);
	if (p0[0] != -1) {
	    close(p0[0]);
	    close(p0[1]);
	}
	if (p1[0] != -1) {
	    close(p1[0]);
	    close(p1[1]);
	}
	if (p2[0] != -1) {
	    close(p2[0]);
	    close(p2[1]);
	}
	return;
    }
    switch (pid = fork()) {
	case -1:
	    put_it("*** Couldn't start new process!");
	    break;
	case 0:
	    setpgrp(0, getpid());
	    signal(SIGINT, SIG_IGN);
	    dup2(p0[0], 0);
	    dup2(p1[1], 1);
	    dup2(p2[1], 2);
	    close(p0[1]);
	    close(p1[0]);
	    close(p2[0]);

	    if ((shell = get_string_var(SHELL_VAR)) == null(char *)) {
		char **args = null(char **),
		    *arg;
		int cnt,
		    max;

		cnt = 0;
		max = 5;
		args = (char **) new_malloc(sizeof(char *) * max);
		while (arg = next_arg(name, &name)) {
		    if (cnt == max) {
			max += 5;
			args = (char **) new_realloc(args, max);
		    }
		    args[cnt++] = arg;
		}
		args[cnt] = null(char *);
		execv(args[0], args);
	    } else {
		if ((flag = get_string_var(SHELL_FLAGS_VAR)) == null(char *))
		    flag = empty_string;
		execl(shell, shell, flag, name, null(char *));
	    }
	    sprintf(buffer, "*** Error starting shell \"%s\": %s", shell, sys_errlist[errno]);
	    write(1, buffer, strlen(buffer));
	    _exit(0);
	    break;
	default:
	    close(p0[0]);
	    close(p1[1]);
	    close(p2[1]);
	    add_process(name, pid, p0[1], p1[0], p2[0], redirect, who, refnum);
	    break;
    }
}

/*
 * text_to_process: sends the given text to the given process.  If the given
 * process index is not valid, an error is reported and 1 is returned.
 * Otherwise 0 is returned. 
 */
int text_to_process(index, text)
int index;
char *text;
{
    if (valid_process_index(index) == 0) {
	put_it("*** No such process number %d", index);
	return (1);
    }
    write(process_list[index]->stdin, text, strlen(text));
    write(process_list[index]->stdin, "\n", 1);
    return (0);
}


/*
 * exec: the /EXEC command.  Handles the whole IRCII process crap. 
 */
void exec(command, args)
char *command,
    *args;
{
    char *who = null(char *),
        *redirect = null(char *),
        *flag,
        *index;
    unsigned int refnum = 0;

#ifdef DAEMON_UID
    if (getuid() == DAEMON_UID) {
	put_it("*** You are not permitted to use EXEC.");
	return;
    }
#endif DAEMON_UID
    if (*args == null(char)) {
	list_processes();
	return;
    }
    redirect = null(char *);
    while ((*args == '-') && (flag = next_arg(args, &args))) {
	int len;

	if (*flag == '-') {
	    len = strlen(++flag);
	    if (strnicmp(flag, "OUT", len) == 0) {
		redirect = "MSG";
		who = null(char *);
	    } else if (strnicmp(flag, "WINDOW", len) == 0) {
		refnum = current_refnum();
	    } else if (strnicmp(flag, "MSG", len) == 0) {
		redirect = "PRIVMSG";
		if ((who = next_arg(args, &args)) == null(char *)) {
		    put_it("*** No nicknames specified");
		    return;
		}
	    } else if (strnicmp(flag, "NOTICE", len) == 0) {
		redirect = "NOTICE";
		if ((who = next_arg(args, &args)) == null(char *)) {
		    put_it("*** No nicknames specified");
		    return;
		}
	    } else if (strnicmp(flag, "IN", len) == 0) {
		if (index = next_arg(args, &args)) {
		    int i;

		    i = atoi(index);
		    if (text_to_process(i, args))
			return;
		} else
		    put_it("*** Missing process number");
		return;
	    } else {
		int sig,
		    i;

		if ((index = next_arg(args, &args)) == null(char *)) {
		    put_it("*** Missing process number");
		    return;
		}
		i = atoi(index);
		if (valid_process_index(i) == 0) {
		    put_it("*** No such process number: %d", i);
		    return;
		}
		if ((sig = atoi(flag)) > 0) {
		    if ((sig > 0) && (sig < NSIG))
			kill_process(i, sig);
		    else
			put_it("*** Signal number can be from 1 to %d", NSIG);
		    return;
		}
		for (sig = 1; sig < NSIG; sig++) {
		    if (strnicmp(signals[sig], flag, len) == 0) {
			kill_process(i, sig);
			return;
		    }
		}
		put_it("*** No such signal: %s", flag);
		return;
	    }
	} else
	    break;
    }
    if (is_number(args)) {
	int i;

	i = atoi(args);
	if (valid_process_index(i)) {
	    malloc_strcpy(&(process_list[i]->redirect), redirect);
	    malloc_strcpy(&(process_list[i]->who), who);
	    process_list[i]->refnum = refnum;
	    message_to(refnum);
	    if (refnum)
		put_it("*** Output from process %d (%s) now going to this window", i, process_list[i]->name);
	    else
		put_it("*** Output from process %d (%s) not going to any window", i, process_list[i]->name);
	    if (redirect) {
		if (who)
		    put_it("*** Output from process %d (%s) now going to %s", i, process_list[i]->name, who);
		else
		    put_it("*** Output from process %d (%s) now going to your channel", i, process_list[i]->name);
	    } else
		put_it("*** Output from process %d (%s) now going to you", i, process_list[i]->name);
	    message_to(null(Window *));
	} else
	    put_it("*** No such processes number %d", i);
    } else
	start_process(args, redirect, who, refnum);
}
