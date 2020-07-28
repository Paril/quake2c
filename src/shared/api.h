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
constexpr size_t MAX_LIGHTSTYLES	= 256;
constexpr size_t MAX_MODELS			= 256;	// these are sent over the net as bytes
constexpr size_t MAX_SOUNDS			= 256;	// so they cannot be blindly increased
constexpr size_t MAX_IMAGES			= 256;
constexpr size_t MAX_ITEMS			= 256;
constexpr size_t MAX_GENERAL		= (MAX_CLIENTS * 2); // general config strings

// game print flags
enum print_level_t
{
	PRINT_LOW,		// pickup messages
	PRINT_MEDIUM,	// death messages
	PRINT_HIGH,		// critical messages
	PRINT_CHAT		// chat messages    
};

// destination class for gi.multicast()
enum multicast_t
{
	MULTICAST_ALL,
	MULTICAST_PHS,
	MULTICAST_PVS,
	MULTICAST_ALL_R,
	MULTICAST_PHS_R,
	MULTICAST_PVS_R
};

//
// muzzle flashes / player effects
//
enum muzzleflash_t
{
	MZ_BLASTER,
	MZ_MACHINEGUN,
	MZ_SHOTGUN,
	MZ_CHAINGUN1,
	MZ_CHAINGUN2,
	MZ_CHAINGUN3,
	MZ_RAILGUN,
	MZ_ROCKET,
	MZ_GRENADE,
	MZ_LOGIN,
	MZ_LOGOUT,
	MZ_RESPAWN,
	MZ_BFG,
	MZ_SSHOTGUN,
	MZ_HYPERBLASTER,
	MZ_ITEMRESPAWN,
// RAFAEL
	MZ_IONRIPPER,
	MZ_BLUEHYPERBLASTER,
	MZ_PHALANX,
// RAFAEL

//ROGUE
	MZ_ETF_RIFLE	= 30,
	MZ_UNUSED,
	MZ_SHOTGUN2,
	MZ_HEATBEAM,
	MZ_BLASTER2,
	MZ_TRACKER,
	MZ_NUKE1,
	MZ_NUKE2,
	MZ_NUKE4,
	MZ_NUKE8,
//ROGUE

	MZ_SILENCED		= 128	// bit flag ORed with one of the above numbers
};

//
// monster muzzle flashes
//
enum monster_muzzleflash_t
{
	MZ2_TANK_BLASTER_1	= 1,
	MZ2_TANK_BLASTER_2,
	MZ2_TANK_BLASTER_3,
	MZ2_TANK_MACHINEGUN_1,
	MZ2_TANK_MACHINEGUN_2,
	MZ2_TANK_MACHINEGUN_3,
	MZ2_TANK_MACHINEGUN_4,
	MZ2_TANK_MACHINEGUN_5,
	MZ2_TANK_MACHINEGUN_6,
	MZ2_TANK_MACHINEGUN_7,
	MZ2_TANK_MACHINEGUN_8,
	MZ2_TANK_MACHINEGUN_9,
	MZ2_TANK_MACHINEGUN_10,
	MZ2_TANK_MACHINEGUN_11,
	MZ2_TANK_MACHINEGUN_12,
	MZ2_TANK_MACHINEGUN_13,
	MZ2_TANK_MACHINEGUN_14,
	MZ2_TANK_MACHINEGUN_15,
	MZ2_TANK_MACHINEGUN_16,
	MZ2_TANK_MACHINEGUN_17,
	MZ2_TANK_MACHINEGUN_18,
	MZ2_TANK_MACHINEGUN_19,
	MZ2_TANK_ROCKET_1,
	MZ2_TANK_ROCKET_2,
	MZ2_TANK_ROCKET_3,

