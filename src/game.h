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

	struct {
		qcvm_function_t		*ClientConnect;
		qcvm_function_t		*ClientBegin;
		qcvm_function_t		*ClientUserinfoChanged;
		qcvm_function_t		*ClientDisconnect;
		qcvm_function_t		*ClientCommand;
		qcvm_function_t		*ClientThink;

		qcvm_function_t		*RunFrame;

		qcvm_function_t		*ServerCommand;
	} funcs;
} game_t;

extern game_t game;