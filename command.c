/*
 *
 *  mooproxy - a buffering proxy for moo-connections
 *  Copyright (C) 2001-2007 Marcel L. Moreaux <marcelm@luon.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 dated June, 1991.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */



#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>

#include "world.h"
#include "command.h"
#include "misc.h"
#include "network.h"
#include "config.h"
#include "log.h"
#include "daemon.h"
#include "resolve.h"
#include "recall.h"



static int command_index( char * );
static int refuse_arguments( World *, char *, char * );

static void command_help( World *, char *, char * );
static void command_quit( World *, char *, char * );
static void command_shutdown( World *, char *, char * );
static void command_connect( World *, char *, char * );
static void command_disconnect( World *, char *, char * );
static void command_listopts( World *, char *, char * );
static void command_getopt( World *, char *, char * );
static void command_setopt( World *, char *, char * );
static void command_recall( World *, char *, char * );
static void command_version( World *, char *, char * );
static void command_date( World *, char *, char * );
static void command_uptime( World *, char *, char * );
static void command_world( World *, char *, char * );
static void command_debug( World *, char *, char * );



static char *help_text[] = {
"Commands:",
"  help                       Show this help message.",
"  quit                       Disconnect from mooproxy.",
"  shutdown [-f]              Shut down mooproxy. ",
"  connect [<host> [<port>]]  Connect to the server. Arguments",
"                             override configuration.",
"  disconnect                 Disconnect from the server.",
"  listopts                   List the available option names.",
"  get <option>               Query the value of one option.",
"  set <option> <value>       Set the value of one option.",
"  recall <count>             Recall <count> lines.",
"  version                    Show the mooproxy version.",
"  date                       Show the current time and date.",
"  uptime                     Show mooproxy's starting time and uptime.",
"  world                      Show the name of the current world.",
NULL
};

static const struct
{
	char *command;
	void (*func)( World *, char *, char * );
} command_db[] = {
	{ "help",		command_help },
	{ "quit",		command_quit },
	{ "shutdown",		command_shutdown },
	{ "connect",		command_connect },
	{ "disconnect",		command_disconnect },
	{ "listopts",		command_listopts },
	{ "get",		command_getopt },
	{ "set",		command_setopt },
	{ "recall",		command_recall },
	{ "version",		command_version },
	{ "date",		command_date },
	{ "uptime",		command_uptime },
	{ "world",		command_world },
	{ "debug",		command_debug },

	{ NULL,			NULL }
};



extern int world_do_command( World *wld, char *line )
{
	char backup, *args, *cmd = line, *cstr = wld->commandstring;
	int idx;

	/* Ignore the equal parts of commandstr and the line */
	while( *cstr && *cstr == *cmd )
	{
		cstr++;
		cmd++;
	}

	/* If there's still something left in cstr, it's not a command */
	if( *cstr )
		return 0;

	/* Now separate the command from the args */
	args = cmd;
	while( *args && !isspace( *args ) )
		args++;

	/* Mark the end of the command */
	backup = *args;
	*args = '\0';

	/* Look up the command */
	idx = command_index( cmd );

	if( idx == -1 )
	{
		if( !wld->strict_commands )
		{
			/* Strictcmd is off, the line should now be
			 * processed like a regular one. */
			*args = backup;
			return 0;
		}

		/* Invalid command, complain */
		world_msg_client( wld, "Invalid command: %s.", cmd );
		free( line );
		return 1;
	}

	/* Ok, it's a command */

	/* Advance args to the actual start of the arguments */
	if( backup )
		args++;

	/* Do the command!
	 * Note: it's OK to modify cmd and args in the command functions. */
	(*command_db[idx].func)( wld, cmd, args );

	free( line );
	return 1;
}



/* Returns the index of the command in the db, or -1 if not found. */
static int command_index( char *cmd )
{
	int i;

	for( i = 0; command_db[i].command; i++ )
		if( !strcmp( cmd, command_db[i].command ) )
			return i;

	return -1;
}



/* If the args contains anything else than whitespace, complain to the
 * client and return 1. Otherwise, NOP and return 0. */
