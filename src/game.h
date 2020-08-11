#pragma once

// memory tags to allow dynamic memory to be cleaned up
#define	TAG_GAME	765		// clear when unloading the dll
#define	TAG_LEVEL	766		// clear when loading a new level

extern game_import_t gi;
extern game_export_t globals;

typedef struct
{
	gclient_t	*clients;
	uint32_t	num_clients;

	void		*client_load_data;

	struct {
		uint32_t	is_client;
		uint32_t	owner;
	} fields;
} game_t;

extern game_t game;