/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
#include "shared/shared.h"
#include "game.h"
#include "g_vm.h"
#include "vm_game.h"

static void InitBuiltins()
{
	InitGIBuiltins(qvm);
	InitGameBuiltins(qvm);
	InitExtBuiltins(qvm);
	InitStringBuiltins(qvm);
	InitMemBuiltins(qvm);
	InitDebugBuiltins(qvm);
	InitVectorBuiltins(qvm);
	InitMathBuiltins(qvm);
}

template<typename T>
static void FieldWrapToT(uint8_t *out, const int32_t *in)
{
	*reinterpret_cast<T *>(out) = *in;
}

static void InitFieldWraps()
{
#define RegisterSingle(name) \
	qvm.field_wraps.Register("client." #name, 0, offsetof(gclient_t, name), nullptr)

#define RegisterSingleWrapped(name, wrap) \
	qvm.field_wraps.Register("client." #name, 0, offsetof(gclient_t, name), wrap)

#define RegisterArray(name, fofs, sofs) \
	qvm.field_wraps.Register("client." #name "[" #fofs "]", 0, offsetof(gclient_t, name) + sofs, nullptr)

#define RegisterVector(name) \
	qvm.field_wraps.Register("client." #name, 0, offsetof(gclient_t, name), nullptr); \
	qvm.field_wraps.Register("client." #name, 1, offsetof(gclient_t, name) + 4, nullptr); \
	qvm.field_wraps.Register("client." #name, 2, offsetof(gclient_t, name) + 8, nullptr)

	// gclient_t
	RegisterSingle(ping);
	RegisterSingle(clientNum);

	// gclient_t::ps
	RegisterVector(ps.viewangles);
	RegisterVector(ps.viewoffset);
	RegisterVector(ps.kick_angles);
	RegisterVector(ps.gunangles);
	RegisterVector(ps.gunoffset);
	RegisterSingle(ps.gunindex);
	RegisterSingle(ps.gunframe);
	RegisterArray(ps.blend, 0, 0);
	RegisterArray(ps.blend, 1, 4);
	RegisterArray(ps.blend, 2, 8);
	RegisterArray(ps.blend, 3, 12);
	RegisterSingle(ps.fov);
	RegisterSingle(ps.rdflags);

	for (int32_t i = 0; i < std::tuple_size_v<decltype(player_state_t::stats)>; i++)
		qvm.field_wraps.Register(va("client.ps.stats[%i]", i), 0, offsetof(gclient_t, ps.stats) + (sizeof(gclient_t::ps.stats[0]) * i), FieldWrapToT<short>);
	
	// gclient_t::ps::pmove
	RegisterSingle(ps.pmove.pm_type);
	
	for (int32_t i = 0; i < std::tuple_size_v<decltype(gclient_t::ps.pmove.origin)>; i++)
		qvm.field_wraps.Register(va("client.ps.pmove.origin[%i]", i), 0, offsetof(gclient_t, ps.pmove.origin) + (sizeof(gclient_t::ps.pmove.origin[0]) * i), FieldWrapToT<short>);
	
	for (int32_t i = 0; i < std::tuple_size_v<decltype(gclient_t::ps.pmove.velocity)>; i++)
		qvm.field_wraps.Register(va("client.ps.pmove.velocity[%i]", i), 0, offsetof(gclient_t, ps.pmove.velocity) + (sizeof(gclient_t::ps.pmove.velocity[0]) * i), FieldWrapToT<short>);
	
	RegisterSingleWrapped(ps.pmove.pm_flags, FieldWrapToT<uint8_t>);
	RegisterSingleWrapped(ps.pmove.pm_time, FieldWrapToT<uint8_t>);
	RegisterSingleWrapped(ps.pmove.gravity, FieldWrapToT<short>);
	
	for (int32_t i = 0; i < std::tuple_size_v<decltype(gclient_t::ps.pmove.delta_angles)>; i++)
		qvm.field_wraps.Register(va("client.ps.pmove.delta_angles[%i]", i), 0, offsetof(gclient_t, ps.pmove.delta_angles) + (sizeof(gclient_t::ps.pmove.delta_angles[0]) * i), FieldWrapToT<short>);
}

// exported from QC
static struct qc_export_t
{
	int32_t		apiversion;
	int32_t		clientsize;
	
	func_t		InitGame;
	func_t		ShutdownGame;
	
	func_t		SpawnEntities;
	
	func_t		ClientConnect;
	func_t		ClientBegin;
	func_t		ClientUserinfoChanged;
	func_t		ClientDisconnect;
	func_t		ClientCommand;
	func_t		ClientThink;

	func_t		RunFrame;

	func_t		ServerCommand;
} qce;

/*
============
InitGame

This will be called when the dll is first loaded, which
only happens when a new game is started or a save game
is loaded.
============
*/
static void InitGame ()
{
	Q_srand(static_cast<uint32_t>(time(NULL)));

	InitVM();

	InitBuiltins();
	InitFieldWraps();

	CheckVM();
	
	// Call GetGameAPI
	auto func = qvm.FindFunction("GetGameAPI");
	qvm.Execute(*func);
	qce = qvm.GetGlobal<qc_export_t>(global_t::PARM0);

	// initialize all clients for this game
	const cvar_t *maxclients = gi.cvar ("maxclients", "4", static_cast<cvar_flags_t>(CVAR_SERVERINFO | CVAR_LATCH));
	game.clients.resize(maxclients->value);

	size_t entity_data_size = 0;

	for (auto &field : qvm.fields)
		entity_data_size = max(entity_data_size, static_cast<size_t>(field.global_index) * sizeof(int32_t));

	// initialize all entities for this game
	globals.max_edicts = MAX_EDICTS;
	globals.edict_size = sizeof(edict_t) + entity_data_size;
	globals.num_edicts = game.clients.size() + 1;
	globals.edicts = reinterpret_cast<edict_t *>(gi.TagMalloc((globals.max_edicts + 1) * globals.edict_size, TAG_GAME));

	func = qvm.FindFunction(qce.InitGame);
	qvm.Execute(*func);
}

#ifdef ALLOW_PROFILING
#include <ostream>
#endif

static void ShutdownGame()
{
	auto func = qvm.FindFunction(qce.ShutdownGame);
	qvm.Execute(*func);

	ShutdownVM();

	gi.FreeTags (TAG_LEVEL);
	gi.FreeTags (TAG_GAME);
}

static void SpawnEntities(const char *mapname, const char *entities, const char *spawnpoint)
{
	gi.FreeTags(TAG_LEVEL);

	memset(globals.edicts, 0, globals.max_edicts * globals.edict_size);

	for (int32_t i = 0; i < globals.max_edicts; i++)
		game.entity(i).s.number = i;

	for (size_t i = 0; i < game.clients.size(); i++)
		game.entity(i + 1).client = &game.clients[i];

	auto func = qvm.FindFunction(qce.SpawnEntities);
	qvm.SetGlobal(global_t::PARM0, std::string(mapname));
	qvm.SetGlobal(global_t::PARM1, std::string(entities));
	qvm.SetGlobal(global_t::PARM2, std::string(spawnpoint));
	qvm.Execute(*func);
}

static qboolean ClientConnect(edict_t *e, char *userinfo)
{
	auto func = qvm.FindFunction(qce.ClientConnect);
	qvm.SetGlobal(global_t::PARM0, qvm.EntityToEnt(e));
	string_t str = qvm.SetGlobal(global_t::PARM1, std::string(userinfo));
	qvm.dynamic_strings.AcquireRefCounted(str);
	qvm.Execute(*func);

	Q_strlcpy(userinfo, qvm.GetString(str), MAX_INFO_STRING);
	qvm.dynamic_strings.ReleaseRefCounted(str);

	return qvm.GetGlobal<qboolean>(global_t::RETURN);
}

static void ClientBegin(edict_t *e)
{
	auto func = qvm.FindFunction(qce.ClientBegin);
	qvm.SetGlobal(global_t::PARM0, qvm.EntityToEnt(e));
	qvm.Execute(*func);
	SyncPlayerState(qvm, e);
}

static void ClientUserinfoChanged(edict_t *e, char *userinfo)
{
	auto func = qvm.FindFunction(qce.ClientUserinfoChanged);
	qvm.SetGlobal(global_t::PARM0, qvm.EntityToEnt(e));
	qvm.SetGlobal(global_t::PARM1, std::string(userinfo));
	qvm.Execute(*func);
	SyncPlayerState(qvm, e);
}

static void ClientDisconnect(edict_t *e)
{
	auto func = qvm.FindFunction(qce.ClientDisconnect);
	qvm.SetGlobal(global_t::PARM0, qvm.EntityToEnt(e));
	qvm.Execute(*func);
	SyncPlayerState(qvm, e);
}

static void ClientCommand(edict_t *e)
{
	auto func = qvm.FindFunction(qce.ClientCommand);
	qvm.SetGlobal(global_t::PARM0, qvm.EntityToEnt(e));
	qvm.Execute(*func);
	SyncPlayerState(qvm, e);
}

static void ClientThink(edict_t *e, usercmd_t *ucmd)
{
	auto func = qvm.FindFunction(qce.ClientThink);
	qvm.SetGlobal(global_t::PARM0, qvm.EntityToEnt(e));

	QC_usercmd_t cmd = {
		ucmd->msec,
		ucmd->buttons,
		{ ucmd->angles[0], ucmd->angles[1], ucmd->angles[2] },
		ucmd->forwardmove,
		ucmd->sidemove,
		ucmd->upmove,
		ucmd->impulse,
		ucmd->lightlevel
	};

	qvm.SetGlobal(global_t::PARM1, cmd);

	qvm.Execute(*func);
	SyncPlayerState(qvm, e);
}

static void RunFrame()
{
	auto func = qvm.FindFunction(qce.RunFrame);
	qvm.Execute(*func);

	for (size_t i = 0; i < game.clients.size(); i++)
		SyncPlayerState(qvm, &game.entity(1 + i));
}

static void ServerCommand()
{
	auto func = qvm.FindFunction(qce.ServerCommand);
	qvm.Execute(*func);
}

game_export_t globals = {
	.apiversion = 3,

	.Init = InitGame,
	.Shutdown = ShutdownGame,

	.SpawnEntities = SpawnEntities,

	.WriteGame = [](const char *, qboolean)
	{
	},
	.ReadGame = [](const char *)
	{
	},

	.WriteLevel = [](const char *)
	{
	},
	.ReadLevel = [](const char *)
	{
	},
	
	.ClientConnect = ClientConnect,
	.ClientBegin = ClientBegin,
	.ClientUserinfoChanged = ClientUserinfoChanged,
	.ClientDisconnect = ClientDisconnect,
	.ClientCommand = ClientCommand,
	.ClientThink = ClientThink,

	.RunFrame = RunFrame,

	.ServerCommand = ServerCommand
};

game_import_t gi;

/*
=================
GetGameAPI

Returns a pointer to the structure with all entry points
and global variables
=================
*/
q_exported game_export_t *GetGameAPI (game_import_t *import)
{
	gi = *import;
	return &globals;
}