static int refuse_arguments( World *wld, char *cmd, char *args )
{
	/* Search for \0 or first non-whitespace char */
	while( *args != '\0' && isspace( *args ) )
		args++;

	/* If that char is \0, it's not non-whitespace */
	if( *args == '\0' )
		return 0;

	world_msg_client( wld, "The command %s does not take arguments.", cmd );
	return 1;
}



/* Print a list of commands. No arguments. */
static void command_help( World *wld, char *cmd, char *args )
{
	int i;

	if( refuse_arguments( wld, cmd, args ) )
		return;

	for( i = 0; help_text[i]; i++ )
		world_msg_client( wld, "%s", help_text[i] );
}



/* Disconnect the client. No arguments. */
static void command_quit( World *wld, char *cmd, char *args )
{
	if( refuse_arguments( wld, cmd, args ) )
		return;

	world_msg_client( wld, "Closing connection." );
	wld->flags |= WLD_CLIENTQUIT;
}



/* Shut mooproxy down (forcibly on -f). Arguments: [-f] */
static void command_shutdown( World *wld, char *cmd, char *args )
{
	long unlogged_lines;
	float unlogged_kb;
	char *tmp;

	tmp = get_one_word( &args );

	/* Garbage arguments? */
	if( tmp != NULL && strcmp( tmp, "-f" ) )
	{
		world_msg_client( wld, "Unrecognised argument: `%s'", tmp );
		return;
	}

	/* (tmp != NULL) => (tmp == "-f"). Ergo, forced shutdown. */
	if( tmp != NULL )
	{
		world_msg_client( wld, "Shutting down forcibly." );
		wld->flags |= WLD_SHUTDOWN;
		return;
	}

	/* If we got here, there were no arguments. */

	/* Unlogged data? Refuse. */
	if( wld->log_queue->size + wld->log_current->size +
			wld->log_bfull > 0 )
	{
		unlogged_lines = wld->log_queue->count +
				wld->log_current->count + wld->log_bfull / 80;
		unlogged_kb = ( wld->log_bfull + wld->log_queue->size +
				wld->log_current->size ) / 1024.0;
		world_msg_client( wld, "There are approximately %li lines "
				"(%.1fkb) not yet logged to disk. ",
				unlogged_lines, unlogged_kb );
		world_msg_client( wld, "Refusing to shut down. "
				"Use /shutdown -f to override." );
		return;
	}

	/* We're good, proceed. */
	world_msg_client( wld, "Shutting down." );
	wld->flags |= WLD_SHUTDOWN;
}



/* Connect to the server. Arguments: [host [port]]
 * The function sets wld->server_host and wld->server_port and then
 * flags the world for server resolve start. */
static void command_connect( World *wld, char *cmd, char *args )
{
	char *tmp;
	long port;

	/* Are we already connected? */
	if( wld->server_status == ST_CONNECTED )
	{
		world_msg_client( wld, "Already connected to %s.",
				wld->server_host );
		return;
	}

	/* Or in the process of connecting? */
	if( wld->server_status == ST_RESOLVING ||
			wld->server_status == ST_CONNECTING )
	{
		world_msg_client( wld, "Connection attempt to %s already "
				"in progress.", wld->server_host );
		return;
	}

	/* Or perhaps we're waiting to reconnect? */
	if( wld->server_status == ST_RECONNECTWAIT )
	{
		world_msg_client( wld, "Resetting autoreconnect delay and "
				"reconnecting immediately." );
		wld->reconnect_delay = 0;
		wld->reconnect_at = current_time();
		world_do_reconnect( wld );
		return;
	}

	/* First, clean up existing target. */
	free( wld->server_host );
	wld->server_host = NULL;
	free( wld->server_port );
	wld->server_port = NULL;

	/* If there is an argument, use the first word as hostname */
	if( ( tmp = get_one_word( &args ) ) != NULL )
		wld->server_host = xstrdup( tmp );

	/* If there's another argument, use that as the port */
	if( ( tmp = get_one_word( &args ) ) != NULL )
	{
		port = strtol( tmp, NULL, 0 );
		/* See that the argument port is valid. */
		if( port < 1 || port > 65535 )
		{
			world_msg_client( wld, "`%s' is not a valid port "
					"number.", tmp );
			return;
		}

		xasprintf( &wld->server_port, "%li", port );
	}

	/* If there's no server hostname at all, complain. */
	if( wld->server_host == NULL && wld->dest_host == NULL )
	{
		world_msg_client( wld, "No server host to connect to." );
		return;
	}

	/* If there's no argument hostname, use the configured one. */
	if( wld->server_host == NULL )
		wld->server_host = xstrdup( wld->dest_host );

	/* If there's no server port at all, complain. */
	if( wld->server_port == NULL && wld->dest_port == -1 )
	{
		world_msg_client( wld, "No server port to connect to." );
		return;
	}

	/* If there's no argument port, use the configured one. */
	if( wld->server_port == NULL )
		xasprintf( &wld->server_port, "%li", wld->dest_port );

	/* Inform the client */
	world_msg_client( wld, "Connecting to %s, port %s",
			wld->server_host, wld->server_port );

	/* We don't reconnect if a new connection attempt fails. */
	wld->reconnect_enabled = 0;

	/* Start the resolving */
	wld->flags |= WLD_SERVERRESOLVE;
}



