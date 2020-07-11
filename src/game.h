#pragma once

#include <vector>

// memory tags to allow dynamic memory to be cleaned up
#define	TAG_GAME	765		// clear when unloading the dll
#define	TAG_LEVEL	766		// clear when loading a new level

extern game_import_t gi;
extern game_export_t globals;

struct
{
	inline edict_t &entity(const size_t &n)
	{
		return *reinterpret_cast<edict_t *>(reinterpret_cast<uint8_t *>(globals.edicts) + (n * globals.edict_size));
	}

	std::vector<gclient_t>	clients;

	uint8_t	*client_data_ptr;
	size_t	client_data_size;
} game;