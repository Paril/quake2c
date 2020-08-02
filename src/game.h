#pragma once

// memory tags to allow dynamic memory to be cleaned up
#define	TAG_GAME	765		// clear when unloading the dll
#define	TAG_LEVEL	766		// clear when loading a new level

extern game_import_t gi;
extern game_export_t globals;

struct
{
	gclient_t	*clients;
	size_t		num_clients;

	uint8_t		*client_load_data;
} game;

inline edict_t *itoe(const size_t n)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
	return (edict_t *)((uint8_t *)globals.edicts + (n * globals.edict_size));
#pragma GCC diagnostic pop
}