	MZ2_INFANTRY_MACHINEGUN_1,
	MZ2_INFANTRY_MACHINEGUN_2,
	MZ2_INFANTRY_MACHINEGUN_3,
	MZ2_INFANTRY_MACHINEGUN_4,
	MZ2_INFANTRY_MACHINEGUN_5,
	MZ2_INFANTRY_MACHINEGUN_6,
	MZ2_INFANTRY_MACHINEGUN_7,
	MZ2_INFANTRY_MACHINEGUN_8,
	MZ2_INFANTRY_MACHINEGUN_9,
	MZ2_INFANTRY_MACHINEGUN_10,
	MZ2_INFANTRY_MACHINEGUN_11,
	MZ2_INFANTRY_MACHINEGUN_12,
	MZ2_INFANTRY_MACHINEGUN_13,
	
	MZ2_SOLDIER_BLASTER_1,
	MZ2_SOLDIER_BLASTER_2,
	MZ2_SOLDIER_SHOTGUN_1,
	MZ2_SOLDIER_SHOTGUN_2,
	MZ2_SOLDIER_MACHINEGUN_1,
	MZ2_SOLDIER_MACHINEGUN_2,
	
	MZ2_GUNNER_MACHINEGUN_1,
	MZ2_GUNNER_MACHINEGUN_2,
	MZ2_GUNNER_MACHINEGUN_3,
	MZ2_GUNNER_MACHINEGUN_4,
	MZ2_GUNNER_MACHINEGUN_5,
	MZ2_GUNNER_MACHINEGUN_6,
	MZ2_GUNNER_MACHINEGUN_7,
	MZ2_GUNNER_MACHINEGUN_8,
	MZ2_GUNNER_GRENADE_1,
	MZ2_GUNNER_GRENADE_2,
	MZ2_GUNNER_GRENADE_3,
	MZ2_GUNNER_GRENADE_4,

	MZ2_CHICK_ROCKET_1,
	
	MZ2_FLYER_BLASTER_1,
	MZ2_FLYER_BLASTER_2,
	
	MZ2_MEDIC_BLASTER_1,
	
	MZ2_GLADIATOR_RAILGUN_1,
	
	MZ2_HOVER_BLASTER_1,
	
	MZ2_ACTOR_MACHINEGUN_1,
	
	MZ2_SUPERTANK_MACHINEGUN_1,
	MZ2_SUPERTANK_MACHINEGUN_2,
	MZ2_SUPERTANK_MACHINEGUN_3,
	MZ2_SUPERTANK_MACHINEGUN_4,
	MZ2_SUPERTANK_MACHINEGUN_5,
	MZ2_SUPERTANK_MACHINEGUN_6,
	MZ2_SUPERTANK_ROCKET_1,
	MZ2_SUPERTANK_ROCKET_2,
	MZ2_SUPERTANK_ROCKET_3,
	
	MZ2_BOSS2_MACHINEGUN_L1,
	MZ2_BOSS2_MACHINEGUN_L2,
	MZ2_BOSS2_MACHINEGUN_L3,
	MZ2_BOSS2_MACHINEGUN_L4,
	MZ2_BOSS2_MACHINEGUN_L5,
	MZ2_BOSS2_ROCKET_1,
	MZ2_BOSS2_ROCKET_2,
	MZ2_BOSS2_ROCKET_3,
	MZ2_BOSS2_ROCKET_4,
	
	MZ2_FLOAT_BLASTER_1,

