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

//
// game.h -- game dll information visible to server
//
typedef struct edict_s edict_t;
typedef struct gclient_s gclient_t;

constexpr int GAME_API_VERSION	= 3;

//===============================================================

//
// per-level limits
//
constexpr size_t MAX_CLIENTS		= 256;	// absolute limit
constexpr size_t MAX_EDICTS			= 1024;	// must change protocol to increase more

// game print flags
enum
{
	PRINT_LOW,		// pickup messages
	PRINT_MEDIUM,	// death messages
	PRINT_HIGH,		// critical messages
	PRINT_CHAT		// chat messages    
};

typedef int print_level_t;

// destination class for gi.multicast()
enum
{
	MULTICAST_ALL,
	MULTICAST_PHS,
	MULTICAST_PVS,
	MULTICAST_ALL_R,
	MULTICAST_PHS_R,
	MULTICAST_PVS_R
};

typedef int multicast_t;


//
// SOUNDS
//

// sound channels
// channel 0 never willingly overrides
// other channels (1-7) allways override a playing sound on that channel
enum
{
	CHAN_AUTO,
	CHAN_WEAPON,
	CHAN_VOICE,
	CHAN_ITEM,
	CHAN_BODY,
	// 3 unused IDs

// modifier flags
	CHAN_NO_PHS_ADD	= 8,	// send to all clients, not just ones in PHS (ATTN 0 will also do this)
	CHAN_RELIABLE	= 16	// send by reliable message, not datagram
};

typedef int sound_channel_t;

typedef vec_t sound_attn_t;

typedef int config_string_t;

/*
==========================================================

CVARS (console variables)

==========================================================
*/

enum
{
	CVAR_NONE		= 0,
	CVAR_ARCHIVE	= 1,	// set to cause it to be saved to vars.rc
	CVAR_USERINFO	= 2,	// added to userinfo  when changed
	CVAR_SERVERINFO	= 4,	// added to serverinfo when changed
	CVAR_NOSET		= 8,	// don't allow change from console at all,
							// but can be set from the command line
	CVAR_LATCH	= 16	// save changes until server restart
};

typedef int cvar_flags_t;

// nothing outside the cvar.*() functions should modify these fields!
struct cvar_t
{
	char			*name;
	char			*string;
	char			*latched_string;    // for CVAR_LATCH vars
	cvar_flags_t	flags;
	qboolean		modified;   // set each time the cvar is changed
	float			value;
};

/*
==============================================================

COLLISION DETECTION

==============================================================
*/

// lower bits are stronger, and will eat weaker brushes completely
typedef int content_flags_t;

typedef int surface_flags_t;

typedef int box_edicts_area_t;

typedef uint8_t plane_type_t;

// plane_t structure
struct cplane_t
{
	vec3_t			normal;
	float			dist;
	plane_type_t	type;           // for fast side tests
	uint8_t			signbits;       // signx + (signy<<1) + (signz<<1)
	uint8_t			pad[2];
};

struct csurface_t
{
	char			name[16];
	surface_flags_t	flags;
	int				value;
};

// a trace is returned when a box is swept through the world
struct trace_t
{
	qboolean		allsolid;   // if true, plane is not valid
	qboolean		startsolid; // if true, the initial point was in a solid area
	float			fraction;   // time completed, 1.0 = didn't hit anything
	vec3_t			endpos;     // final position
	cplane_t		plane;      // surface normal at impact
	csurface_t		*surface;   // surface hit
	content_flags_t	contents;   // contents on other side of surface hit
	edict_t			*ent;       // not set by CM_*() functions
};

//
// button bits
//
enum
{
	BUTTON_ATTACK		= 1,
	BUTTON_USE			= 2,
	BUTTON_ANY			= 128		// any key whatsoever
};

typedef uint8_t button_bits_t;

// usercmd_t is sent to the server each client frame
struct usercmd_t
{
	uint8_t					msec;
	button_bits_t			buttons;
	std::array<short, 3>	angles;
	short					forwardmove, sidemove, upmove;
	uint8_t					impulse;	// remove?
	uint8_t					lightlevel;	// light level the player is standing on
};

