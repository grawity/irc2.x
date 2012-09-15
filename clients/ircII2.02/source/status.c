/*
 * status.c: handles the status line updating, etc for IRCII 
 *
 * Written By Michael Sandrof <ms5n@andrew.cmu.edu> 
 *
 * Copyright(c) 1990 
 *
 * See the COPYRIGHT file, or do a HELP IRCII COPYRIGHT 
 */
#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/time.h>
#ifdef _AUX_SOURCE
#include <time.h>
#endif _AUX_SOURCE
#include <signal.h>
#include "irc.h"
#include "term.h"
#include "status.h"
#include "vars.h"
#include "hook.h"
#include "input.h"
#include "edit.h"
#include "window.h"
#include "mail.h"

extern char *update_clock();
extern char *convert_format();
extern char *status_nickname();
extern char *status_query_nick();
extern char *status_channel();
extern char *status_insert_mode();
extern char *status_overwrite_mode();
extern char *status_away();
extern char *status_oper();
extern char *status_user();
extern char *status_hold();
extern char *status_version();
extern char *status_clock();
extern char *status_window();
extern char *status_mail();
extern char *status_null_function();


/*
 * Maximum number of "%" expressions in a status line format.  If you change
 * this number, you must manually change the sprintf() in make_status 
 */
#define MAX_FUNCTIONS 20

/* The format statements to build each portion of the status line */
static char *status_format = null(char *);
static char *query_format = null(char *);
static char *clock_format = null(char *);
static char *channel_format = null(char *);
static char *mail_format = null(char *);

/*
 * status_func: The list of status line function in the proper order for
 * display.  This list is set in convert_format() 
 */
static char *(*status_func[MAX_FUNCTIONS]) ();
/* func_cnt: the number of status line functions assigned */
static int func_cnt;

static int alarm_hours,		/* hour setting for alarm in 24 hour time */
    alarm_minutes;		/* minute setting for alarm */
static struct itimerval clock_timer = {10L, 0L, 1L, 0L};	/* the alarm timer stuff */

/* alarmed: This is called whenever a SIGALRM is received and the alarm is on */
static void alarmed()
{
         put_it("*** The time is %s", update_clock(1));
    term_beep();
    term_beep();
    term_beep();
}

/*
 * alarm_switch: turns on and off the alarm display.  Sets the system timer
 * and sets up a signal to trap SIGALRMs.  If flag is 1, the alarmed()
 * routine will be activated ever 10 seconds or so.  If flag is 0, the timer
 * and signal stuff are reset 
 */
static alarm_switch(flag)
char flag;
{
    static char alarm_on = 0;

    if (flag) {
	if (!alarm_on) {
	    setitimer(ITIMER_REAL, &clock_timer, null(struct itimervar *));
	    signal(SIGALRM, alarmed);
	    alarm_on = 1;
	}
    } else if (alarm_on) {
	setitimer(ITIMER_REAL, null(struct itimervar *), null(struct itimervar *));
	signal(SIGALRM, SIG_IGN);
	alarm_on = 0;
    }
}

/*
 * set_alarm: given an input string, this checks it's validity as a clock
 * type time thingy.  It accepts two time formats.  The first is the HH:MM:XM
 * format where HH is between 1 and 12, MM is between 0 and 59, and XM is
 * either AM or PM.  The second is the HH:MM format where HH is between 0 and
 * 23 and MM is between 0 and 59.  This routine also looks for one special
 * case, "OFF", which sets the alarm string to null 
 */
void set_alarm(str)
char *str;
{
    char hours[10],
         minutes[10],
         merid[3];
    char time_str[10];
    int c,
        h,
        m,
        min_hours,
        max_hours;

    if (stricmp(str, var_settings[OFF]) == 0) {
	set_string_var(CLOCK_ALARM_VAR, null(char *));
	alarm_switch(0);
	return;
    }
    c = sscanf(str, " %2[^:]:%2[^paPA]%2s ", hours, minutes, merid);
    switch (c) {
	case 2:
	    min_hours = 0;
	    max_hours = 23;
	    break;
	case 3:
	    min_hours = 1;
	    max_hours = 12;
	    upper(merid);
	    break;
	default:
	    put_it("*** CLOCK_ALARM: Bad time format.");
	    set_string_var(CLOCK_ALARM_VAR, null(char *));
	    return;
    }

    h = atoi(hours);
    m = atoi(minutes);
    if (h >= min_hours && h <= max_hours && isdigit(hours[0]) &&
	(isdigit(hours[1]) || hours[1] == null(char))) {
	if (m >= 0 && m <= 59 && isdigit(minutes[0]) && isdigit(minutes[1])) {
	    alarm_minutes = m;
	    alarm_hours = h;
	    if (max_hours == 12) {
		if (merid[0] != 'A') {
		    if (merid[0] == 'P') {
			if (h == 12)
			    alarm_hours = 0;
			else
			    alarm_hours += 12;
		    } else {
			put_it("*** CLOCK_ALARM: alarm time must end with either \"AM\" or \"PM\"");
			set_string_var(CLOCK_ALARM_VAR, null(char *));
		    }
		}
		if (merid[1] == 'M') {
		    sprintf(time_str, "%d:%02d%s", h, m, merid);
		    set_string_var(CLOCK_ALARM_VAR, time_str);
		} else {
		    put_it("*** CLOCK_ALARM: alarm time must end with either \"AM\" or \"PM\"");
		    set_string_var(CLOCK_ALARM_VAR, null(char *));
		}
	    } else {
		sprintf(time_str, "%d:%02d", h, m);
		set_string_var(CLOCK_ALARM_VAR, time_str);
	    }
	} else {
	    put_it("*** CLOCK_ALARM: alarm minutes value must be between 0 and 59.");
	    set_string_var(CLOCK_ALARM_VAR, null(char *));
	}
    } else {
	put_it("*** CLOCK_ALARM: alarm hour value must be between %d and %d.", min_hours, max_hours);
	set_string_var(CLOCK_ALARM_VAR, null(char *));
    }
}