	MZ2_SOLDIER_BLASTER_3,
	MZ2_SOLDIER_SHOTGUN_3,
	MZ2_SOLDIER_MACHINEGUN_3,
	MZ2_SOLDIER_BLASTER_4,
	MZ2_SOLDIER_SHOTGUN_4,
	MZ2_SOLDIER_MACHINEGUN_4,
	MZ2_SOLDIER_BLASTER_5,
	MZ2_SOLDIER_SHOTGUN_5,
	MZ2_SOLDIER_MACHINEGUN_5,
	MZ2_SOLDIER_BLASTER_6,
	MZ2_SOLDIER_SHOTGUN_6,
	MZ2_SOLDIER_MACHINEGUN_6,
	MZ2_SOLDIER_BLASTER_7,
	MZ2_SOLDIER_SHOTGUN_7,
	MZ2_SOLDIER_MACHINEGUN_7,
	MZ2_SOLDIER_BLASTER_8,
	MZ2_SOLDIER_SHOTGUN_8,
	MZ2_SOLDIER_MACHINEGUN_8,

// Xian
	MZ2_MAKRON_BFG,
	MZ2_MAKRON_BLASTER_1,
	MZ2_MAKRON_BLASTER_2,
	MZ2_MAKRON_BLASTER_3,
	MZ2_MAKRON_BLASTER_4,
	MZ2_MAKRON_BLASTER_5,
	MZ2_MAKRON_BLASTER_6,
	MZ2_MAKRON_BLASTER_7,
	MZ2_MAKRON_BLASTER_8,
	MZ2_MAKRON_BLASTER_9,
	MZ2_MAKRON_BLASTER_10,
	MZ2_MAKRON_BLASTER_11,
	MZ2_MAKRON_BLASTER_12,
	MZ2_MAKRON_BLASTER_13,
	MZ2_MAKRON_BLASTER_14,
	MZ2_MAKRON_BLASTER_15,
	MZ2_MAKRON_BLASTER_16,
	MZ2_MAKRON_BLASTER_17,
	MZ2_MAKRON_RAILGUN_1,
	MZ2_JORG_MACHINEGUN_L1,
	MZ2_JORG_MACHINEGUN_L2,
	MZ2_JORG_MACHINEGUN_L3,
	MZ2_JORG_MACHINEGUN_L4,
	MZ2_JORG_MACHINEGUN_L5,
	MZ2_JORG_MACHINEGUN_L6,
	MZ2_JORG_MACHINEGUN_R1,
	MZ2_JORG_MACHINEGUN_R2,
	MZ2_JORG_MACHINEGUN_R3,
	MZ2_JORG_MACHINEGUN_R4,
	MZ2_JORG_MACHINEGUN_R5,
	MZ2_JORG_MACHINEGUN_R6,
	MZ2_JORG_BFG_1,
	MZ2_BOSS2_MACHINEGUN_R1,
	MZ2_BOSS2_MACHINEGUN_R2,
	MZ2_BOSS2_MACHINEGUN_R3,
	MZ2_BOSS2_MACHINEGUN_R4,
	MZ2_BOSS2_MACHINEGUN_R5,
// Xian

//ROGUE
	MZ2_CARRIER_MACHINEGUN_L1,
	MZ2_CARRIER_MACHINEGUN_R1,
	MZ2_CARRIER_GRENADE,
	MZ2_TURRET_MACHINEGUN,
	MZ2_TURRET_ROCKET,
	MZ2_TURRET_BLASTER,
	MZ2_STALKER_BLASTER,
	MZ2_DAEDALUS_BLASTER,
	MZ2_MEDIC_BLASTER_2,
	MZ2_CARRIER_RAILGUN,
	MZ2_WIDOW_DISRUPTOR,
	MZ2_WIDOW_BLASTER,
	MZ2_WIDOW_RAIL,
	MZ2_WIDOW_PLASMABEAM,			// PMM - not used
	MZ2_CARRIER_MACHINEGUN_L2,
	MZ2_CARRIER_MACHINEGUN_R2,
	MZ2_WIDOW_RAIL_LEFT,
	MZ2_WIDOW_RAIL_RIGHT,
	MZ2_WIDOW_BLASTER_SWEEP1,
	MZ2_WIDOW_BLASTER_SWEEP2,
	MZ2_WIDOW_BLASTER_SWEEP3,
	MZ2_WIDOW_BLASTER_SWEEP4,
	MZ2_WIDOW_BLASTER_SWEEP5,
	MZ2_WIDOW_BLASTER_SWEEP6,
	MZ2_WIDOW_BLASTER_SWEEP7,
	MZ2_WIDOW_BLASTER_SWEEP8,
	MZ2_WIDOW_BLASTER_SWEEP9,
	MZ2_WIDOW_BLASTER_100,
	MZ2_WIDOW_BLASTER_90,
	MZ2_WIDOW_BLASTER_80,
	MZ2_WIDOW_BLASTER_70,
	MZ2_WIDOW_BLASTER_60,
	MZ2_WIDOW_BLASTER_50,
	MZ2_WIDOW_BLASTER_40,
	MZ2_WIDOW_BLASTER_30,
	MZ2_WIDOW_BLASTER_20,
	MZ2_WIDOW_BLASTER_10,
	MZ2_WIDOW_BLASTER_0,
	MZ2_WIDOW_BLASTER_10L,
	MZ2_WIDOW_BLASTER_20L,
	MZ2_WIDOW_BLASTER_30L,
	MZ2_WIDOW_BLASTER_40L,
	MZ2_WIDOW_BLASTER_50L,
	MZ2_WIDOW_BLASTER_60L,
	MZ2_WIDOW_BLASTER_70L,
	MZ2_WIDOW_RUN_1,
	MZ2_WIDOW_RUN_2,
	MZ2_WIDOW_RUN_3,
	MZ2_WIDOW_RUN_4,
	MZ2_WIDOW_RUN_5,
	MZ2_WIDOW_RUN_6,
	MZ2_WIDOW_RUN_7,
	MZ2_WIDOW_RUN_8,
	MZ2_CARRIER_ROCKET_1,
	MZ2_CARRIER_ROCKET_2,
	MZ2_CARRIER_ROCKET_3,
	MZ2_CARRIER_ROCKET_4,
	MZ2_WIDOW2_BEAMER_1,
	MZ2_WIDOW2_BEAMER_2,
	MZ2_WIDOW2_BEAMER_3,
	MZ2_WIDOW2_BEAMER_4,
	MZ2_WIDOW2_BEAMER_5,
	MZ2_WIDOW2_BEAM_SWEEP_1,
	MZ2_WIDOW2_BEAM_SWEEP_2,
	MZ2_WIDOW2_BEAM_SWEEP_3,
	MZ2_WIDOW2_BEAM_SWEEP_4,
	MZ2_WIDOW2_BEAM_SWEEP_5,
	MZ2_WIDOW2_BEAM_SWEEP_6,
	MZ2_WIDOW2_BEAM_SWEEP_7,
	MZ2_WIDOW2_BEAM_SWEEP_8,
	MZ2_WIDOW2_BEAM_SWEEP_9,
	MZ2_WIDOW2_BEAM_SWEEP_10,
	MZ2_WIDOW2_BEAM_SWEEP_11
// ROGUE
};

