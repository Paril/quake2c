/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#pragma once

// player_state_t->refdef flags
enum refdef_flags_t
{
	RDF_UNDERWATER		= 1,	// warp the screen as apropriate
	RDF_NOWORLDMODEL	= 2,	// used for player configuration screen

//ROGUE
	RDF_IRGOGGLES	= 4,
	RDF_UVGOGGLES	= 8
//ROGUE
};

// player_state->stats[] indexes
enum player_stat_t : int16_t
{
	// For engine compatibility, these 18 IDs should remain the same
	// and keep their described usage
	STAT_HEALTH_ICON,
	STAT_HEALTH,
	STAT_AMMO_ICON,
	STAT_AMMO,
	STAT_ARMOR_ICON,
	STAT_ARMOR,
	STAT_SELECTED_ICON,
	STAT_PICKUP_ICON,
	STAT_PICKUP_STRING,
	STAT_TIMER_ICON,
	STAT_TIMER,
	STAT_HELPICON,
	STAT_SELECTED_ITEM,
	STAT_LAYOUTS,
	STAT_FRAGS,
	STAT_FLASHES,		// cleared each frame, 1 = health, 2 = armor
	STAT_CHASE,
	STAT_SPECTATOR,
	// Bits from here to 31 are free for mods

	MAX_STATS	= 32
};

// player_state_t is the information needed in addition to pmove_state_t
// to rendered a view.  There will only be 10 player_state_t sent each second,
// but the number of pmove_state_t changes will be reletive to client
// frame rates
struct player_state_t
{
	pmove_state_t   pmove;      // for prediction

	// these fields do not need to be communicated bit-precise

	vec3_t      viewangles;     // for fixed views
	vec3_t      viewoffset;     // add to pmovestate->origin
	vec3_t      kick_angles;    // add to view direction to get render angles
								// set by weapon kicks, pain effects, etc

	vec3_t      gunangles;
	vec3_t      gunoffset;
	int         gunindex;
	int         gunframe;

	float       blend[4];       // rgba full screen effect

	float       fov;            // horizontal field of view

	refdef_flags_t	rdflags;        // refdef flags

	std::array<player_stat_t, MAX_STATS>	stats;       // fast status bar updates
};


struct gclient_t
{
	player_state_t	ps;		// communicated by server to clients
	int				ping;

	// the game dll can add anything it wants after
	// this point in the structure
	int	clientNum;
};