/* update_clock: figures out the current time and returns it in a nice format */
char *update_clock(flag)
int flag;
{
    static char time_str[10];
    static int min = -1,
        hour = -1;
    struct tm *time_val;
    char *merid;
    long t;

    t = time(0);
    time_val = localtime(&t);
    if (get_string_var(CLOCK_ALARM_VAR)) {
	if ((time_val->tm_hour == alarm_hours) &&
	    (time_val->tm_min == alarm_minutes)) {
	    alarm_switch(1);
	} else
	    alarm_switch(0);
    }
    if (flag || (time_val->tm_min != min) || (time_val->tm_hour != hour)) {
	int tmp_hour,
	    tmp_min;

	tmp_hour = time_val->tm_hour;
	tmp_min = time_val->tm_min;
	if (get_int_var(CLOCK_24HOUR_VAR))
	    merid = empty_string;
	else {
	    if (time_val->tm_hour < 12)
		merid = "AM";
	    else
		merid = "PM";
	    if (time_val->tm_hour > 12)
		time_val->tm_hour -= 12;
	    else if (time_val->tm_hour == 0)
		time_val->tm_hour = 12;
	}
	sprintf(time_str, "%d:%02d%s", time_val->tm_hour, time_val->tm_min, merid);
	if ((tmp_min != min) || (tmp_hour != hour))
	    do_hook(TIMER_LIST, time_str, empty_string);
	hour = tmp_hour;
	min = tmp_min;
	return (time_str);
    }
    if (flag)
	return (time_str);
    return (null(char *));
}

/*
 * convert_sub_format: This is used to convert the formats of the
 * sub-portions of the status line to a format statement specially designed
 * for that sub-portions.  convert_sub_format looks for a single occurence of
 * %c (where c is passed to the function). When found, it is replaced by "%s"
 * for use is a sprintf.  All other occurences of % followed by any other
 * character are left unchanged.  Only the first occurence of %c is
 * converted, all subsequence occurences are left unchanged.  This routine
 * mallocs the returned string. 
 */
static char *convert_sub_format(format, c)
char *format;
char c;
{
    char buffer[BUFFER_SIZE + 1],
        *bletch = "%% ";
    char *ptr = null(char *);
    char dont_got_it = 1;

    if (format == null(char *))
	return (null(char *));
    *buffer = null(char);
    while (format) {
	if (ptr = index(format, '%')) {
	    *ptr = null(char);
	    strmcat(buffer, format, BUFFER_SIZE);
	    *(ptr++) = '%';
	    if ((*ptr == c) && dont_got_it) {
		dont_got_it = 0;
		strmcat(buffer, "%s", BUFFER_SIZE);
	    } else {
		bletch[2] = *ptr;
		strmcat(buffer, bletch, BUFFER_SIZE);
	    }
	    ptr++;
	} else
	    strmcat(buffer, format, BUFFER_SIZE);
	format = ptr;
    }
    malloc_strcpy(&ptr, buffer);
    return (ptr);
}