// pmove_state_t is the information necessary for client side movement
// prediction
enum
{
	// can accelerate and turn
	PM_NORMAL,
	PM_SPECTATOR,
	// no acceleration or turning
	PM_DEAD,
	PM_GIB,     // different bounding box
	PM_FREEZE
};

typedef int pmtype_t;

// pmove->pm_flags
enum
{
	PMF_DUCKED			= 1,
	PMF_JUMP_HELD		= 2,
	PMF_ON_GROUND		= 4,
	PMF_TIME_WATERJUMP	= 8,	// pm_time is waterjump
	PMF_TIME_LAND		= 16,	// pm_time is time before rejump
	PMF_TIME_TELEPORT	= 32,	// pm_time is non-moving time
	PMF_NO_PREDICTION	= 64,	// temporarily disables prediction (used for grappling hook)
	PMF_TELEPORT_BIT	= 128	// used by q2pro
};

typedef uint8_t pmflags_t;

// this structure needs to be communicated bit-accurate
// from the server to the client to guarantee that
// prediction stays in sync, so no floats are used.
// if any part of the game code modifies this struct, it
// will result in a prediction error of some degree.
struct pmove_state_t
{
	pmtype_t	pm_type;

	std::array<short, 3>	origin;		// 12.3
	std::array<short, 3>	velocity;	// 12.3
	pmflags_t				pm_flags;		// ducked, jump_held, etc
	uint8_t					pm_time;		// each unit = 8 ms
	short					gravity;
	std::array<short, 3>	delta_angles;	// add to command angles to get view direction
												// changed by spawns, rotating objects, and teleporters
};

const size_t MAX_TOUCH	= 32;

struct pmove_t
{
	// state (in / out)
	pmove_state_t	s;

	// command (in)
	usercmd_t	cmd;
	qboolean	snapinitial;    // if s has been changed outside pmove

	// results (out)
	int								numtouch;
	std::array<edict_t*, MAX_TOUCH>	touchents;

	vec3_t	viewangles;         // clamped
	float	viewheight;

	vec3_t	mins, maxs;         // bounding box size

	edict_t	*groundentity;
	int		watertype;
	int		waterlevel;

	// callbacks to test the world
	trace_t			(* q_gameabi trace)(const vec3_t &start, const vec3_t &mins, const vec3_t &maxs, const vec3_t &end);
	content_flags_t	(*pointcontents)(const vec3_t &point);
};

//===============================================================

