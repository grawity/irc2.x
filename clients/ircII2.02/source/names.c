/*
 * names.c: This here is used to maintain a list of all the people currently
 * on your channel.  Seems to work 
 *
 *
 * Written By Michael Sandrof <ms5n@andrew.cmu.edu> 
 *
 * Copyright(c) 1990 
 *
 * See the COPYRIGHT file, or do a HELP IRCII COPYRIGHT 
 */
#include "irc.h"
#include "names.h"
#include "window.h"
#include "lastlog.h"
#include "list.h"

/* NickList: structure for the list of nicknames of people on a channel */
typedef struct nick_stru {
    struct nick_stru *next;	/* pointer to next nickname entry */
    char *nick;			/* nickname of person on channel */
}         NickList;

/* ChannelList: structure for the list of channels you are current on */
typedef struct channel_stru {
    struct channel_stru *next;	/* pointer to next channel entry */
    char *channel;		/* channel name */
    NickList *nicks;		/* pointer to list of nicks on channel */
}            ChannelList;

/* channel_list: list of all the channels you are currently on */
static ChannelList *channel_list = null(ChannelList *);

/* clear_channel: erases all entries in a nick list for the given channel */
static void clear_channel(chan)
ChannelList *chan;
{
    NickList *tmp,
            *next;

#ifdef DEBUG
    put_it("Clearing channel: %s", chan->channel);
#endif DEBUG
    for (tmp = chan->nicks; tmp; tmp = next) {
	next = tmp->next;
	new_free(&(tmp->nick));
	new_free(&tmp);
    }
    chan->nicks = null(NickList *);
}

/*
 * add_channel: adds the named channel to the channel list.  If the channel
 * is already in the list, nothing happens.   The added channel becomes the
 * current channel as well 
 */
void add_channel(channel)
char *channel;
{
    ChannelList *new;

#ifdef DEBUG
    put_it("Adding channel %s", channel);
#endif DEBUG
    if (!is_current_channel(channel, 0))
	current_channel(channel);
    update_all_status();
    if (new = (ChannelList *) remove_from_list(&channel_list, channel)){
	clear_channel(new);
	new_free(&(new->channel));
	new_free(&new);
    }
    new = (ChannelList *) new_malloc(sizeof(ChannelList));
    new->channel = null(char *);
    malloc_strcpy(&(new->channel), channel);
    new->nicks = null(NickList *);
    add_to_list(&channel_list, new, sizeof(ChannelList));
}

/*
 * add_to_channel: adds the given nickname to the given channel.  If the
 * nickname is already on the channel, nothing happens.  If the channel is
 * not on the channel list, nothing happens (although perhaps the channel
 * should be addded to the list?  but this should never happen) 
 */
void add_to_channel(channel, nick)
char *channel;
char *nick;
{
    NickList *new;
    ChannelList *chan;

    if (chan = (ChannelList *) list_lookup(&channel_list, channel, !USE_WILDCARDS, !REMOVE_FROM_LIST)) {
	if (*nick == '@')
	    nick++;
#ifdef DEBUG
	put_it("Adding %s to channel %s", nick, channel);
#endif DEBUG
	if (new = (NickList *) remove_from_list(&(chan->nicks), nick)){
	    new_free(&(new->nick));
	    new_free(&new);
	}
	new = (NickList *) new_malloc(sizeof(NickList));
	new->nick = null(char *);
	malloc_strcpy(&(new->nick), nick);
	add_to_list(&(chan->nicks), new, sizeof(NickList));
    }
}

/*
 * remove_channel: removes the named channel from the channel_list.  If the
 * channel is not on the channel_list, nothing happens.  If the channel was
 * the current channel, this will select the top of the channel_list to be
 * the current_channel, or 0 if the list is empty. 
 */
void remove_channel(channel)
char *channel;
{
    ChannelList *tmp;

#ifdef DEBUG
    put_it("Removing channel %s", channel);
#endif DEBUG
    if (channel) {
	if (tmp = (ChannelList *) list_lookup(&channel_list, channel, !USE_WILDCARDS, REMOVE_FROM_LIST)) {
	    clear_channel(tmp);
	    new_free(&(tmp->channel));
	    new_free(&tmp);
	}
	if (is_current_channel(channel, 1))
	    switch_channels();
	update_all_status();
    } else {
	ChannelList *next;

#ifdef DEBUG
	put_it("Removing all channels", channel);
#endif DEBUG
	for (tmp = channel_list; tmp; tmp = next) {
	    next = tmp->next;
	    clear_channel(tmp);
	    new_free(&(tmp->channel));
	    new_free(&tmp);
	}
	/* current_channel("0"); */
	current_channel(null(char *));
	channel_list = null(ChannelList *);
    }
}

/*
 * remove_from_channel: removes the given nickname from the given channel. If
 * the nickname is not on the channel or the channel doesn't exist, nothing
 * happens. 
 */
