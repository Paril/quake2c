#pragma once

typedef uint32_t entity_effects_t;

typedef uint32_t render_effects_t;

typedef int32_t entity_event_t;

// entity_state_t is the information conveyed from the server
// in an update message about entities that the client will
// need to render in some way
typedef struct
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
} entity_state_t;

typedef struct list_s
{
    struct list_s	*next, *prev;
} list_t;

// edict->svflags
typedef int32_t svflags_t;

// edict->solid values
typedef int32_t solid_t;

enum { MAX_ENT_CLUSTERS	= 16 };

typedef struct edict_s edict_t;

typedef struct edict_s
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
} edict_t;