/* Disconnects from the server. No arguments. */
static void command_disconnect( World *wld, char *cmd, char *args )
{
	if( refuse_arguments( wld, cmd, args ) )
		return;

	switch ( wld->server_status )
	{
		case ST_DISCONNECTED:
		world_msg_client( wld, "Not connected." );
		break;

		case ST_RESOLVING:
		world_cancel_server_resolve( wld );
		world_msg_client( wld, "Connection attempt aborted." );
		break;

		case ST_CONNECTING:
		world_cancel_server_connect( wld );
		world_msg_client( wld, "Connection attempt aborted." );
		break;

		case ST_CONNECTED:
		wld->flags |= WLD_SERVERQUIT;
		world_msg_client( wld, "Disconnected." );
		break;

		case ST_RECONNECTWAIT:
		wld->server_status = ST_DISCONNECTED;
		wld->reconnect_delay = 0;
		world_msg_client( wld, "Canceled reconnect." );
	}
}



/* Prints a list of options the user may query or change. No arguments.
 * The list is truncated to 65 characters wide, so that it should fit nicely
 * on a 80x24 terminal, with up to 15 characters of infostring prepended. */
static void command_listopts( World *wld, char *cmd, char *args )
{
	char *line, **list;
	int i, num;

	if( refuse_arguments( wld, cmd, args ) )
		return;

	world_msg_client( wld, "Options:" );

	/* The line initially starts with spaces for indenting. */
	line = xstrdup( "   " );
	num = world_get_key_list( wld, &list );

	for( i = 0; i < num; i++ )
	{
		/* If the length of the line plus the current option would
		 * exceed 65 characters, send the line (without current opt)
		 * to the client, and create a new line. */
		if( strlen( line ) + strlen( list[i] ) > 65 )
		{
			world_msg_client( wld, "%s", line );
			strcpy( line, "   " );
		}

		/* Append the current option to the line. */
		line = xrealloc( line, strlen( line ) + strlen( list[i] ) + 3 );
		line = strcat( line, list[i] );
		line = strcat( line, ", " );

		free( list[i] );
	}

	/* Change the last ", " to ".\0", making the list look good :) */
	i = strlen( line );
	line[i - 2] = '.';
	line[i - 1] = '\0';

	world_msg_client( wld, "%s", line );

	free( list );
	free( line );
}



/* Query one options. Arguments: <option name> */
static void command_getopt( World *wld, char *cmd, char *args )
{
	char *val;

	args = trim_whitespace( args );

	/* If the argument was nothing but whitespace, complain. */
	if( !*args )
	{
		world_msg_client( wld, "Use: get <option>" );
		return;
	}

	switch( world_get_key( wld, args, &val ) )
	{
		case GET_KEY_OK:
		world_msg_client( wld, "The option %s is %s.", args, val );
		free( val );
		break;

		case GET_KEY_NF:
		world_msg_client( wld, "No such option, %s.", args );
		break;

		case GET_KEY_PERM:
		world_msg_client( wld, "The option %s may not be read.", args );
		break;
	}
}