void remove_from_channel(channel, nick)
char *channel;
char *nick;
{
    ChannelList *chan;
    NickList *tmp;

    if (channel) {
	if (chan = (ChannelList *) list_lookup(&channel_list, channel, !USE_WILDCARDS, !REMOVE_FROM_LIST)) {
	    if (tmp = (NickList *) list_lookup(&(chan->nicks), nick, !USE_WILDCARDS, REMOVE_FROM_LIST)) {
#ifdef DEBUG
		put_it("Removing %s from channel %s", nick, channel);
#endif DEBUG
		new_free(&(tmp->nick));
		new_free(&tmp);
	    }
	}
    } else {
	for (chan = channel_list; chan; chan = chan->next) {
	    if (tmp = (NickList *) list_lookup(&(chan->nicks), nick, !USE_WILDCARDS, REMOVE_FROM_LIST)) {
#ifdef DEBUG
		put_it("Removing %s from channel %s", nick, chan->channel);
#endif DEBUG
		new_free(&(tmp->nick));
		new_free(&tmp);
	    }
	}
    }
}

/*
 * rename_nick: in response to a changed nickname, this looks up the given
 * nickname on all you channels and changes it the new_nick 
 */
void rename_nick(old_nick, new_nick)
char *old_nick,
    *new_nick;
{
    ChannelList *chan;
    NickList *tmp;

#ifdef DEBUG
    put_it("Changing %s to %s", old_nick, new_nick);
#endif DEBUG
    for (chan = channel_list; chan; chan = chan->next) {
	if (tmp = (NickList *) list_lookup(&(chan->nicks), old_nick, !USE_WILDCARDS, REMOVE_FROM_LIST)) {
	    new_free(&(tmp->nick));
	    new_free(&tmp);
	    add_to_channel(chan->channel, new_nick);
	}
    }
}

/*
 * is_on_channel: returns true if the given nickname is in the given channel,
 * false otherwise.  Also returns false if the given channel is not on the
 * channel list. 
 */
int is_on_channel(channel, nick)
char *channel;
char *nick;
{
    ChannelList *chan;

    if (chan = (ChannelList *) list_lookup(&channel_list, channel, !USE_WILDCARDS, !REMOVE_FROM_LIST)) {
	if (list_lookup(&(chan->nicks), nick, !USE_WILDCARDS, !REMOVE_FROM_LIST))
	    return (1);
    }
    return (0);
}

void show_channel(chan)
ChannelList *chan;
{
    NickList *tmp;

    strncpy(buffer, chan->channel, BUFFER_SIZE);
    strmcat(buffer, ": ", BUFFER_SIZE);
    for (tmp = chan->nicks; tmp; tmp = tmp->next) {
	strmcat(buffer, tmp->nick, BUFFER_SIZE);
	strmcat(buffer, " ",BUFFER_SIZE);
    }
    put_it("***\t%s", buffer);
}

/* list_channels: displays your current channel and your channel list */
void list_channels()
{
    ChannelList *tmp;

    if (channel_list) {
	if (current_channel(null(char *)))
	    put_it("*** Current channel %s", current_channel(null(char *)));
	else
	    put_it("*** No current channel for this window");
	put_it("*** You are on the following channels:");
	for (tmp = channel_list; tmp; tmp = tmp->next)
	    show_channel(tmp);
    } else
	put_it("*** You are not on any channels");
}

/*
 * reconnect_all_channels: used after you get disconnected from a server,
 * clear each channel nickname list and re-JOINs each channel in the
 * channel_list 
 */
void reconnect_all_channels()
{
    ChannelList *tmp;

    for (tmp = channel_list; tmp; tmp = tmp->next) {
#ifdef DEBUG
	put_it("Reconnecting to channel %s", tmp->channel);
#endif DEBUG
	clear_channel(tmp);
	if (irc2_6)
	    send_to_server("JOIN %s", tmp->channel);
	else
	    send_to_server("CHANNEL %s", tmp->channel);
    }
    message_from(null(char *), LOG_CRAP);
}

void switch_channels()
{
    ChannelList *tmp;

    if (channel_list) {
	if (current_channel(null(char *))) {
	    if (tmp = (ChannelList *) list_lookup(&channel_list, current_channel(null(char *)), !USE_WILDCARDS, !REMOVE_FROM_LIST)) {
		for (tmp = tmp->next; tmp; tmp = tmp->next) {
		    if (!is_current_channel(tmp->channel, 0)) {
			current_channel(tmp->channel);
			update_all_status();
			return;
		    }
		}
	    }
	}
	for (tmp = channel_list; tmp; tmp = tmp->next) {
	    if (!is_current_channel(tmp->channel, 0)) {
		current_channel(tmp->channel);
		update_all_status();
		return;
	    }
	}
    }
}

/* real_channel: returns your "real" channel (your non-multiple channel) */
char *real_channel()
{
    ChannelList *tmp;

    if (channel_list) {
	for (tmp = channel_list; tmp; tmp = tmp->next) {
	    if (*(tmp->channel) != '#')
		return (tmp->channel);
	}
    }
    return (null(char *));
}
