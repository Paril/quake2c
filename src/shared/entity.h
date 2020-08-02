#pragma once

// entity_state_t->effects
// Effects are things handled on the client side (lights, particles, frame animations)
// that happen constantly on the given entity.
// An entity that has effects will be sent to the client
// even if it has a zero index model.
enum entity_effects_t : uint32_t
{
	EF_ROTATE			= 0x00000001,	// rotate (bonus items)
	EF_GIB				= 0x00000002,	// leave a trail
	EF_BLASTER			= 0x00000008,	// redlight + trail
	EF_ROCKET			= 0x00000010,	// redlight + trail
	EF_GRENADE			= 0x00000020,
	EF_HYPERBLASTER		= 0x00000040,
	EF_BFG				= 0x00000080,
	EF_COLOR_SHELL		= 0x00000100,
	EF_POWERSCREEN		= 0x00000200,
	EF_ANIM01			= 0x00000400,	// automatically cycle between frames 0 and 1 at 2 hz
	EF_ANIM23			= 0x00000800,	// automatically cycle between frames 2 and 3 at 2 hz
	EF_ANIM_ALL			= 0x00001000,	// automatically cycle through all frames at 2hz
	EF_ANIM_ALLFAST		= 0x00002000,	// automatically cycle through all frames at 10hz
	EF_FLIES			= 0x00004000,
	EF_QUAD				= 0x00008000,
	EF_PENT				= 0x00010000,
	EF_TELEPORTER		= 0x00020000,	// particle fountain
	EF_FLAG1			= 0x00040000,
	EF_FLAG2			= 0x00080000,
// RAFAEL
	EF_IONRIPPER		= 0x00100000,
	EF_GREENGIB			= 0x00200000,
	EF_BLUEHYPERBLASTER	= 0x00400000,
	EF_SPINNINGLIGHTS	= 0x00800000,
	EF_PLASMA			= 0x01000000,
	EF_TRAP				= 0x02000000,
// RAFAEL

//ROGUE
	EF_TRACKER			= 0x04000000,
	EF_DOUBLE			= 0x08000000,
	EF_SPHERETRANS		= 0x10000000,
	EF_TAGTRAIL			= 0x20000000,
	EF_HALF_DAMAGE		= 0x40000000,
	EF_TRACKERTRAIL		= 0x80000000
//ROGUE
};

// entity_state_t->renderfx flags
enum render_effects_t : uint32_t
{
	RF_MINLIGHT		= 1,	// allways have some light (viewmodel)
	RF_VIEWERMODEL	= 2,	// don't draw through eyes, only mirrors
	RF_WEAPONMODEL	= 4,	// only draw through eyes
	RF_FULLBRIGHT	= 8,	// allways draw full intensity
	RF_DEPTHHACK	= 16,	// for view weapon Z crunching
	RF_TRANSLUCENT	= 32,
	RF_FRAMELERP	= 64,
	RF_BEAM			= 128,
	RF_CUSTOMSKIN	= 256,	// skin is an index in image_precache
	RF_GLOW			= 512,	// pulse lighting for bonus items
	RF_SHELL_RED	= 1024,
	RF_SHELL_GREEN	= 2048,
	RF_SHELL_BLUE	= 4096,

//ROGUE
	RF_IR_VISIBLE		= 0x00008000,      // 32768
	RF_SHELL_DOUBLE		= 0x00010000,      // 65536
	RF_SHELL_HALF_DAM	= 0x00020000,
	RF_USE_DISGUISE		= 0x00040000
//ROGUE
};

// entity_state_t->event values
// ertity events are for effects that take place reletive
// to an existing entities origin.  Very network efficient.
// All muzzle flashes really should be converted to events...
enum entity_event_t
{
	EV_NONE,
	EV_ITEM_RESPAWN,
	EV_FOOTSTEP,
	EV_FALLSHORT,
	EV_FALL,
	EV_FALLFAR,
	EV_PLAYER_TELEPORT,
	EV_OTHER_TELEPORT
};

// entity_state_t is the information conveyed from the server
// in an update message about entities that the client will
// need to render in some way
struct entity_state_t
{
	int     number;         // edict index

	vec3_t  origin;
	vec3_t  angles;
	vec3_t  old_origin;     // for lerping
	int     modelindex;
	int     modelindex2, modelindex3, modelindex4;  // weapons, CTF flags, etc
	int     frame;
	int     skinnum;
	entity_effects_t	effects;
	render_effects_t	renderfx;
	int     solid;			// for client side prediction, 8*(bits 0-4) is x/y radius
							// 8*(bits 5-9) is z down distance, 8(bits10-15) is z up
							// gi.linkentity sets this properly
	int     sound;			// for looping sounds, to guarantee shutoff
	entity_event_t	event;	// impulse events -- muzzle flashes, footsteps, etc
							// events only go out for a single frame, they
							// are automatically cleared each frame
};

struct list_t
{
    list_t  *next; // head
    list_t  *prev; // tail
};

// edict->svflags
enum svflags_t
{
	SVF_NOCLIENT	= 0x00000001,	// don't send entity to clients, even if it has effects
	SVF_DEADMONSTER	= 0x00000002,	// treat as CONTENTS_DEADMONSTER for collision
	SVF_MONSTER		= 0x00000004,	// treat as CONTENTS_MONSTER for collision
};

// edict->solid values
enum solid_t
{
	SOLID_NOT,          // no interaction with other objects
	SOLID_TRIGGER,      // only touch when inside, after moving
	SOLID_BBOX,         // touch on edge
	SOLID_BSP           // bsp clip, touch on edge
};

static const size_t MAX_ENT_CLUSTERS	= 16;

typedef struct edict_s edict_t;

struct edict_s
{
	entity_state_t	s;
	gclient_t		*client;
	qboolean		inuse;
	int				linkcount;

	list_t			area;				// linked to a division node or leaf

	int             num_clusters;		// if -1, use headnode instead
	int             clusternums[MAX_ENT_CLUSTERS];
	int             headnode;			// unused if num_clusters != -1
	int             areanum, areanum2;

	//================================

	svflags_t		svflags;            // SVF_NOCLIENT, SVF_DEADMONSTER, SVF_MONSTER, etc
	vec3_t			mins, maxs;
	vec3_t			absmin, absmax, size;
	solid_t			solid;
	content_flags_t	clipmask;
	edict_t			*owner;
};