/* Set one option. Arguments: <option name> <new value> */
static void command_setopt( World *wld, char *cmd, char *args )
{
	char *key, *val, *err;
	int ret;

	args = trim_whitespace( args );
	key = args;

	while( *args && !isspace( *args ) )
		args++;

	/* If there were not at least two words as arguments, complain. */
	if( !*args )
	{
		world_msg_client( wld, "Use: set <option> <value>" );
		return;
	}

	/* Replace the first whitespace character after the first word
	 * with \0, making the first word an independant C string. */
	*args++ = '\0';

	while( *args && isspace( *args ) )
		args++;

	/* args now contains everything after the first word, with whitespace
	 * trimmed. Remove any quotes, and put the result in val. */
	val = remove_enclosing_quotes( args );

	/* Key contains the key, val contains the value. Now try and set. */
	ret = world_set_key( wld, key, val, &err );

	/* The option doesn't exist. */
	if( ret == SET_KEY_NF )
	{
		world_msg_client( wld, "No such option, %s.", key );
		return;
	}

	/* We may not set the option. */
	if( ret == SET_KEY_PERM )
	{
		world_msg_client( wld, "The option %s may not be set.", key );
		return;
	}

	/* There was an error setting the option. */
	if( ret == SET_KEY_BAD )
	{
		world_msg_client( wld, "%s", err );
		free( err );
		return;
	}

	/* The option was set successfully, but we should shut up about it. */
	if( ret == SET_KEY_OKSILENT )
		return;

	/* This shouldn't happen. */
	if( ret != SET_KEY_OK )
	{
		world_msg_client( wld, "Huh? world_set_key( %s ) returned "
				"weird %i!", key, ret );
		return;
	}

	/* ret == SET_KEY_OK, the key is set successfully */
	if( world_get_key( wld, key, &val ) == GET_KEY_OK )
	{
		world_msg_client( wld, "The option %s is now %s.", key, val );
		free( val );
	} else {
		/* The key was successfully set, but we could not query the
		 * new value. Either the option is hidden or it may not be
		 * read. We just report it has been changed. */
		world_msg_client( wld, "The option %s has been changed.", key );
	}
}



/* Recalls lines. FIXME: better description. */
static void command_recall( World *wld, char *cmd, char *args )
{
	Linequeue *queue;
	long count;
	char *str;

	trim_whitespace( args );

	/* If there are no arguments, print terse usage. */
	if( args[0] == '\0' )
	{
		world_msg_client( wld, "Use: recall [from <when>] [to <when>]"
				" [search <text>]" );
		world_msg_client( wld, "Or:  recall last <number>"
				" [search <text>]" );
		world_msg_client( wld, "Or:  recall <number>" );
		world_msg_client( wld, "See the README file for details." );
		return;
	}

	/* We want to include the inactive lines in our recall as well, so
	 * we move them to the history right now. */
	world_inactive_to_history( wld );

	/* Check if there are any lines to recall. */
	if( wld->history_lines->count == 0 )
	{
		world_msg_client( wld, "There are no lines to recall." );
		return;
	}

	/* Check if the arg string is all digits. */
	for( str = args; isdigit( *str ); str++ )
		continue;

	/* It isn't, pass it on for more sophisticated processing. */
	if( *str != '\0' )
	{
		world_recall_command( wld, args );
		return;
	}

	/* Ok, it's all digits, do 'classic' recall. */
	count = atol( args );
	if( count <= 0 )
	{
		world_msg_client( wld, "Number of lines to recall should be "
				"greater than zero." );
		return;
	}

	/* Get the recalled lines. */
	queue = world_recall_history( wld, count );

	/* Announce the recall. */
	world_msg_client( wld, "Recalling %lu line%s.", queue->count,
			( queue->count == 1 ) ? "" : "s" );

	/* Append the recalled lines, and clean up. */
	linequeue_merge( wld->client_toqueue, queue );
	linequeue_destroy( queue );
}



