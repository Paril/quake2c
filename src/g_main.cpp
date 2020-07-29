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
	InitMathBuiltins(qvm);
}

template<typename T>
static void FieldWrapToT(void *out, const void *in)
{
	*(T *)(out) = *(const int32_t *)(in);
}

static void FieldCoord2Short(void *out, const void *in)
{
	*(short *)(out) = (*(const vec_t *)(in) * coord2short);
}

static void FieldCoord2Angle(void *out, const void *in)
{
	*(short *)(out) = (*(const vec_t *)(in) * angle2short);
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

#define RegisterVectorCoord2Short(name) \
	qvm.field_wraps.Register("client." #name, 0, offsetof(gclient_t, name), FieldCoord2Short); \
	qvm.field_wraps.Register("client." #name, 1, offsetof(gclient_t, name) + 2, FieldCoord2Short); \
	qvm.field_wraps.Register("client." #name, 2, offsetof(gclient_t, name) + 4, FieldCoord2Short)

#define RegisterVectorCoord2Angle(name) \
	qvm.field_wraps.Register("client." #name, 0, offsetof(gclient_t, name), FieldCoord2Angle); \
	qvm.field_wraps.Register("client." #name, 1, offsetof(gclient_t, name) + 2, FieldCoord2Angle); \
	qvm.field_wraps.Register("client." #name, 2, offsetof(gclient_t, name) + 4, FieldCoord2Angle)

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
	
	RegisterVectorCoord2Short(ps.pmove.origin);
	RegisterVectorCoord2Short(ps.pmove.velocity);
	
	RegisterSingleWrapped(ps.pmove.pm_flags, FieldWrapToT<uint8_t>);
	RegisterSingleWrapped(ps.pmove.pm_time, FieldWrapToT<uint8_t>);
	RegisterSingleWrapped(ps.pmove.gravity, FieldWrapToT<short>);

	RegisterVectorCoord2Angle(ps.pmove.delta_angles);
}

// exported from QC
static struct qc_export_t
{
	int32_t		apiversion;
	int32_t		clientsize;
	
	func_t		InitGame;
	func_t		ShutdownGame;
	
	func_t		PreSpawnEntities;
	func_t		SpawnEntities;
	func_t		PostSpawnEntities;
	
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
	InitVM();

	InitBuiltins();
	InitFieldWraps();

	CheckVM();

#ifdef ALLOW_DEBUGGING
	CheckDebuggerCommands();
#endif

	// Call GetGameAPI
	QCFunction *func = qvm.FindFunction("GetGameAPI");
	qvm.Execute(func);
	qce = qvm.GetGlobal<qc_export_t>(GLOBAL_PARM0);

	// initialize all clients for this game
	const cvar_t *maxclients = gi.cvar ("maxclients", "4", CVAR_SERVERINFO | CVAR_LATCH);
	game.num_clients = min(MAX_CLIENTS, (size_t)maxclients->value);
	game.clients = (gclient_t *)gi.TagMalloc(sizeof(gclient_t) * game.num_clients, TAG_GAME);

	size_t entity_data_size = 0;

	for (auto &field : qvm.fields)
		entity_data_size = max(entity_data_size, field.global_index * sizeof(int32_t));

	// initialize all entities for this game
	globals.max_edicts = MAX_EDICTS;
	globals.edict_size = sizeof(edict_t) + entity_data_size;
	globals.num_edicts = game.num_clients + 1;
	globals.edicts = (edict_t *)gi.TagMalloc((globals.max_edicts + 1) * globals.edict_size, TAG_GAME);

	func = qvm.FindFunction(qce.InitGame);
	qvm.Execute(func);
}

static void ShutdownGame()
{
#ifdef ALLOW_DEBUGGING
	CheckDebuggerCommands();
#endif

	QCFunction *func = qvm.FindFunction(qce.ShutdownGame);
	qvm.Execute(func);

	ShutdownVM();

	gi.FreeTags (TAG_LEVEL);
	gi.FreeTags (TAG_GAME);
}

static void AssignClientPointers()
{
	for (size_t i = 0; i < game.num_clients; i++)
		itoe(i + 1)->client = &game.clients[i];
}

static void BackupClientData()
{
	// in Q2, gclient_t was stored in a separate pointer, but in Q2QC they're fields
	// and as such wiped with the entity structure. We have to mimic the original Q2 behavior of backing up
	// the gclient_t structures.
	game.client_load_data = (uint8_t *)gi.TagMalloc(globals.edict_size * game.num_clients, TAG_GAME);
	memcpy(game.client_load_data, itoe(1), globals.edict_size * game.num_clients);
	qvm.dynamic_strings.MarkIfHasRef(itoe(1), game.client_load_data, (globals.edict_size * game.num_clients) / sizeof(global_t));
}

static void RestoreClientData()
{
	// copy over any client-specific data back into the clients and re-sync
	for (size_t i = 0; i < game.num_clients; i++)
	{
		edict_t *ent = itoe(i + 1);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
		edict_t *backup = (edict_t *)(game.client_load_data + (globals.edict_size * i));
#pragma GCC diagnostic pop

		// restore client structs
		for (auto &def : qvm.fields)
		{
			const char *name = qvm.GetString(def.name_index);

			if (def.name_index == STRING_EMPTY || strnicmp(name, "client.", 6))
				continue;

			const size_t len = def.id == TYPE_VECTOR ? 3 : 1;

			void *dst = qvm.GetEntityFieldPointer(ent, def.global_index);
			void *src = qvm.GetEntityFieldPointer(backup, def.global_index);

			memcpy(dst, src, sizeof(global_t) * len);
		}

		SyncPlayerState(qvm, ent);
	}
	
	qvm.dynamic_strings.MarkIfHasRef(game.client_load_data, itoe(1), (globals.edict_size * game.num_clients) / sizeof(global_t));
	qvm.dynamic_strings.CheckRefUnset(game.client_load_data, (globals.edict_size * game.num_clients) / sizeof(global_t), true);
	gi.TagFree(game.client_load_data);
	game.client_load_data = nullptr;
}

static void WipeEntities()
{
	memset(globals.edicts, 0, globals.max_edicts * globals.edict_size);

	for (int32_t i = 0; i < globals.max_edicts; i++)
		itoe(i)->s.number = i;

	AssignClientPointers();
}

static void SpawnEntities(const char *mapname, const char *entities, const char *spawnpoint)
{
#ifdef ALLOW_DEBUGGING
	CheckDebuggerCommands();
#endif

	gi.FreeTags(TAG_LEVEL);

	QCFunction *func = qvm.FindFunction(qce.PreSpawnEntities);
	qvm.Execute(func);

	BackupClientData();

	WipeEntities();

	func = qvm.FindFunction(qce.SpawnEntities);
	qvm.SetGlobalStr(GLOBAL_PARM0, std::string(mapname));
	qvm.SetGlobalStr(GLOBAL_PARM1, std::string(entities));
	qvm.SetGlobalStr(GLOBAL_PARM2, std::string(spawnpoint));
	qvm.Execute(func);

	func = qvm.FindFunction(qce.PostSpawnEntities);
	qvm.Execute(func);

	RestoreClientData();
}

static qboolean ClientConnect(edict_t *e, char *userinfo)
{
#ifdef ALLOW_DEBUGGING
	CheckDebuggerCommands();
#endif

	QCFunction *func = qvm.FindFunction(qce.ClientConnect);
	qvm.SetGlobal(GLOBAL_PARM0, qvm.EntityToEnt(e));
	qvm.SetGlobalStr(GLOBAL_PARM1, std::string(userinfo));
	qvm.Execute(func);

	Q_strlcpy(userinfo, qvm.GetString(qvm.GetGlobal<string_t>(GLOBAL_PARM1)), MAX_INFO_STRING);

	return qvm.GetGlobal<qboolean>(GLOBAL_RETURN);
}

static void ClientBegin(edict_t *e)
{
#ifdef ALLOW_DEBUGGING
	CheckDebuggerCommands();
#endif

	QCFunction *func = qvm.FindFunction(qce.ClientBegin);
	qvm.SetGlobal(GLOBAL_PARM0, qvm.EntityToEnt(e));
	qvm.Execute(func);
	SyncPlayerState(qvm, e);
}

static void ClientUserinfoChanged(edict_t *e, char *userinfo)
{
#ifdef ALLOW_DEBUGGING
	CheckDebuggerCommands();
#endif

	QCFunction *func = qvm.FindFunction(qce.ClientUserinfoChanged);
	qvm.SetGlobal(GLOBAL_PARM0, qvm.EntityToEnt(e));
	qvm.SetGlobalStr(GLOBAL_PARM1, std::string(userinfo));
	qvm.Execute(func);
	SyncPlayerState(qvm, e);
}

static void ClientDisconnect(edict_t *e)
{
#ifdef ALLOW_DEBUGGING
	CheckDebuggerCommands();
#endif

	QCFunction *func = qvm.FindFunction(qce.ClientDisconnect);
	qvm.SetGlobal(GLOBAL_PARM0, qvm.EntityToEnt(e));
	qvm.Execute(func);
	SyncPlayerState(qvm, e);
}

static void ClientCommand(edict_t *e)
{
#ifdef ALLOW_DEBUGGING
	CheckDebuggerCommands();
#endif

	QCFunction *func = qvm.FindFunction(qce.ClientCommand);
	qvm.SetGlobal(GLOBAL_PARM0, qvm.EntityToEnt(e));
	qvm.Execute(func);
	SyncPlayerState(qvm, e);
}

static void ClientThink(edict_t *e, usercmd_t *ucmd)
{
#ifdef ALLOW_DEBUGGING
	CheckDebuggerCommands();
#endif

	QCFunction *func = qvm.FindFunction(qce.ClientThink);
	qvm.SetGlobal(GLOBAL_PARM0, qvm.EntityToEnt(e));

	QC_usercmd_t cmd = {
		ucmd->msec,
		ucmd->buttons,
		{ ucmd->angles[0] * short2angle, ucmd->angles[1] * short2angle, ucmd->angles[2] * short2angle },
		ucmd->forwardmove,
		ucmd->sidemove,
		ucmd->upmove,
		ucmd->impulse,
		ucmd->lightlevel
	};

	qvm.SetGlobal(GLOBAL_PARM1, cmd);

	qvm.Execute(func);
	SyncPlayerState(qvm, e);
}

static void RunFrame()
{
#ifdef ALLOW_DEBUGGING
	CheckDebuggerCommands();
#endif

	QCFunction *func = qvm.FindFunction(qce.RunFrame);
	qvm.Execute(func);

	for (size_t i = 0; i < game.num_clients; i++)
		SyncPlayerState(qvm, itoe(1 + i));
}

static void ServerCommand()
{
#ifdef ALLOW_DEBUGGING
	CheckDebuggerCommands();
#endif

	QCFunction *func = qvm.FindFunction(qce.ServerCommand);
	qvm.Execute(func);
}


//=========================================================

const uint32_t	SAVE_MAGIC1		= (('V'<<24)|('S'<<16)|('C'<<8)|'Q');	// "QCSV"
const uint32_t	SAVE_MAGIC2		= (('A'<<24)|('S'<<16)|('C'<<8)|'Q');	// "QCSA"
const uint32_t	SAVE_VERSION	= 666;

enum pointer_class_type_t : uint8_t
{
	POINTER_NONE,
	POINTER_GLOBAL,
	POINTER_ENT
};

static void WriteDefinitionData(FILE *fp, const QCDefinition *def, const global_t *value)
{
	const deftype_t type = def->id & ~TYPE_GLOBAL;

	if (type == TYPE_STRING)
	{
		string_t strid = (string_t)*value;
		size_t strlength = qvm.StringLength(strid);
		const char *str = qvm.GetString(strid);
		fwrite(&strlength, sizeof(strlength), 1, fp);
		fwrite(str, sizeof(char), strlength, fp);
		return;
	}
	else if (type == TYPE_FUNCTION)
	{
		func_t func = (func_t)*value;
		QCFunction *func_ptr = &qvm.functions[(size_t)func];
		const char *str = qvm.GetString(func_ptr->name_index);
		auto strlength = qvm.StringLength(func_ptr->name_index);
		
		fwrite(&strlength, sizeof(strlength), 1, fp);
		fwrite(str, sizeof(char), strlength, fp);
		return;
	}
	else if (type == TYPE_POINTER)
	{
		pointer_class_type_t classtype = POINTER_NONE;

		if (*value == GLOBAL_NULL)
		{
			fwrite(&classtype, sizeof(classtype), 1, fp);
			return;
		}

		ptrdiff_t ptr_val = (ptrdiff_t)*value;
		
		if (ptr_val >= (ptrdiff_t)qvm.global_data && ptr_val < (ptrdiff_t)(qvm.global_data + qvm.global_size))
		{
			classtype = POINTER_GLOBAL;
			fwrite(&classtype, sizeof(classtype), 1, fp);

			// find closest def
			global_t def_id = (global_t)((ptr_val - (ptrdiff_t)qvm.global_data) / sizeof(global_t));
			size_t offset = 0;

			while (def_id >= GLOBAL_QC)
			{
				if (qvm.definition_map_by_id.contains(def_id))
					break;

				def_id = (global_t)((int32_t)(def_id) - 1);
				offset++;
			}

			if (def_id < GLOBAL_QC)
				qvm.Error("couldn't find ptr reference");

			QCDefinition *closest_def = qvm.definition_map_by_id.at(def_id);
			const char *str = qvm.GetString(closest_def->name_index);
			const size_t len = qvm.StringLength(closest_def->name_index);

			fwrite(&len, sizeof(len), 1, fp);
			fwrite(str, sizeof(char), strlen(str), fp);
			fwrite(&offset, sizeof(offset), 1, fp);
		}
		else if (ptr_val >= (ptrdiff_t)globals.edicts && ptr_val < (ptrdiff_t)globals.edicts + (globals.edict_size * globals.max_edicts))
		{
			classtype = POINTER_ENT;
			qvm.Error("ent field ptrs not supported at the moment");
			//fwrite
		}
		else
			qvm.Error("somehow got a bad field ptr");
		
		return;
	}
	else if (type == TYPE_ENTITY)
	{
		edict_t *ent = qvm.EntToEntity((ent_t)*value);

		if (ent == nullptr)
			fwrite(&globals.max_edicts, sizeof(globals.max_edicts), 1, fp);
		else
			fwrite(&ent->s.number, sizeof(ent->s.number), 1, fp);

		return;
	}
	
	const size_t len = (type == TYPE_VECTOR) ? 3 : 1;
	fwrite(value, sizeof(global_t), len, fp);
}

static void WriteEntityFieldData(FILE *fp, edict_t *ent, const QCDefinition *def)
{
	int32_t *field = qvm.GetEntityFieldPointer(ent, (int32_t)def->global_index);
	WriteDefinitionData(fp, def, (global_t *)field);
}

static void ReadDefinitionData(FILE *fp, const QCDefinition *def, global_t *value)
{
	const deftype_t type = def->id & ~TYPE_GLOBAL;

	if (type == TYPE_STRING)
	{
		std::string def_value;
		size_t def_len;
		fread(&def_len, sizeof(def_len), 1, fp);
		def_value.resize(def_len);
		fread(def_value.data(), sizeof(char), def_len, fp);
		qvm.SetStringPtr(value, std::move(def_value));
		return;
	}
	else if (type == TYPE_FUNCTION)
	{
		std::string func_name;
		size_t func_len;
		fread(&func_len, sizeof(func_len), 1, fp);

		if (!func_len)
			*value = GLOBAL_NULL;
		else
		{
			func_name.resize(func_len);
			fread(func_name.data(), sizeof(char), func_len, fp);

			*value = (global_t)qvm.FindFunctionID(func_name.data());

			if (*value == GLOBAL_NULL)
				qvm.Error("can't find func %s", func_name.data());
		}
		return;
	}
	else if (type == TYPE_POINTER)
	{
		pointer_class_type_t ptrclass;
		fread(&ptrclass, sizeof(ptrclass), 1, fp);

		if (ptrclass == POINTER_NONE)
		{
			*value = GLOBAL_NULL;
			return;
		}
		else if (ptrclass == POINTER_GLOBAL)
		{
			std::string global_name;
			size_t global_len;
			fread(&global_len, sizeof(global_len), 1, fp);
			global_name.resize(global_len);
			fread(global_name.data(), sizeof(char), global_len, fp);

			if (!qvm.definition_map_by_name.contains(global_name))
				qvm.Error("bad pointer; can't map %s", global_name.data());

			auto global_def = qvm.definition_map_by_name.at(global_name);
			size_t global_offset;
			fread(&global_offset, sizeof(global_offset), 1, fp);

			auto ptr = qvm.GetGlobalByIndex((global_t)((uint32_t)global_def->global_index + global_offset));
			*value = (global_t)(ptrdiff_t)ptr;
			return;
		}
		else if (ptrclass == POINTER_ENT)
		{
			qvm.Error("ent pointers not supported");
		}
		
		qvm.Error("bad pointer");
	}
	else if (type == TYPE_ENTITY)
	{
		int32_t number;
		fread(&number, sizeof(number), 1, fp);

		if (number == globals.max_edicts)
			*value = (global_t)ENT_INVALID;
		else
			*value = (global_t)qvm.EntityToEnt(itoe(number));

		return;
	}
	
	const size_t len = (type == TYPE_VECTOR) ? 3 : 1;
	fread(value, sizeof(global_t), len, fp);
}

static void ReadEntityFieldData(FILE *fp, edict_t *ent, const QCDefinition *def)
{
	int32_t *field = qvm.GetEntityFieldPointer(ent, (int32_t)def->global_index);
	ReadDefinitionData(fp, def, (global_t *)field);
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
	FILE *fp = fopen(filename, "wb");

	auto func = qvm.FindFunction(qce.PreWriteGame);
	qvm.SetGlobal(GLOBAL_PARM0, autosave);
	qvm.Execute(func);
	
	fwrite(&SAVE_MAGIC1, sizeof(SAVE_MAGIC1), 1, fp);
	fwrite(&SAVE_VERSION, sizeof(SAVE_VERSION), 1, fp);
	fwrite(&globals.edict_size, sizeof(globals.edict_size), 1, fp);
	fwrite(&game.num_clients, sizeof(game.num_clients), 1, fp);

	// save "game." values
	size_t name_len;

	for (auto &def : qvm.definitions)
	{
		if (!(def.id & TYPE_GLOBAL))
			continue;

		const char *name = qvm.GetString(def.name_index);

		if (strnicmp(name, "game.", 5))
			continue;

		name_len = strlen(name);
		fwrite(&name_len, sizeof(name_len), 1, fp);
		fwrite(name, sizeof(char), name_len, fp);

		WriteDefinitionData(fp, &def, qvm.GetGlobalByIndex(def.global_index));
	}

	name_len = 0;
	fwrite(&name_len, sizeof(name_len), 1, fp);

	// save client fields
	for (auto &def : qvm.fields)
	{
		const char *name = qvm.GetString(def.name_index);

		if (strnicmp(name, "client.", 6))
			continue;

		name_len = strlen(name);
		fwrite(&name_len, sizeof(name_len), 1, fp);
		fwrite(name, sizeof(char), name_len, fp);

		for (size_t i = 0; i < game.num_clients; i++)
			WriteEntityFieldData(fp, itoe(i + 1), &def);
	}
	
	name_len = 0;
	fwrite(&name_len, sizeof(name_len), 1, fp);
	
	func = qvm.FindFunction(qce.PostWriteGame);
	qvm.SetGlobal(GLOBAL_PARM0, autosave);
	qvm.Execute(func);

	fclose(fp);
}

static void ReadGame(const char *filename)
{
	FILE *fp = fopen(filename, "rb");

	auto func = qvm.FindFunction(qce.PreReadGame);
	qvm.Execute(func);

	uint32_t magic, version, edict_size, maxclients;

	fread(&magic, sizeof(magic), 1, fp);

	if (magic != SAVE_MAGIC1)
		qvm.Error("Not a save game");
	
	fread(&version, sizeof(version), 1, fp);

	if (version != SAVE_VERSION)
		qvm.Error("Savegame from different version (got %d, expected %d)", version, SAVE_VERSION);
	
	fread(&edict_size, sizeof(edict_size), 1, fp);

	if (globals.edict_size != edict_size)
		qvm.Error("Savegame has bad fields (%i vs %i)", globals.edict_size, edict_size);

	fread(&maxclients, sizeof(maxclients), 1, fp);

	// should agree with server's version
	if (game.num_clients != maxclients)
		qvm.Error("Savegame has bad maxclients");

	// setup entities
	WipeEntities();
	
	// free any string refs inside of the entity structure
	qvm.dynamic_strings.CheckRefUnset(globals.edicts, (globals.edict_size * globals.max_edicts) / sizeof(global_t));

	// load game globals
	std::string def_name;
	std::string def_value;

	size_t len;

	while (true)
	{
		fread(&len, sizeof(len), 1, fp);

		if (!len)
			break;

		def_name.resize(len);

		fread(def_name.data(), sizeof(char), len, fp);

		if (!qvm.definition_map_by_name.contains(def_name))
			qvm.Error("Bad definition %s", def_name.data());

		QCDefinition *def = qvm.definition_map_by_name.at(def_name);
		ReadDefinitionData(fp, def, qvm.GetGlobalByIndex(def->global_index));
	}

	// load client fields
	while (true)
	{
		fread(&len, sizeof(len), 1, fp);

		if (!len)
			break;

		def_name.resize(len);

		fread(def_name.data(), sizeof(char), len, fp);

		string_t str;
		
		if (!qvm.FindString(def_name, str) || qvm.dynamic_strings.IsRefCounted(str))
			qvm.Error("Bad string in save file");

		if (!qvm.field_map_by_name.contains(def_name))
			qvm.Error("Bad field %s", def_name.data());
		
		QCDefinition *field = qvm.field_map_by_name.at(def_name);
		
		for (size_t i = 0; i < game.num_clients; i++)
			ReadEntityFieldData(fp, itoe(i + 1), field);
	}

	func = qvm.FindFunction(qce.PostReadGame);
	qvm.Execute(func);

	for (size_t i = 0; i < game.num_clients; i++)
		SyncPlayerState(qvm, itoe(i + 1));

	fclose(fp);
}

//==========================================================


/*
=================
WriteLevel

=================
*/
static void WriteLevel(const char *filename)
{
	FILE *fp = fopen(filename, "wb");

	auto func = qvm.FindFunction(qce.PreWriteLevel);
	qvm.Execute(func);
	
	fwrite(&SAVE_MAGIC2, sizeof(SAVE_MAGIC2), 1, fp);
	fwrite(&SAVE_VERSION, sizeof(SAVE_VERSION), 1, fp);
	fwrite(&globals.edict_size, sizeof(globals.edict_size), 1, fp);
	fwrite(&game.num_clients, sizeof(game.num_clients), 1, fp);

	// save "level." values
	size_t name_len;

	for (auto &def : qvm.definitions)
	{
		if (!(def.id & TYPE_GLOBAL))
			continue;

		const char *name = qvm.GetString(def.name_index);

		if (strnicmp(name, "level.", 5))
			continue;

		name_len = strlen(name);
		fwrite(&name_len, sizeof(name_len), 1, fp);
		fwrite(name, sizeof(char), name_len, fp);

		WriteDefinitionData(fp, &def, qvm.GetGlobalByIndex(def.global_index));
	}

	name_len = 0;
	fwrite(&name_len, sizeof(name_len), 1, fp);

	// save non-client structs
	for (auto &def : qvm.fields)
	{
		const char *name = qvm.GetString(def.name_index);

		if (def.name_index == STRING_EMPTY || strnicmp(name, "client.", 6) == 0)
			continue;

		name_len = strlen(name);
		fwrite(&name_len, sizeof(name_len), 1, fp);
		fwrite(name, sizeof(char), name_len, fp);

		for (size_t i = 0; i < globals.num_edicts; i++)
		{
			auto ent = itoe(i);

			if (!ent->inuse)
				continue;

			fwrite(&i, sizeof(i), 1, fp);
			WriteEntityFieldData(fp, ent, &def);
		}

		size_t i = -1;
		fwrite(&i, sizeof(i), 1, fp);
	}

	name_len = 0;
	fwrite(&name_len, sizeof(name_len), 1, fp);
	
	func = qvm.FindFunction(qce.PostWriteLevel);
	qvm.Execute(func);

	fclose(fp);
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
	FILE *fp = fopen(filename, "rb");

	auto func = qvm.FindFunction(qce.PreReadLevel);
	qvm.Execute(func);

	uint32_t magic, version, edict_size, maxclients;

	fread(&magic, sizeof(magic), 1, fp);

	if (magic != SAVE_MAGIC2)
		qvm.Error("Not a save game");

	fread(&version, sizeof(version), 1, fp);

	if (version != SAVE_VERSION)
		qvm.Error("Savegame from different version (got %d, expected %d)", version, SAVE_VERSION);

	fread(&edict_size, sizeof(edict_size), 1, fp);

	if (globals.edict_size != edict_size)
		qvm.Error("Savegame has bad fields");

	fread(&maxclients, sizeof(maxclients), 1, fp);

	// should agree with server's version
	if (game.num_clients != maxclients)
		qvm.Error("Savegame has bad maxclients");

	// setup entities
	BackupClientData();

	WipeEntities();

	globals.num_edicts = game.num_clients + 1;
	
	// free any string refs inside of the entity structure
	qvm.dynamic_strings.CheckRefUnset(globals.edicts, (globals.edict_size * globals.max_edicts) / sizeof(global_t));

	// load level globals
	std::string def_name;
	std::string def_value;

	while (true)
	{
		size_t len;
		fread(&len, sizeof(len), 1, fp);

		if (!len)
			break;

		def_name.resize(len);

		fread(def_name.data(), sizeof(char), len, fp);

		if (!qvm.definition_map_by_name.contains(def_name))
			qvm.Error("Bad definition %s", def_name.data());

		QCDefinition *def = qvm.definition_map_by_name.at(def_name);
		ReadDefinitionData(fp, def, qvm.GetGlobalByIndex(def->global_index));
	}

	// load entity fields
	while (true)
	{
		size_t len;
		fread(&len, sizeof(len), 1, fp);

		if (!len)
			break;

		def_name.resize(len);

		fread(def_name.data(), sizeof(char), len, fp);

		string_t str;
		
		if (!qvm.FindString(def_name, str) || qvm.dynamic_strings.IsRefCounted(str))
			qvm.Error("Bad string in save file");

		if (!qvm.field_map_by_name.contains(def_name))
			qvm.Error("Bad field %s", def_name.data());
		
		QCDefinition *field = qvm.field_map_by_name.at(def_name);

		while (true)
		{
			size_t ent_id;
			fread(&ent_id, sizeof(ent_id), 1, fp);

			if (ent_id == -1u)
				break;

			if (ent_id >= globals.max_edicts)
				qvm.Error("%s: bad entity number", __func__);

			if (ent_id >= globals.num_edicts)
				globals.num_edicts = ent_id + 1;

			edict_t *ent = itoe(ent_id);
			ReadEntityFieldData(fp, ent, field);
		}
	}

	AssignClientPointers();

	RestoreClientData();

	for (size_t i = 0; i < globals.num_edicts; i++)
	{
		edict_t *ent = itoe(i);

		// let the server rebuild world links for this ent
		ent->area = {};
		gi.linkentity(ent);
	}
	
	func = qvm.FindFunction(qce.PostReadLevel);
	qvm.SetGlobal(GLOBAL_PARM0, globals.num_edicts);
	qvm.Execute(func);

	fclose(fp);
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
