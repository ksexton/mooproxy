/*
 *
 *  mooproxy - a buffering proxy for moo-connections
 *  Copyright (C) 2001-2005 Marcel L. Moreaux <marcelm@luon.net>
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



#ifndef MOOPROXY__HEADER__TIMER
#define MOOPROXY__HEADER__TIMER



#include <time.h>



/* Update current time administration, and execute periodic or scheduled
 * events. This function should be called regularly (preferably once each
 * second). t should be the current time as reported by time() */
extern void world_timer_tick( World *wld, time_t t );



#endif  /* ifndef MOOPROXY__HEADER__TIMER */