/* Print the version. No arguments. */
static void command_version( World *wld, char *cmd, char *args )
{
	if( refuse_arguments( wld, cmd, args ) )
		return;

	world_msg_client( wld, "Mooproxy version " VERSIONSTR "." );
}



/* Print the current (full) date. No arguments. */
static void command_date( World *wld, char *cmd, char *args )
{
	if( refuse_arguments( wld, cmd, args ) )
		return;

	world_msg_client( wld, "The current date is %s.",
			time_string( current_time(), FULL_TIME ) ); 
}



/* Print mooproxies starting date/time and uptime. No arguments. */
static void command_uptime( World *wld, char *cmd, char *args )
{
	time_t tme = uptime_started_at();
	long utme = (long) current_time() - tme;

	if( refuse_arguments( wld, cmd, args ) )
		return;

	world_msg_client( wld, "Started %s. Uptime is %li days, %.2li:%.2li:"
			"%.2li.", time_string( tme, FULL_TIME ), utme / 86400,
			utme % 86400 / 3600, utme % 3600 / 60, utme % 60 );
}



/* Print the name of the current world. No arguments. */
static void command_world( World *wld, char *cmd, char *args )
{
	if( refuse_arguments( wld, cmd, args ) )
		return;

	world_msg_client( wld, "The world is %s.", wld->name );
}



static void command_debug( World *wld, char *cmd, char *args )
{
	trim_whitespace( args );

	if( strcasecmp( args, "stats" ) )
	{
		world_msg_client( wld, "Supported actions:" );
		world_msg_client( wld, "  stats" );
		return;
	}

	world_msg_client( wld, "Stats:" );

	world_msg_client( wld, "  max_buffer_size: %li,  "
		"max_logbuffer_size: %li,  LINE_BYTE_COST: %li",
		wld->max_buffer_size,
		wld->max_logbuffer_size,
		( sizeof( Line ) + sizeof( void * ) * 2 + 8 ) );

	world_msg_client( wld, "                       lines      bytes"
		"       free     %%used" );

	world_msg_client( wld, "  inactive_lines:  %9li  %9li   %8s  %7.3f%%",
		wld->inactive_lines->count,
		wld->inactive_lines->size,
		"-",
		wld->inactive_lines->size / 10.24 / wld->max_buffer_size );

	world_msg_client( wld, "  history_lines :  %9li  %9li   %8s  %7.3f%%",
		wld->history_lines->count,
		wld->history_lines->size,
		"-",
		wld->history_lines->size / 10.24 / wld->max_buffer_size );

	world_msg_client( wld, "  `---- +          %9li  %9li  %9li  %7.3f%%",
		wld->inactive_lines->count + wld->history_lines->count,
		wld->inactive_lines->size + wld->history_lines->size,
		wld->max_buffer_size * 1024 - wld->inactive_lines->size
		- wld->history_lines->size,
		( wld->inactive_lines->size + wld->history_lines->size )
		/ 10.24 / wld->max_buffer_size );

	world_msg_client( wld, "  log_queue     :  %9li  %9li   %8s  %7.3f%%",
		wld->log_queue->count,
		wld->log_queue->size,
		"-",
		wld->log_queue->size / 10.24 / wld->max_logbuffer_size );

	world_msg_client( wld, "  log_current   :  %9li  %9li   %8s  %7.3f%%",
		wld->log_current->count,
		wld->log_current->size,
		"-",
		wld->log_current->size / 10.24 / wld->max_logbuffer_size );

	world_msg_client( wld, "  `---- +          %9li  %9li  %9li  %7.3f%%",
		wld->log_queue->count + wld->log_current->count,
		wld->log_queue->size + wld->log_current->size,
		wld->max_logbuffer_size * 1024 - wld->log_queue->size
		- wld->log_current->size,
		( wld->log_queue->size + wld->log_current->size )
		/ 10.24 / wld->max_logbuffer_size );
}