// temp entity events
//
// Temp entity events are for things that happen
// at a location seperate from any existing entity.
// Temporary entity messages are explicitly constructed
// and broadcast.
enum temp_event_t
{
	TE_GUNSHOT,
	TE_BLOOD,
	TE_BLASTER,
	TE_RAILTRAIL,
	TE_SHOTGUN,
	TE_EXPLOSION1,
	TE_EXPLOSION2,
	TE_ROCKET_EXPLOSION,
	TE_GRENADE_EXPLOSION,
	TE_SPARKS,
	TE_SPLASH,
	TE_BUBBLETRAIL,
	TE_SCREEN_SPARKS,
	TE_SHIELD_SPARKS,
	TE_BULLET_SPARKS,
	TE_LASER_SPARKS,
	TE_PARASITE_ATTACK,
	TE_ROCKET_EXPLOSION_WATER,
	TE_GRENADE_EXPLOSION_WATER,
	TE_MEDIC_CABLE_ATTACK,
	TE_BFG_EXPLOSION,
	TE_BFG_BIGEXPLOSION,
	TE_BOSSTPORT,	// used as '22' in a map, so DON'T RENUMBER!!!
	TE_BFG_LASER,
	TE_GRAPPLE_CABLE,
	TE_WELDING_SPARKS,
	TE_GREENBLOOD,
	TE_BLUEHYPERBLASTER,
	TE_PLASMA_EXPLOSION,
	TE_TUNNEL_SPARKS,
//ROGUE
	TE_BLASTER2,
	TE_RAILTRAIL2,
	TE_FLAME,
	TE_LIGHTNING,
	TE_DEBUGTRAIL,
	TE_PLAIN_EXPLOSION,
	TE_FLASHLIGHT,
	TE_FORCEWALL,
	TE_HEATBEAM,
	TE_MONSTER_HEATBEAM,
	TE_STEAM,
	TE_BUBBLETRAIL2,
	TE_MOREBLOOD,
	TE_HEATBEAM_SPARKS,
	TE_HEATBEAM_STEAM,
	TE_CHAINFIST_SMOKE,
	TE_ELECTRIC_SPARKS,
	TE_TRACKER_EXPLOSION,
	TE_TELEPORT_EFFECT,
	TE_DBALL_GOAL,
	TE_WIDOWBEAMOUT,
	TE_NUKEBLAST,
	TE_WIDOWSPLASH,
	TE_EXPLOSION1_BIG,
	TE_EXPLOSION1_NP,
	TE_FLECHETTE,
//ROGUE

