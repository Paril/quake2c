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
typedef enum
{
	RDF_UNDERWATER		= 1,	// warp the screen as apropriate
	RDF_NOWORLDMODEL	= 2,	// used for player configuration screen

//ROGUE
	RDF_IRGOGGLES	= 4,
	RDF_UVGOGGLES	= 8
//ROGUE
} refdef_flags_t;

// player_state->stats[] indexes
enum
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

#ifdef KMQUAKE2_ENGINE_MOD
	MAX_STATS	= 256
#else
	MAX_STATS	= 32
#endif
};

typedef int16_t player_stat_t;

// player_state_t is the information needed in addition to pmove_state_t
// to rendered a view.  There will only be 10 player_state_t sent each second,
// but the number of pmove_state_t changes will be reletive to client
// frame rates
typedef struct
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

#ifdef KMQUAKE2_ENGINE_MOD //Knightmare added
	int			gunskin;		// for animated weapon skins
	int			gunindex2;		// for a second weapon model (boot)
	int			gunframe2;
	int			gunskin2;

	// server-side speed control!
	int			maxspeed;
	int			duckspeed;
	int			waterspeed;
	int			accel;
	int			stopspeed;
#endif

	float       blend[4];       // rgba full screen effect

	float       fov;            // horizontal field of view

	refdef_flags_t	rdflags;        // refdef flags

	player_stat_t	stats[MAX_STATS];       // fast status bar updates
} player_state_t;


struct gclient_s
{
	player_state_t	ps;		// communicated by server to clients
	int				ping;

	// the game dll can add anything it wants after
	// this point in the structure
	int	clientNum;
};