static char *convert_format(str)
char *str;
{
    char buffer[BUFFER_SIZE + 1];
    char *ptr,
        *format,
        *malloc_ptr = null(char *);

    format = get_string_var(STATUS_FORMAT_VAR);
    *buffer = null(char);
    while (format) {
	if (ptr = index(format, '%')) {
	    *ptr = null(char);
	    strmcat(buffer, format, BUFFER_SIZE);
	    *(ptr++) = '%';
	    if (func_cnt < MAX_FUNCTIONS) {
		switch (*(ptr++)) {
		    case '%':
			strmcat(buffer, "%", BUFFER_SIZE);
			break;
		    case 'N':
			strmcat(buffer, "%s", BUFFER_SIZE);
			status_func[func_cnt++] = status_nickname;
			break;
		    case 'Q':
			new_free(&query_format);
			query_format = convert_sub_format(get_string_var(STATUS_QUERY_VAR), 'Q');
			strmcat(buffer, "%s", BUFFER_SIZE);
			status_func[func_cnt++] = status_query_nick;
			break;
		    case 'C':
			new_free(&channel_format);
			channel_format = convert_sub_format(get_string_var(STATUS_CHANNEL_VAR), 'C');
			strmcat(buffer, "%s", BUFFER_SIZE);
			status_func[func_cnt++] = status_channel;
			break;
		    case 'M':
			new_free(&mail_format);
			mail_format = convert_sub_format(get_string_var(STATUS_MAIL_VAR), 'M');
			strmcat(buffer, "%s", BUFFER_SIZE);
			status_func[func_cnt++] = status_mail;
			break;
		    case 'I':
			strmcat(buffer, "%s", BUFFER_SIZE);
			status_func[func_cnt++] = status_insert_mode;
			break;
		    case 'O':
			strmcat(buffer, "%s", BUFFER_SIZE);
			status_func[func_cnt++] = status_overwrite_mode;
			break;
		    case 'A':
			strmcat(buffer, "%s", BUFFER_SIZE);
			status_func[func_cnt++] = status_away;
			break;
		    case 'V':
			strmcat(buffer, "%s", BUFFER_SIZE);
			status_func[func_cnt++] = status_version;
			break;
		    case 'T':
			new_free(&clock_format);
			clock_format = convert_sub_format(get_string_var(STATUS_CLOCK_VAR), 'T');
			strmcat(buffer, "%s", BUFFER_SIZE);
			status_func[func_cnt++] = status_clock;
			break;
		    case 'U':
			strmcat(buffer, "%s", BUFFER_SIZE);
			status_func[func_cnt++] = status_user;
			break;
		    case 'H':
			strmcat(buffer, "%s", BUFFER_SIZE);
			status_func[func_cnt++] = status_hold;
			break;
		    case '*':
			strmcat(buffer, "%s", BUFFER_SIZE);
			status_func[func_cnt++] = status_oper;
			break;
		    case 'W':
			strmcat(buffer, "%s", BUFFER_SIZE);
			status_func[func_cnt++] = status_window;
			break;
		}
	    } else
		ptr++;
	} else
	    strmcat(buffer, format, BUFFER_SIZE);
	format = ptr;
    }
    malloc_strcpy(&malloc_ptr, buffer);	/* this frees the old str first */
    return (malloc_ptr);
}

void build_status(format)
char *format;
{
    int i;

    new_free(&status_format);
    func_cnt = 0;
    if (format = get_string_var(STATUS_FORMAT_VAR))
	status_format = convert_format(format);	/* convert_format mallocs for
						 * us */
    for (i = func_cnt; i < MAX_FUNCTIONS; i++)
	status_func[i] = status_null_function;
    update_all_status();
}

void make_status(window)
Window *window;
{
    int i,
        len;
    char buffer[BUFFER_SIZE + 1];
    char *func_value[MAX_FUNCTIONS];

    if (!dumb && status_format) {
	for (i = 0; i < MAX_FUNCTIONS; i++)
	    func_value[i] = (status_func[i]) (window);
	sprintf(buffer, status_format, func_value[0], func_value[1],
		func_value[2], func_value[3], func_value[4], func_value[5],
		func_value[6], func_value[7], func_value[8], func_value[9],
		func_value[10], func_value[11], func_value[12],
		func_value[13], func_value[14], func_value[15],
		func_value[16], func_value[17], func_value[18],
		func_value[19]);
	for (i = 0; i < MAX_FUNCTIONS; i++)
	    new_free(&(func_value[i]));
	if (get_int_var(FULL_STATUS_LINE_VAR)) {
	    int len;
	    char c[2];

	    len = strlen(buffer);
	    c[0] = *(buffer + len - 1);
	    c[1] = null(char);
	    for (i = len; i < CO; i++)
		strcat(buffer, c);
	}
	if (strlen(buffer) > CO)
	    buffer[CO] = null(char);

	/*
	 * Thanks to Max Bell (mbell@cie.uoregon.edu) for info about TVI
	 * terminals and the sg terminal capability 
	 */
	if (window->last_sl && !SG) {
	    for (i = 0; buffer[i] && window->last_sl[i]; i++) {
		if (buffer[i] != window->last_sl[i])
		    break;
	    }
	} else
	    i = 0;
	if ((len = strlen(buffer + i)) || buffer[i] || window->last_sl[i]) {
	    term_move_cursor(i, window->bottom);
	    term_standout_on();
	    fwrite(buffer + i, len, 1, stdout);
	    term_standout_off();
	    term_clear_to_eol(len, window->bottom);
	    malloc_strcpy(&window->last_sl, buffer);
	    update_input(UPDATE_JUST_CURSOR);
	}
    }
}