	TE_NUM_ENTITIES
};

// color for TE_SPLASH
enum splash_type_t
{
	SPLASH_UNKNOWN,
	SPLASH_SPARKS,
	SPLASH_BLUE_WATER,
	SPLASH_BROWN_WATER,
	SPLASH_SLIME,
	SPLASH_LAVA,
	SPLASH_BLOOD
};

//
// SOUNDS
//

// sound channels
// channel 0 never willingly overrides
// other channels (1-7) allways override a playing sound on that channel
enum sound_channel_t
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

// sound attenuation values
constexpr vec_t ATTN_NONE	= 0;	// full volume the entire level
constexpr vec_t ATTN_NORM	= 1;
constexpr vec_t	ATTN_IDLE	= 2;
constexpr vec_t ATTN_STATIC	= 3;	// diminish very rapidly with distance

using sound_attn_t = vec_t;

//
// config strings are a general means of communication from
// the server to all connected clients.
// Each config string can be at most MAX_QPATH characters.
//
enum config_string_t
{
	CS_NAME,
	CS_CDTRACK,
	CS_SKY,
	CS_SKYAXIS,		// %f %f %f format
	CS_SKYROTATE,
	CS_STATUSBAR,	// display program string
	
	CS_AIRACCEL	= 29,	// air acceleration control
	CS_MAXCLIENTS,
	CS_MAPCHECKSUM,		// for catching cheater maps
	
	CS_MODELS,
	CS_SOUNDS			= CS_MODELS + MAX_MODELS,
	CS_IMAGES			= CS_SOUNDS + MAX_SOUNDS,
	CS_LIGHTS			= CS_IMAGES + MAX_IMAGES,
	CS_ITEMS			= CS_LIGHTS + MAX_LIGHTSTYLES,
	CS_PLAYERSKINS		= CS_ITEMS + MAX_ITEMS,
	CS_GENERAL			= CS_PLAYERSKINS + MAX_CLIENTS,
	MAX_CONFIGSTRINGS	= CS_GENERAL + MAX_GENERAL
};

// Some mods actually exploit CS_STATUSBAR to take space up to CS_AIRACCEL
constexpr size_t CS_SIZE(const config_string_t &cs)
{
	return (cs >= CS_STATUSBAR && cs < CS_AIRACCEL ? MAX_QPATH * (CS_AIRACCEL - cs) : MAX_QPATH);
}

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
enum content_flags_t
{
	CONTENTS_SOLID			= 1,	// an eye is never valid in a solid
	CONTENTS_WINDOW			= 2,	// translucent, but not watery
	CONTENTS_AUX			= 4,
	CONTENTS_LAVA			= 8,
	CONTENTS_SLIME			= 16,
	CONTENTS_WATER			= 32,
	CONTENTS_MIST			= 64,
	LAST_VISIBLE_CONTENTS	= 64,

// remaining contents are non-visible, and don't eat brushes

	CONTENTS_AREAPORTAL		= 0x8000,

	CONTENTS_PLAYERCLIP		= 0x10000,
	CONTENTS_MONSTERCLIP	= 0x20000,

// currents can be added to any other contents, and may be mixed
	CONTENTS_CURRENT_0		= 0x40000,
	CONTENTS_CURRENT_90		= 0x80000,
	CONTENTS_CURRENT_180	= 0x100000,
	CONTENTS_CURRENT_270	= 0x200000,
	CONTENTS_CURRENT_UP		= 0x400000,
	CONTENTS_CURRENT_DOWN	= 0x800000,
	
