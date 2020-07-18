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
	
	func_t		PreWriteGame;
	func_t		PostWriteGame;
	
	func_t		PreReadGame;
	func_t		PostReadGame;

	func_t		PreWriteLevel;
	func_t		PostWriteLevel;
	
	func_t		PreReadLevel;
	func_t		PostReadLevel;
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

static void ShutdownGame()
{
	auto func = qvm.FindFunction(qce.ShutdownGame);
	qvm.Execute(*func);

	ShutdownVM();

	gi.FreeTags (TAG_LEVEL);
	gi.FreeTags (TAG_GAME);
}

static void AssignClientPointers()
{
	for (size_t i = 0; i < game.clients.size(); i++)
		game.entity(i + 1).client = &game.clients[i];
}

static void WipeEntities()
{
	memset(globals.edicts, 0, globals.max_edicts * globals.edict_size);

	for (int32_t i = 0; i < globals.max_edicts; i++)
		game.entity(i).s.number = i;

	AssignClientPointers();
}

static void SpawnEntities(const char *mapname, const char *entities, const char *spawnpoint)
{
	gi.FreeTags(TAG_LEVEL);

	WipeEntities();

	auto func = qvm.FindFunction(qce.SpawnEntities);
	qvm.SetGlobalStr(global_t::PARM0, std::string(mapname));
	qvm.SetGlobalStr(global_t::PARM1, std::string(entities));
	qvm.SetGlobalStr(global_t::PARM2, std::string(spawnpoint));
	qvm.Execute(*func);
}