//
// functions provided by the main engine
//
extern "C" struct game_import_t
{
	// special messages
	void (* q_printf(2, 3) bprintf)(print_level_t printlevel, const char *fmt, ...);
	void (* q_printf(1, 2) dprintf)(const char *fmt, ...);
	void (* q_printf(3, 4) cprintf)(edict_t *ent, print_level_t printlevel, const char *fmt, ...);
	void (* q_printf(2, 3) centerprintf)(edict_t *ent, const char *fmt, ...);
	void (*sound)(edict_t *ent, sound_channel_t channel, int soundindex, vec_t volume, sound_attn_t attenuation, vec_t timeofs);
	void (*positioned_sound)(const vec3_t &origin, edict_t *ent, sound_channel_t channel, int soundindex, vec_t volume, sound_attn_t attenuation, vec_t timeofs);

	// config strings hold all the index strings, the lightstyles,
	// and misc data like the sky definition and cdtrack.
	// All of the current configstrings are sent to clients when
	// they connect, and changes are sent to all connected clients.
	void (*configstring)(config_string_t num, const char *string);

	void (* q_noreturn q_printf(1, 2) error)(const char *fmt, ...);

	// the *index functions create configstrings and some internal server state
	int (*modelindex)(const char *name);
	int (*soundindex)(const char *name);
	int (*imageindex)(const char *name);

	void (*setmodel)(edict_t *ent, const char *name);

	// collision detection
	trace_t (* q_gameabi trace)(const vec3_t &start, const vec3_t &mins, const vec3_t &maxs, const vec3_t &end, edict_t *passent, content_flags_t contentmask);
	content_flags_t (*pointcontents)(const vec3_t &point);
	qboolean (*inPVS)(const vec3_t &p1, const vec3_t &p2);
	qboolean (*inPHS)(const vec3_t &p1, const vec3_t &p2);
	void (*SetAreaPortalState)(int portalnum, qboolean open);
	qboolean (*AreasConnected)(int area1, int area2);

	// an entity will never be sent to a client or used for collision
	// if it is not passed to linkentity.  If the size, position, or
	// solidity changes, it must be relinked.
	void (*linkentity)(edict_t *ent);
	void (*unlinkentity)(edict_t *ent);     // call before removing an interactive edict
	int (*BoxEdicts)(const vec3_t &mins, const vec3_t &maxs, edict_t **list, int maxcount, box_edicts_area_t areatype);
	void (*Pmove)(pmove_t *pmove);          // player movement code common with client prediction

	// network messaging
	void (*multicast)(const vec3_t &origin, multicast_t to);
	void (*unicast)(edict_t *ent, qboolean reliable);
	void (*WriteChar)(int c);
	void (*WriteByte)(int c);
	void (*WriteShort)(int c);
	void (*WriteLong)(int c);
	void (*WriteFloat)(vec_t f);
	void (*WriteString)(const char *s);
	void (*WritePosition)(const vec3_t &pos);    // some fractional bits
	void (*WriteDir)(const vec3_t &pos);         // single byte encoded, very coarse
	void (*WriteAngle)(vec_t f);

	// managed memory allocation
	void *(*TagMalloc)(unsigned size, unsigned tag);
	void (*TagFree)(void *block);
	void (*FreeTags)(unsigned tag);

	// console variable interaction
	cvar_t *(*cvar)(const char *var_name, const char *value, cvar_flags_t flags);
	cvar_t *(*cvar_set)(const char *var_name, const char *value);
	cvar_t *(*cvar_forceset)(const char *var_name, const char *value);

	// ClientCommand and ServerCommand parameter access
	int (*argc)(void);
	char *(*argv)(int n);
	char *(*args)(void);     // concatenation of all argv >= 1

	// add commands to the server console as if they were typed in
	// for map changing, etc
	void (*AddCommandString)(const char *text);

	void (*DebugGraph)(vec_t value, int color);
};

//
// functions exported by the game subsystem
//
extern "C" struct game_export_t
{
	int	apiversion;

	// the init function will only be called when a game starts,
	// not each time a level is loaded.  Persistant data for clients
	// and the server can be allocated in init
	void (*Init)(void);
	void (*Shutdown)(void);

	// each new level entered will cause a call to SpawnEntities
	void (*SpawnEntities)(const char *mapname, const char *entstring, const char *spawnpoint);

	// Read/Write Game is for storing persistant cross level information
	// about the world state and the clients.
	// WriteGame is called every time a level is exited.
	// ReadGame is called on a loadgame.
	void (*WriteGame)(const char *filename, qboolean autosave);
	void (*ReadGame)(const char *filename);

	// ReadLevel is called after the default map information has been
	// loaded with SpawnEntities
	void (*WriteLevel)(const char *filename);
	void (*ReadLevel)(const char *filename);

	qboolean (*ClientConnect)(edict_t *ent, char *userinfo);
	void (*ClientBegin)(edict_t *ent);
	void (*ClientUserinfoChanged)(edict_t *ent, char *userinfo);
	void (*ClientDisconnect)(edict_t *ent);
	void (*ClientCommand)(edict_t *ent);
	void (*ClientThink)(edict_t *ent, usercmd_t *cmd);

	void (*RunFrame)(void);

	// ServerCommand will be called when an "sv <command>" command is issued on the
	// server console.
	// The game can issue gi.argc() / gi.argv() commands to get the rest
	// of the parameters
	void (*ServerCommand)(void);

	//
	// global variables shared between game and server
	//

	// The edict array is allocated in the game dll so it
	// can vary in size from one game to another.
	//
	// The size will be fixed when ge->Init() is called
	edict_t		*edicts;
	int			edict_size;
	int			num_edicts;	// current number, <= max_edicts
	int			max_edicts;
};