	CONTENTS_ORIGIN			= 0x1000000,	// removed before bsping an entity
	
	CONTENTS_MONSTER		= 0x2000000,	// should never be on a brush, only in game
	CONTENTS_DEADMONSTER	= 0x4000000,
	CONTENTS_DETAIL			= 0x8000000,	// brushes to be added after vis leafs
	CONTENTS_TRANSLUCENT	= 0x10000000,	// auto set if any surface has trans
	CONTENTS_LADDER			= 0x20000000,

// content masks
	MASK_ALL			= -1,
	MASK_SOLID			= CONTENTS_SOLID | CONTENTS_WINDOW,
	MASK_PLAYERSOLID	= CONTENTS_SOLID | CONTENTS_PLAYERCLIP | CONTENTS_WINDOW | CONTENTS_MONSTER,
	MASK_DEADSOLID		= CONTENTS_SOLID | CONTENTS_PLAYERCLIP | CONTENTS_WINDOW,
	MASK_MONSTERSOLID	= CONTENTS_SOLID | CONTENTS_MONSTERCLIP | CONTENTS_WINDOW | CONTENTS_MONSTER,
	MASK_WATER			= CONTENTS_WATER | CONTENTS_LAVA | CONTENTS_SLIME,
	MASK_OPAQUE			= CONTENTS_SOLID | CONTENTS_SLIME | CONTENTS_LAVA,
	MASK_SHOT			= CONTENTS_SOLID | CONTENTS_MONSTER | CONTENTS_WINDOW | CONTENTS_DEADMONSTER,
	MASK_CURRENT		= CONTENTS_CURRENT_0 | CONTENTS_CURRENT_90 | CONTENTS_CURRENT_180 | CONTENTS_CURRENT_270 | CONTENTS_CURRENT_UP | CONTENTS_CURRENT_DOWN
};

enum surface_flags_t
{
	SURF_LIGHT	= 0x1,	// value will hold the light strength
	
	SURF_SLICK	= 0x2,	// effects game physics
	
	SURF_SKY		= 0x4,	// don't draw, but add to skybox
	SURF_WARP		= 0x8,	// turbulent water warp
	SURF_TRANS33	= 0x10,
	SURF_TRANS66	= 0x20,
	SURF_FLOWING	= 0x40,	// scroll towards angle
	SURF_NODRAW		= 0x80,	// don't bother referencing the texture
	
	SURF_ALPHATEST	= 0x02000000	// used by kmquake2
};

enum box_edicts_area_t
{
	AREA_SOLID		= 1,
	AREA_TRIGGERS	= 2
};

enum plane_type_t : uint8_t
{
	// 0-2 are axial planes
	PLANE_X,
	PLANE_Y,
	PLANE_Z,

	// 3-5 are non-axial planes snapped to the nearest
	PLANE_ANYX,
	PLANE_ANYY,
	PLANE_ANYZ,

	// planes (x&~1) and (x&~1)+1 are always opposites
	PLANE_NON_AXIAL
};

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
enum button_bits_t : uint8_t
{
	BUTTON_ATTACK		= 1,
	BUTTON_USE			= 2,
	BUTTON_ANY			= 128		// any key whatsoever
};

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
enum pmtype_t
{
	// can accelerate and turn
	PM_NORMAL,
	PM_SPECTATOR,
	// no acceleration or turning
	PM_DEAD,
	PM_GIB,     // different bounding box
	PM_FREEZE
};

// pmove->pm_flags
enum pmflags_t : uint8_t
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

// default server FPS
const uint32_t	BASE_FRAMERATE		= 10;
const uint32_t	BASE_FRAMETIME		= 100;
const vec_t		BASE_1_FRAMETIME	= 1.0 / BASE_FRAMERATE;
const vec_t		BASE_FRAMETIME_1000	= BASE_FRAMETIME / 1000.f;