static qboolean ClientConnect(edict_t *e, char *userinfo)
{
	auto func = qvm.FindFunction(qce.ClientConnect);
	qvm.SetGlobal(global_t::PARM0, qvm.EntityToEnt(e));
	qvm.SetGlobalStr(global_t::PARM1, std::string(userinfo));
	qvm.Execute(*func);

	Q_strlcpy(userinfo, qvm.GetString(qvm.GetGlobal<string_t>(global_t::PARM1)), MAX_INFO_STRING);

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
	qvm.SetGlobalStr(global_t::PARM1, std::string(userinfo));
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


//=========================================================

const uint32_t	SAVE_MAGIC1		= (('V'<<24)|('S'<<16)|('C'<<8)|'Q');	// "QCSV"
const uint32_t	SAVE_MAGIC2		= (('A'<<24)|('S'<<16)|('C'<<8)|'Q');	// "QCSA"
const uint32_t	SAVE_VERSION	= 666;

enum class pointer_class_type_t : uint8_t
{
	POINTER_NONE,
	POINTER_GLOBAL,
	POINTER_ENT
};

static void WriteDefinitionData(std::ostream &stream, const QCDefinition &def, const global_t *value)
{
	const deftype_t type = static_cast<deftype_t>(def.id & ~TYPE_GLOBAL);

	if (type == TYPE_STRING)
	{
		const char *str = qvm.GetString(static_cast<string_t>(*value));
		stream <= strlen(str);
		stream.write(str, strlen(str));
		return;
	}
	else if (type == TYPE_FUNCTION)
	{
		func_t func = static_cast<func_t>(*value);
		auto &func_ptr = qvm.functions[static_cast<size_t>(func)];
		const char *str = qvm.GetString(func_ptr.name_index);

		stream <= strlen(str);
		stream.write(str, strlen(str));
		return;
	}
	else if (type == TYPE_POINTER)
	{
		if (*value == global_t::QC_NULL)
		{
			stream <= pointer_class_type_t::POINTER_NONE;
			return;
		}

		ptrdiff_t ptr_val = static_cast<ptrdiff_t>(*value);
		
		if (ptr_val >= reinterpret_cast<ptrdiff_t>(qvm.global_data) && ptr_val < reinterpret_cast<ptrdiff_t>(qvm.global_data + qvm.global_size))
		{
			stream <= pointer_class_type_t::POINTER_GLOBAL;

			// find closest def
			global_t def_id = static_cast<global_t>((ptr_val - reinterpret_cast<ptrdiff_t>(qvm.global_data)) / sizeof(global_t));
			size_t offset = 0;

			while (def_id >= global_t::QC_OFS)
			{
				if (qvm.definition_map_by_id.contains(def_id))
					break;

				def_id = static_cast<global_t>(static_cast<int32_t>(def_id) - 1);
				offset++;
			}

			if (def_id < global_t::QC_OFS)
				qvm.Error("couldn't find ptr reference");

			auto closest_def = qvm.definition_map_by_id.at(def_id);
			const char *str = qvm.GetString(closest_def->name_index);

			stream <= strlen(str);
			stream.write(str, strlen(str));
			stream <= offset;
		}
		else if (ptr_val >= reinterpret_cast<ptrdiff_t>(globals.edicts) && ptr_val < reinterpret_cast<ptrdiff_t>(globals.edicts) + (globals.edict_size * globals.max_edicts))
		{
			qvm.Error("somehow got a bad field ptr");
			//stream <= pointer_class_type_t::POINTER_ENT;
		}
		else
			qvm.Error("somehow got a bad field ptr");
		
		return;
	}
	else if (type == TYPE_ENTITY)
	{
		auto ent = qvm.EntToEntity(static_cast<ent_t>(*value));

		if (ent == nullptr)
			stream <= globals.max_edicts;
		else
			stream <= ent->s.number;

		return;
	}
	
	const size_t len = (type == TYPE_VECTOR) ? 3 : 1;
	stream.write(reinterpret_cast<const char *>(value), sizeof(global_t) * len);
}

static void WriteEntityFieldData(std::ostream &stream, edict_t &ent, const QCDefinition &def)
{
	auto field = qvm.GetEntityFieldPointer(ent, def.global_index);
	WriteDefinitionData(stream, def, reinterpret_cast<global_t *>(field));
}

static void ReadDefinitionData(std::istream &stream, const QCDefinition &def, global_t *value)
{
	const deftype_t type = static_cast<deftype_t>(def.id & ~TYPE_GLOBAL);

	if (type == TYPE_STRING)
	{
		std::string def_value;
		size_t def_len;
		stream >= def_len;
		def_value.resize(def_len);
		stream.read(def_value.data(), def_len);
		qvm.SetStringPtr(value, std::move(def_value));
		return;
	}
	else if (type == TYPE_FUNCTION)
	{
		std::string func_name;
		size_t func_len;
		stream >= func_len;

		if (!func_len)
			*value = global_t::QC_NULL;
		else
		{
			func_name.resize(func_len);
			stream.read(func_name.data(), func_len);

			*value = static_cast<global_t>(qvm.FindFunctionID(func_name.data()));

			if (*value == global_t::QC_NULL)
				qvm.Error("can't find func %s", func_name.data());
		}
		return;
	}
	else if (type == TYPE_POINTER)
	{
		pointer_class_type_t ptrclass;
		stream >= ptrclass;

		if (ptrclass == pointer_class_type_t::POINTER_NONE)
		{
			*value = global_t::QC_NULL;
			return;
		}
		else if (ptrclass == pointer_class_type_t::POINTER_GLOBAL)
		{
			std::string global_name;
			size_t global_len;
			stream >= global_len;
			global_name.resize(global_len);
			stream.read(global_name.data(), global_len);

			string_t str;
			
			if (!qvm.FindString(global_name, str))
				qvm.Error("bad pointer; can't find %s", global_name.data());

			if (!qvm.definition_map.contains(str))
				qvm.Error("bad pointer; can't map %s", global_name.data());

			auto global_def = qvm.definition_map.at(str);
			size_t global_offset;
			stream >= global_offset;

			auto ptr = qvm.GetGlobalByIndex(static_cast<global_t>(global_def->global_index + global_offset));
			*value = static_cast<global_t>(reinterpret_cast<ptrdiff_t>(ptr));
			return;
		}
		else if (ptrclass == pointer_class_type_t::POINTER_ENT)
		{
			qvm.Error("bad pointer");
		}
		
		qvm.Error("bad pointer");
	}
	else if (type == TYPE_ENTITY)
	{
		int32_t number;
		stream >= number;

		if (number == globals.max_edicts)
			*value = static_cast<global_t>(ent_t::ENT_INVALID);
		else
			*value = static_cast<global_t>(qvm.EntityToEnt(&game.entity(number)));

		return;
	}
	
	const size_t len = (type == TYPE_VECTOR) ? 3 : 1;
	stream.read(reinterpret_cast<char *>(value), sizeof(global_t) * len);
}

static void ReadEntityFieldData(std::istream &stream, edict_t &ent, const QCDefinition &def)
{
	auto field = qvm.GetEntityFieldPointer(ent, def.global_index);
	ReadDefinitionData(stream, def, reinterpret_cast<global_t *>(field));
}

/*
============
WriteGame

This will be called whenever the game goes to a new level,
and when the user explicitly saves the game.

Game information include cross level data, like multi level
triggers, help computer info, and all client states.

A single player death will automatically restore from the
last save position.
============
*/
static void WriteGame(const char *filename, qboolean autosave)
{
	std::filesystem::path file_path(filename);
	std::ofstream stream(file_path, std::ios::binary);

	auto func = qvm.FindFunction(qce.PreWriteGame);
	qvm.SetGlobal(global_t::PARM0, autosave);
	qvm.Execute(*func);
	
	stream <= SAVE_MAGIC1;
	stream <= SAVE_VERSION;
	stream <= globals.edict_size;
	stream <= game.clients.size();

	// save "game." values
	for (auto &def : qvm.definitions)
	{
		if (!(def.id & TYPE_GLOBAL))
			continue;

		const char *name = qvm.GetString(def.name_index);

		if (strnicmp(name, "game.", 5))
			continue;

		size_t name_len = strlen(name);
		
		stream <= name_len;
		stream.write(name, name_len);

		WriteDefinitionData(stream, def, qvm.GetGlobalByIndex(static_cast<global_t>(def.global_index)));
	}

	stream <= 0u;

	// save client structs
	for (auto &def : qvm.fields)
	{
		const char *name = qvm.GetString(def.name_index);

		if (strnicmp(name, "client.", 6))
			continue;

		size_t name_len = strlen(name);

		stream <= name_len;
		stream.write(name, name_len);

		for (size_t i = 0; i < game.clients.size(); i++)
			WriteEntityFieldData(stream, game.entity(i + 1), def);
	}

	stream <= 0u;
	
	func = qvm.FindFunction(qce.PostWriteGame);
	qvm.SetGlobal(global_t::PARM0, autosave);
	qvm.Execute(*func);
}

static void ReadGame(const char *filename)
{
	std::filesystem::path file_path(filename);
	std::ifstream stream(file_path, std::ios::binary);

	auto func = qvm.FindFunction(qce.PreReadGame);
	qvm.Execute(*func);

	uint32_t magic, version, edict_size, maxclients;

	stream >= magic;

	if (magic != SAVE_MAGIC1)
		qvm.Error("Not a save game");

	stream >= version;

	if (version != SAVE_VERSION)
		qvm.Error("Savegame from different version (got %d, expected %d)", version, SAVE_VERSION);

	stream >= edict_size;
	stream >= maxclients;

	// should agree with server's version
	if (game.clients.size() != maxclients)
		qvm.Error("Savegame has bad maxclients");

	if (globals.edict_size != edict_size)
		qvm.Error("Savegame has bad fields");

	// setup entities
	WipeEntities();
	
	// free any string refs inside of the entity structure
	qvm.dynamic_strings.CheckRefUnset(globals.edicts, (globals.edict_size * globals.max_edicts) / sizeof(global_t));

	// load game globals
	std::string def_name;
	std::string def_value;

	while (true)
	{
		size_t len;

		stream >= len;

		if (!len)
			break;

		def_name.resize(len);

		stream.read(def_name.data(), len);

		string_t str;
		
		if (!qvm.FindString(def_name, str) || qvm.dynamic_strings.IsRefCounted(str))
			qvm.Error("Bad string in save file");

		if (!qvm.definition_map.contains(str))
			qvm.Error("Bad definition %s", def_name.data());

		auto &def = *qvm.definition_map.at(str);
		ReadDefinitionData(stream, def, qvm.GetGlobalByIndex(static_cast<global_t>(def.global_index)));
	}

	// load client fields
	while (true)
	{
		size_t len;

		stream >= len;

		if (!len)
			break;

		def_name.resize(len);

		stream.read(def_name.data(), len);

		string_t str;
		
		if (!qvm.FindString(def_name, str) || qvm.dynamic_strings.IsRefCounted(str))
			qvm.Error("Bad string in save file");

		if (!qvm.field_map_by_name.contains(def_name))
			qvm.Error("Bad field %s", def_name.data());
		
		auto &field = *qvm.field_map_by_name.at(def_name);
		
		for (size_t i = 0; i < game.clients.size(); i++)
			ReadEntityFieldData(stream, game.entity(i + 1), field);
	}

	func = qvm.FindFunction(qce.PostReadGame);
	qvm.Execute(*func);

	for (size_t i = 0; i < game.clients.size(); i++)
		SyncPlayerState(qvm, &game.entity(i + 1));

	// make a temp copy of the clients' entities for ReadLevel
	game.client_load_data = reinterpret_cast<uint8_t *>(gi.TagMalloc(globals.edict_size * game.clients.size(), TAG_GAME));
	memcpy(game.client_load_data, &game.entity(1), globals.edict_size * game.clients.size());
	qvm.dynamic_strings.MarkIfHasRef(&game.entity(1), game.client_load_data, (globals.edict_size * game.clients.size()) / sizeof(global_t));
}

//==========================================================


/*
=================
WriteLevel

=================
*/
static void WriteLevel(const char *filename)
{
	std::filesystem::path file_path(filename);
	std::ofstream stream(file_path, std::ios::binary);

	auto func = qvm.FindFunction(qce.PreWriteLevel);
	qvm.Execute(*func);
	
	stream <= SAVE_MAGIC2;
	stream <= SAVE_VERSION;
	stream <= globals.edict_size;
	stream <= game.clients.size();

	// save "level." values
	for (auto &def : qvm.definitions)
	{
		if (!(def.id & TYPE_GLOBAL))
			continue;

		const char *name = qvm.GetString(def.name_index);

		if (strnicmp(name, "level.", 5))
			continue;

		size_t name_len = strlen(name);

		stream <= name_len;
		stream.write(name, name_len);

		WriteDefinitionData(stream, def, qvm.GetGlobalByIndex(static_cast<global_t>(def.global_index)));
	}

	stream <= 0u;

	// save non-client structs
	for (auto &def : qvm.fields)
	{
		const char *name = qvm.GetString(def.name_index);

		if (def.name_index == string_t::STRING_EMPTY || strnicmp(name, "client.", 6) == 0)
			continue;

		size_t name_len = strlen(name);
		stream <= name_len;
		stream.write(name, name_len);

		for (size_t i = 0; i < globals.num_edicts; i++)
		{
			auto &ent = game.entity(i);

			if (!ent.inuse)
				continue;

			stream <= i;
			WriteEntityFieldData(stream, ent, def);
		}

		stream <= -1u;
	}

	stream <= 0u;
	
	func = qvm.FindFunction(qce.PostWriteLevel);
	qvm.Execute(*func);
}


/*
=================
ReadLevel

SpawnEntities will allready have been called on the
level the same way it was when the level was saved.

That is necessary to get the baselines
set up identically.

The server will have cleared all of the world links before
calling ReadLevel.

No clients are connected yet.
=================
*/
static void ReadLevel(const char *filename)
{
	std::filesystem::path file_path(filename);
	std::ifstream stream(file_path, std::ios::binary);

	auto func = qvm.FindFunction(qce.PreReadLevel);
	qvm.Execute(*func);

	uint32_t magic, version, edict_size, maxclients;

	stream >= magic;

	if (magic != SAVE_MAGIC2)
		qvm.Error("Not a save game");

	stream >= version;

	if (version != SAVE_VERSION)
		qvm.Error("Savegame from different version (got %d, expected %d)", version, SAVE_VERSION);

	stream >= edict_size;
	stream >= maxclients;

	// should agree with server's version
	if (game.clients.size() != maxclients)
		qvm.Error("Savegame has bad maxclients");

	if (globals.edict_size != edict_size)
		qvm.Error("Savegame has bad fields");

	// setup entities
	WipeEntities();
	globals.num_edicts = game.clients.size() + 1;
	
	// free any string refs inside of the entity structure
	qvm.dynamic_strings.CheckRefUnset(globals.edicts, (globals.edict_size * globals.max_edicts) / sizeof(global_t));

	// load level globals
	std::string def_name;
	std::string def_value;

	while (true)
	{
		size_t len;

		stream >= len;

		if (!len)
			break;

		def_name.resize(len);

		stream.read(def_name.data(), len);

		string_t str;
		
		if (!qvm.FindString(def_name, str) || qvm.dynamic_strings.IsRefCounted(str))
			qvm.Error("Bad string in save file");

		if (!qvm.definition_map.contains(str))
			qvm.Error("Bad definition %s", def_name.data());

		auto &def = *qvm.definition_map.at(str);
		ReadDefinitionData(stream, def, qvm.GetGlobalByIndex(static_cast<global_t>(def.global_index)));
	}

	// load entity fields
	while (true)
	{
		size_t len;

		stream >= len;

		if (!len)
			break;

		def_name.resize(len);

		stream.read(def_name.data(), len);

		string_t str;
		
		if (!qvm.FindString(def_name, str) || qvm.dynamic_strings.IsRefCounted(str))
			qvm.Error("Bad string in save file");

		if (!qvm.field_map_by_name.contains(def_name))
			qvm.Error("Bad field %s", def_name.data());
		
		auto &field = *qvm.field_map_by_name.at(def_name);

		size_t ent_id;

		while (true)
		{
			stream >= ent_id;

			if (ent_id == -1u)
				break;

			if (ent_id >= globals.max_edicts)
				qvm.Error("%s: bad entity number", __func__);

			if (ent_id >= globals.num_edicts)
				globals.num_edicts = ent_id + 1;

			auto &ent = game.entity(ent_id);
			ReadEntityFieldData(stream, ent, field);
		}
	}

	AssignClientPointers();
	
	// copy over any client-specific data back into the clients and re-sync

	for (size_t i = 0; i < game.clients.size(); i++)
	{
		auto &ent = game.entity(i + 1);
		auto &backup = *reinterpret_cast<edict_t *>(game.client_load_data + (globals.edict_size * i));

		// restore client structs
		for (auto &def : qvm.fields)
		{
			const char *name = qvm.GetString(def.name_index);

			if (def.name_index == string_t::STRING_EMPTY || strnicmp(name, "client.", 6))
				continue;

			const size_t len = def.id == TYPE_VECTOR ? 3 : 1;

			auto dst = qvm.GetEntityFieldPointer(ent, def.global_index);
			auto src = qvm.GetEntityFieldPointer(backup, def.global_index);

			memcpy(dst, src, sizeof(global_t) * len);
		}

		SyncPlayerState(qvm, &ent);
	}
	
	qvm.dynamic_strings.MarkIfHasRef(game.client_load_data, &game.entity(1), (globals.edict_size * game.clients.size()) / sizeof(global_t));
	qvm.dynamic_strings.CheckRefUnset(game.client_load_data, (globals.edict_size * game.clients.size()) / sizeof(global_t), true);
	gi.TagFree(game.client_load_data);

	for (size_t i = 0; i < globals.num_edicts; i++)
	{
		auto &ent = game.entity(i);

		// let the server rebuild world links for this ent
		ent.area = {};
		gi.linkentity(&ent);
	}
	
	func = qvm.FindFunction(qce.PostReadLevel);
	qvm.SetGlobal(global_t::PARM0, globals.num_edicts);
	qvm.Execute(*func);
}


game_export_t globals = {
	.apiversion = 3,

	.Init = InitGame,
	.Shutdown = ShutdownGame,

	.SpawnEntities = SpawnEntities,

	.WriteGame = WriteGame,
	.ReadGame = ReadGame,

	.WriteLevel = WriteLevel,
	.ReadLevel = ReadLevel,
	
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