static char *status_nickname(window)
Window *window;
{
    char *ptr = null(char *);

    malloc_strcpy(&ptr, nickname);
    return (ptr);
}

static char *status_query_nick(window)
Window *window;
{
    char *ptr = null(char *);

    if (window->query_nick && query_format) {
	sprintf(buffer, query_format, window->query_nick);
	malloc_strcpy(&ptr, buffer);
    } else
	malloc_strcpy(&ptr, empty_string);
    return (ptr);
}

static char *status_clock(window)
Window *window;
{
    char *ptr = null(char *);

    if (get_int_var(CLOCK_VAR) && clock_format) {
	sprintf(buffer, clock_format, update_clock(1));
	malloc_strcpy(&ptr, buffer);
    } else
	malloc_strcpy(&ptr, empty_string);
    return (ptr);
}

static char *status_channel(window)
Window *window;
{
    int num;
    char *ptr = null(char *),
         channel[1024];

    if (window->current_channel) {
	strcpy(channel, window->current_channel);
	if ((num = get_int_var(CHANNEL_NAME_WIDTH_VAR)) &&
	    (strlen(channel) > num))
	    channel[num] = null(char);
	num = atoi(channel);
	if (get_int_var(HIDE_PRIVATE_CHANNELS_VAR) && ((num < 0) || (num > 999)))
	    sprintf(buffer, channel_format, "*private*");
	else
	    sprintf(buffer, channel_format, channel);
	malloc_strcpy(&ptr, buffer);
    } else
	malloc_strcpy(&ptr, empty_string);
    return (ptr);
}

static char *status_mail(window)
Window *window;
{
    char *ptr;

    if (ptr = check_mail())
	sprintf(buffer, mail_format, ptr);
    else
	*buffer = null(char);
    ptr = null(char *);
    malloc_strcpy(&ptr, buffer);
    return (ptr);
}

static char *status_insert_mode(window)
Window *window;
{
    char *ptr = null(char *),
        *text;

    text = empty_string;
    if (get_int_var(INSERT_MODE_VAR)) {
	if ((text = get_string_var(STATUS_INSERT_VAR)) == null(char *))
	    text = empty_string;
    }
    malloc_strcpy(&ptr, text);
    return (ptr);
}

static char *status_overwrite_mode(window)
Window *window;
{
    char *ptr = null(char *),
        *text;

    text = empty_string;
    if (!get_int_var(INSERT_MODE_VAR)) {
	if ((text = get_string_var(STATUS_OVERWRITE_VAR)) == null(char *))
	    text = empty_string;
    }
    malloc_strcpy(&ptr, text);
    return (ptr);
}

static char *status_away(window)
Window *window;
{
    char *ptr = null(char *),
        *text;

    if (away_message && (text = get_string_var(STATUS_AWAY_VAR)))
	malloc_strcpy(&ptr, text);
    else
	malloc_strcpy(&ptr, empty_string);
    return (ptr);
}

static char *status_user(window)
Window *window;
{
    char *ptr = null(char *),
        *text;

    if (text = get_string_var(STATUS_USER_VAR))
	malloc_strcpy(&ptr, text);
    else
	malloc_strcpy(&ptr, empty_string);
    return (ptr);
}

static char *status_hold(window)
Window *window;
{
    char *ptr = null(char *),
        *text;

    if (window->held && (text = get_string_var(STATUS_HOLD_VAR)))
	malloc_strcpy(&ptr, text);
    else
	malloc_strcpy(&ptr, empty_string);
    return (ptr);
}

static char *status_oper(window)
Window *window;
{
    char *ptr = null(char *),
        *text;

    if (operator && (text = get_string_var(STATUS_OPER_VAR)))
	malloc_strcpy(&ptr, text);
    else
	malloc_strcpy(&ptr, empty_string);
    return (ptr);
}

static char *status_window(window)
Window *window;
{
    char *ptr = null(char *),
        *text;

    if ((text = get_string_var(STATUS_WINDOW_VAR)) &&
	(number_of_windows() > 1) && (current_window == window))
	malloc_strcpy(&ptr, text);
    else
	malloc_strcpy(&ptr, empty_string);
    return (ptr);
}

static char *status_version(window)
Window *window;
{
    char *ptr = null(char *);

    malloc_strcpy(&ptr, irc_version);
    return (ptr);
}


static char *status_null_function(window)
Window *window;
{
    char *ptr = null(char *);

    malloc_strcpy(&ptr, empty_string);
    return (ptr);
}
