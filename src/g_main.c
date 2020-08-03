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

#ifdef ALLOW_DEBUGGING
#include "g_thread.h"
#endif

static qcvm_t *qvm;

static void InitBuiltins()
{
	qcvm_init_gi_builtins(qvm);
	qcvm_init_game_builtins(qvm);
	qcvm_init_ext_builtins(qvm);
	qcvm_init_string_builtins(qvm);
	qcvm_init_mem_builtins(qvm);
	qcvm_init_debug_builtins(qvm);
	qcvm_init_math_builtins(qvm);
}

static void FieldCoord2Short(void *out, const void *in)
{
	*(short *)(out) = (*(const vec_t *)(in) * coord2short);
}

static void FieldCoord2Angle(void *out, const void *in)
{
	*(short *)(out) = (*(const vec_t *)(in) * angle2short);
}

#define qcvm_field_wrap_to_type(name, T) \
static void name(void *out, const void *in) \
{ \
	*(T *)(out) = *(const int32_t *)(in); \
}

qcvm_field_wrap_to_type(qcvm_field_wrap_to_int16, int16_t)
qcvm_field_wrap_to_type(qcvm_field_wrap_to_uint8, uint8_t)

static void InitFieldWraps()
{
#define RegisterSingle(name) \
	qcvm_field_wrap_list_register(&qvm->field_wraps, "client." #name, 0, offsetof(gclient_t, name), NULL)

#define RegisterSingleWrapped(name, wrap) \
	qcvm_field_wrap_list_register(&qvm->field_wraps, "client." #name, 0, offsetof(gclient_t, name), wrap)

#define RegisterArray(name, fofs, sofs) \
	qcvm_field_wrap_list_register(&qvm->field_wraps, "client." #name "[" #fofs "]", 0, offsetof(gclient_t, name) + sofs, NULL)

#define RegisterVector(name) \
	qcvm_field_wrap_list_register(&qvm->field_wraps, "client." #name, 0, offsetof(gclient_t, name), NULL); \
	qcvm_field_wrap_list_register(&qvm->field_wraps, "client." #name, 1, offsetof(gclient_t, name) + 4, NULL); \
	qcvm_field_wrap_list_register(&qvm->field_wraps, "client." #name, 2, offsetof(gclient_t, name) + 8, NULL)

#define RegisterVectorCoord2Short(name) \
	qcvm_field_wrap_list_register(&qvm->field_wraps, "client." #name, 0, offsetof(gclient_t, name), FieldCoord2Short); \
	qcvm_field_wrap_list_register(&qvm->field_wraps, "client." #name, 1, offsetof(gclient_t, name) + 2, FieldCoord2Short); \
	qcvm_field_wrap_list_register(&qvm->field_wraps, "client." #name, 2, offsetof(gclient_t, name) + 4, FieldCoord2Short)

#define RegisterVectorCoord2Angle(name) \
	qcvm_field_wrap_list_register(&qvm->field_wraps, "client." #name, 0, offsetof(gclient_t, name), FieldCoord2Angle); \
	qcvm_field_wrap_list_register(&qvm->field_wraps, "client." #name, 1, offsetof(gclient_t, name) + 2, FieldCoord2Angle); \
	qcvm_field_wrap_list_register(&qvm->field_wraps, "client." #name, 2, offsetof(gclient_t, name) + 4, FieldCoord2Angle)

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

	for (int32_t i = 0; i < MAX_STATS; i++)
		qcvm_field_wrap_list_register(&qvm->field_wraps, qcvm_temp_format(qvm, "client.ps.stats[%i]", i), 0, offsetof(gclient_t, ps.stats) + (sizeof(player_stat_t) * i), qcvm_field_wrap_to_int16);
	
	// gclient_t::ps::pmove
	RegisterSingle(ps.pmove.pm_type);
	
	RegisterVectorCoord2Short(ps.pmove.origin);
	RegisterVectorCoord2Short(ps.pmove.velocity);
	
	RegisterSingleWrapped(ps.pmove.pm_flags, qcvm_field_wrap_to_uint8);
	RegisterSingleWrapped(ps.pmove.pm_time, qcvm_field_wrap_to_uint8);
	RegisterSingleWrapped(ps.pmove.gravity, qcvm_field_wrap_to_int16);

	RegisterVectorCoord2Angle(ps.pmove.delta_angles);
}

// exported from QC
typedef struct
{
	int32_t		apiversion;
	int32_t		clientsize;
	
	qcvm_func_t		InitGame;
	qcvm_func_t		ShutdownGame;
	
	qcvm_func_t		PreSpawnEntities;
	qcvm_func_t		SpawnEntities;
	qcvm_func_t		PostSpawnEntities;
	
	qcvm_func_t		ClientConnect;
	qcvm_func_t		ClientBegin;
	qcvm_func_t		ClientUserinfoChanged;
	qcvm_func_t		ClientDisconnect;
	qcvm_func_t		ClientCommand;
	qcvm_func_t		ClientThink;

	qcvm_func_t		RunFrame;

	qcvm_func_t		ServerCommand;
	
	qcvm_func_t		PreWriteGame;
	qcvm_func_t		PostWriteGame;
	
	qcvm_func_t		PreReadGame;
	qcvm_func_t		PostReadGame;

	qcvm_func_t		PreWriteLevel;
	qcvm_func_t		PostWriteLevel;
	
	qcvm_func_t		PreReadLevel;
	qcvm_func_t		PostReadLevel;
} qc_export_t;

static qc_export_t qce;

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
	qvm = (qcvm_t *)gi.TagMalloc(sizeof(qcvm_t), TAG_GAME);

	cvar_t *game_var = gi.cvar("game", "", 0);

	qcvm_load(qvm, "Quake II DLL Wrapper", qcvm_temp_format(qvm, "%s/progs.dat", game_var->string));

#ifdef ALLOW_PROFILING
	qvm->profile_flags = (int32_t)gi.cvar("qc_profile_flags", "0", CVAR_LATCH)->value;
#endif

	InitBuiltins();
	InitFieldWraps();

	qcvm_check(qvm);

#ifdef ALLOW_DEBUGGING
	qvm->debug.create_mutex = qcvm_cpp_create_mutex;
	qvm->debug.free_mutex = qcvm_cpp_free_mutex;
	qvm->debug.lock_mutex = qcvm_cpp_lock_mutex;
	qvm->debug.unlock_mutex = qcvm_cpp_unlock_mutex;
	qvm->debug.create_thread = qcvm_cpp_create_thread;
	qvm->debug.thread_sleep = qcvm_cpp_thread_sleep;

	qcvm_check_debugger_commands(qvm);
#endif

	// Call GetGameAPI
	qcvm_function_t *func = qcvm_find_function(qvm, "GetGameAPI");
	qcvm_execute(qvm, func);
	qce = *qcvm_get_global_typed(qc_export_t, qvm, GLOBAL_PARM0);

	// initialize all clients for this game
	const cvar_t *maxclients = gi.cvar ("maxclients", "4", CVAR_SERVERINFO | CVAR_LATCH);
	game.num_clients = minsz(MAX_CLIENTS, (size_t)maxclients->value);
	game.clients = (gclient_t *)gi.TagMalloc(sizeof(gclient_t) * game.num_clients, TAG_GAME);

	size_t entity_data_size = 0;

	for (qcvm_definition_t *field = qvm->fields; field < qvm->fields + qvm->fields_size; field++)
		entity_data_size = maxsz(entity_data_size, field->global_index * sizeof(int32_t));

	// initialize all entities for this game
	globals.max_edicts = MAX_EDICTS;
	globals.edict_size = sizeof(edict_t) + entity_data_size;
	globals.num_edicts = game.num_clients + 1;
	globals.edicts = (edict_t *)gi.TagMalloc((globals.max_edicts + 1) * globals.edict_size, TAG_GAME);

	func = qcvm_get_function(qvm, qce.InitGame);
	qcvm_execute(qvm, func);
}

static void ShutdownGame()
{
#ifdef ALLOW_DEBUGGING
	qcvm_check_debugger_commands(qvm);
#endif

	qcvm_function_t *func = qcvm_get_function(qvm, qce.ShutdownGame);
	qcvm_execute(qvm, func);

	qcvm_shutdown(qvm);

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
	qcvm_string_list_mark_if_has_ref(&qvm->dynamic_strings, itoe(1), game.client_load_data, (globals.edict_size * game.num_clients) / sizeof(qcvm_global_t));
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
		for (qcvm_definition_t *def = qvm->fields; def < qvm->fields + qvm->fields_size; def++)
		{
			const char *name = qcvm_get_string(qvm, def->name_index);

			if (def->name_index == STRING_EMPTY || strnicmp(name, "client.", 6))
				continue;

			const size_t len = def->id == TYPE_VECTOR ? 3 : 1;

			void *dst = qcvm_get_entity_field_pointer(ent, def->global_index);
			void *src = qcvm_get_entity_field_pointer(backup, def->global_index);

			memcpy(dst, src, sizeof(qcvm_global_t) * len);
		}

		SyncPlayerState(qvm, ent);
	}
	
	qcvm_string_list_mark_if_has_ref(&qvm->dynamic_strings, game.client_load_data, itoe(1), (globals.edict_size * game.num_clients) / sizeof(qcvm_global_t));
	qcvm_string_list_check_ref_unset(&qvm->dynamic_strings, game.client_load_data, (globals.edict_size * game.num_clients) / sizeof(qcvm_global_t), true);
	gi.TagFree(game.client_load_data);
	game.client_load_data = NULL;
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
	qcvm_check_debugger_commands(qvm);
#endif

	gi.FreeTags(TAG_LEVEL);

	qcvm_function_t *func = qcvm_get_function(qvm, qce.PreSpawnEntities);
	qcvm_execute(qvm, func);

	BackupClientData();

	WipeEntities();

	func = qcvm_get_function(qvm, qce.SpawnEntities);
	qcvm_set_global_str(qvm, GLOBAL_PARM0, mapname);
	qcvm_set_global_str(qvm, GLOBAL_PARM1, entities);
	qcvm_set_global_str(qvm, GLOBAL_PARM2, spawnpoint);
	qcvm_execute(qvm, func);

	func = qcvm_get_function(qvm, qce.PostSpawnEntities);
	qcvm_execute(qvm, func);

	RestoreClientData();
}

static qboolean ClientConnect(edict_t *e, char *userinfo)
{
#ifdef ALLOW_DEBUGGING
	qcvm_check_debugger_commands(qvm);
#endif

	qcvm_function_t *func = qcvm_get_function(qvm, qce.ClientConnect);
	const qcvm_ent_t ent = qcvm_entity_to_ent(e);
	qcvm_set_global_typed_value(qcvm_ent_t, qvm, GLOBAL_PARM0, ent);
	qcvm_set_global_str(qvm, GLOBAL_PARM1, userinfo);
	qcvm_execute(qvm, func);

	Q_strlcpy(userinfo, qcvm_get_string(qvm, *qcvm_get_global_typed(qcvm_string_t, qvm, GLOBAL_PARM1)), MAX_INFO_STRING);

	return *qcvm_get_global_typed(qboolean, qvm, GLOBAL_RETURN);
}

static void ClientBegin(edict_t *e)
{
#ifdef ALLOW_DEBUGGING
	qcvm_check_debugger_commands(qvm);
#endif

	qcvm_function_t *func = qcvm_get_function(qvm, qce.ClientBegin);
	const qcvm_ent_t ent = qcvm_entity_to_ent(e);
	qcvm_set_global_typed_value(qcvm_ent_t, qvm, GLOBAL_PARM0, ent);
	qcvm_execute(qvm, func);
	SyncPlayerState(qvm, e);
}

static void ClientUserinfoChanged(edict_t *e, char *userinfo)
{
#ifdef ALLOW_DEBUGGING
	qcvm_check_debugger_commands(qvm);
#endif

	qcvm_function_t *func = qcvm_get_function(qvm, qce.ClientUserinfoChanged);
	const qcvm_ent_t ent = qcvm_entity_to_ent(e);
	qcvm_set_global_typed_value(qcvm_ent_t, qvm, GLOBAL_PARM0, ent);
	qcvm_set_global_str(qvm, GLOBAL_PARM1, userinfo);
	qcvm_execute(qvm, func);
	SyncPlayerState(qvm, e);
}

static void ClientDisconnect(edict_t *e)
{
#ifdef ALLOW_DEBUGGING
	qcvm_check_debugger_commands(qvm);
#endif

	qcvm_function_t *func = qcvm_get_function(qvm, qce.ClientDisconnect);
	const qcvm_ent_t ent = qcvm_entity_to_ent(e);
	qcvm_set_global_typed_value(qcvm_ent_t, qvm, GLOBAL_PARM0, ent);
	qcvm_execute(qvm, func);
	SyncPlayerState(qvm, e);
}

static void ClientCommand(edict_t *e)
{
#ifdef ALLOW_DEBUGGING
	qcvm_check_debugger_commands(qvm);
#endif

	qcvm_function_t *func = qcvm_get_function(qvm, qce.ClientCommand);
	const qcvm_ent_t ent = qcvm_entity_to_ent(e);
	qcvm_set_global_typed_value(qcvm_ent_t, qvm, GLOBAL_PARM0, ent);
	qcvm_execute(qvm, func);
	SyncPlayerState(qvm, e);
}

static void ClientThink(edict_t *e, usercmd_t *ucmd)
{
#ifdef ALLOW_DEBUGGING
	qcvm_check_debugger_commands(qvm);
#endif

	qcvm_function_t *func = qcvm_get_function(qvm, qce.ClientThink);
	const qcvm_ent_t ent = qcvm_entity_to_ent(e);
	qcvm_set_global_typed_value(qcvm_ent_t, qvm, GLOBAL_PARM0, ent);

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

	qcvm_set_global_typed_value(QC_usercmd_t, qvm, GLOBAL_PARM1, cmd);

	qcvm_execute(qvm, func);
	SyncPlayerState(qvm, e);
}

static void RunFrame()
{
#ifdef ALLOW_DEBUGGING
	qcvm_check_debugger_commands(qvm);
#endif

	qcvm_function_t *func = qcvm_get_function(qvm, qce.RunFrame);
	qcvm_execute(qvm, func);

	for (size_t i = 0; i < game.num_clients; i++)
		SyncPlayerState(qvm, itoe(1 + i));
}

static void ServerCommand()
{
#ifdef ALLOW_DEBUGGING
	qcvm_check_debugger_commands(qvm);
#endif

	qcvm_function_t *func = qcvm_get_function(qvm, qce.ServerCommand);
	qcvm_execute(qvm, func);
}

//=========================================================

static const uint32_t	SAVE_MAGIC1		= (('V'<<24)|('S'<<16)|('C'<<8)|'Q');	// "QCSV"
static const uint32_t	SAVE_MAGIC2		= (('A'<<24)|('S'<<16)|('C'<<8)|'Q');	// "QCSA"
static const uint32_t	SAVE_VERSION	= 666;

enum
{
	POINTER_NONE,
	POINTER_GLOBAL,
	POINTER_ENT
};

typedef uint8_t pointer_class_type_t;

static void WriteDefinitionData(FILE *fp, const qcvm_definition_t *def, const qcvm_global_t *value)
{
	const qcvm_deftype_t type = def->id & ~TYPE_GLOBAL;

	if (type == TYPE_STRING)
	{
		const qcvm_string_t strid = (qcvm_string_t)*value;
		const size_t strlength = qcvm_get_string_length(qvm, strid);
		const char *str = qcvm_get_string(qvm, strid);
		fwrite(&strlength, sizeof(strlength), 1, fp);
		fwrite(str, sizeof(char), strlength, fp);
		return;
	}
	else if (type == TYPE_FUNCTION)
	{
		const qcvm_func_t func = (qcvm_func_t)*value;
		const qcvm_function_t *func_ptr = &qvm->functions[(size_t)func];
		const char *str = qcvm_get_string(qvm, func_ptr->name_index);
		const size_t strlength = qcvm_get_string_length(qvm, func_ptr->name_index);
		
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

		const ptrdiff_t ptr_val = (ptrdiff_t)*value;
		
		if (ptr_val >= (ptrdiff_t)qvm->global_data && ptr_val < (ptrdiff_t)(qvm->global_data + qvm->global_size))
		{
			classtype = POINTER_GLOBAL;
			fwrite(&classtype, sizeof(classtype), 1, fp);

			// find closest def
			qcvm_global_t def_id = (qcvm_global_t)((ptr_val - (ptrdiff_t)qvm->global_data) / sizeof(qcvm_global_t));
			size_t offset = 0;

			while (def_id >= GLOBAL_QC)
			{
				if (qvm->definition_map_by_id[def_id])
					break;

				def_id = (qcvm_global_t)((int32_t)(def_id) - 1);
				offset++;
			}

			if (def_id < GLOBAL_QC)
				qcvm_error(qvm, "couldn't find ptr reference");

			qcvm_definition_t *closest_def = qvm->definition_map_by_id[def_id];
			const char *str = qcvm_get_string(qvm, closest_def->name_index);
			const size_t len = qcvm_get_string_length(qvm, closest_def->name_index);

			fwrite(&len, sizeof(len), 1, fp);
			fwrite(str, sizeof(char), strlen(str), fp);
			fwrite(&offset, sizeof(offset), 1, fp);
		}
		else if (ptr_val >= (ptrdiff_t)globals.edicts && ptr_val < (ptrdiff_t)globals.edicts + (globals.edict_size * globals.max_edicts))
		{
			classtype = POINTER_ENT;
			qcvm_error(qvm, "ent field ptrs not supported at the moment");
			//fwrite
		}
		else
			qcvm_error(qvm, "somehow got a bad field ptr");
		
		return;
	}
	else if (type == TYPE_ENTITY)
	{
		const edict_t *ent = qcvm_ent_to_entity((qcvm_ent_t)*value, false);

		if (ent == NULL)
			fwrite(&globals.max_edicts, sizeof(globals.max_edicts), 1, fp);
		else
			fwrite(&ent->s.number, sizeof(ent->s.number), 1, fp);

		return;
	}
	
	const size_t len = (type == TYPE_VECTOR) ? 3 : 1;
	fwrite(value, sizeof(qcvm_global_t), len, fp);
}

static void WriteEntityFieldData(FILE *fp, edict_t *ent, const qcvm_definition_t *def)
{
	const int32_t *field = (const int32_t *)qcvm_get_entity_field_pointer(ent, (int32_t)def->global_index);
	WriteDefinitionData(fp, def, (const qcvm_global_t *)field);
}

static void ReadDefinitionData(qcvm_t *vm, FILE *fp, const qcvm_definition_t *def, qcvm_global_t *value)
{
	const qcvm_deftype_t type = def->id & ~TYPE_GLOBAL;

	if (type == TYPE_STRING)
	{
		size_t def_len;
		fread(&def_len, sizeof(def_len), 1, fp);
		char *def_value = qcvm_temp_buffer(vm, def_len);
		fread(def_value, sizeof(char), def_len, fp);
		def_value[def_len] = 0;
		qcvm_set_string_ptr(qvm, value, def_value);
		return;
	}
	else if (type == TYPE_FUNCTION)
	{
		size_t func_len;
		fread(&func_len, sizeof(func_len), 1, fp);

		if (!func_len)
			*value = GLOBAL_NULL;
		else
		{
			char *func_name = qcvm_temp_buffer(vm, func_len);
			fread(func_name, sizeof(char), func_len, fp);
			func_name[func_len] = 0;

			*value = (qcvm_global_t)qcvm_find_function_id(qvm, func_name);

			if (*value == GLOBAL_NULL)
				qcvm_error(qvm, "can't find func %s", func_name);
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
			size_t global_len;
			fread(&global_len, sizeof(global_len), 1, fp);
			char *global_name = qcvm_temp_buffer(vm, global_len);
			fread(global_name, sizeof(char), global_len, fp);
			global_name[global_len] = 0;

			qcvm_definition_hash_t *hashed = qvm->definition_hashes[Q_hash_string(global_name, qvm->definitions_size)];

			for (; hashed; hashed = hashed->hash_next)
				if (!strcmp(qcvm_get_string(vm, hashed->def->name_index), global_name))
					break;

			if (!hashed)
				qcvm_error(qvm, "bad pointer; can't map %s", global_name);

			qcvm_definition_t *global_def = hashed->def;
			size_t global_offset;
			fread(&global_offset, sizeof(global_offset), 1, fp);

			qcvm_global_t *ptr = qcvm_get_global(qvm, (qcvm_global_t)((uint32_t)global_def->global_index + global_offset));
			*value = (qcvm_global_t)(ptrdiff_t)ptr;
			return;
		}
		else if (ptrclass == POINTER_ENT)
		{
			qcvm_error(qvm, "ent pointers not supported");
		}
		
		qcvm_error(qvm, "bad pointer");
	}
	else if (type == TYPE_ENTITY)
	{
		int32_t number;
		fread(&number, sizeof(number), 1, fp);

		if (number == globals.max_edicts)
			*value = (qcvm_global_t)ENT_INVALID;
		else
			*value = (qcvm_global_t)qcvm_entity_to_ent(itoe(number));

		return;
	}
	
	const size_t len = (type == TYPE_VECTOR) ? 3 : 1;
	fread(value, sizeof(qcvm_global_t), len, fp);
}

static void ReadEntityFieldData(qcvm_t *vm, FILE *fp, edict_t *ent, const qcvm_definition_t *def)
{
	int32_t *field = (int32_t *)qcvm_get_entity_field_pointer(ent, (int32_t)def->global_index);
	ReadDefinitionData(vm, fp, def, (qcvm_global_t *)field);
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

	qcvm_function_t *func = qcvm_get_function(qvm, qce.PreWriteGame);
	qcvm_set_global_typed_value(qboolean, qvm, GLOBAL_PARM0, autosave);
	qcvm_execute(qvm, func);
	
	fwrite(&SAVE_MAGIC1, sizeof(SAVE_MAGIC1), 1, fp);
	fwrite(&SAVE_VERSION, sizeof(SAVE_VERSION), 1, fp);
	fwrite(&globals.edict_size, sizeof(globals.edict_size), 1, fp);
	fwrite(&game.num_clients, sizeof(game.num_clients), 1, fp);

	// save "game." values
	size_t name_len;

	for (qcvm_definition_t *def = qvm->definitions; def < qvm->definitions + qvm->definitions_size; def++)
	{
		if (!(def->id & TYPE_GLOBAL))
			continue;

		const char *name = qcvm_get_string(qvm, def->name_index);

		if (strnicmp(name, "game.", 5))
			continue;

		name_len = strlen(name);
		fwrite(&name_len, sizeof(name_len), 1, fp);
		fwrite(name, sizeof(char), name_len, fp);

		WriteDefinitionData(fp, def, qcvm_get_global(qvm, def->global_index));
	}

	name_len = 0;
	fwrite(&name_len, sizeof(name_len), 1, fp);

	// save client fields
	for (qcvm_definition_t *def = qvm->fields; def < qvm->fields + qvm->fields_size; def++)
	{
		const char *name = qcvm_get_string(qvm, def->name_index);

		if (strnicmp(name, "client.", 6))
			continue;

		name_len = strlen(name);
		fwrite(&name_len, sizeof(name_len), 1, fp);
		fwrite(name, sizeof(char), name_len, fp);

		for (size_t i = 0; i < game.num_clients; i++)
			WriteEntityFieldData(fp, itoe(i + 1), def);
	}
	
	name_len = 0;
	fwrite(&name_len, sizeof(name_len), 1, fp);
	
	func = qcvm_get_function(qvm, qce.PostWriteGame);
	qcvm_set_global_typed_value(qboolean, qvm, GLOBAL_PARM0, autosave);
	qcvm_execute(qvm, func);

	fclose(fp);
}

static void ReadGame(const char *filename)
{
	FILE *fp = fopen(filename, "rb");

	qcvm_function_t *func = qcvm_get_function(qvm, qce.PreReadGame);
	qcvm_execute(qvm, func);

	uint32_t magic, version, edict_size, maxclients;

	fread(&magic, sizeof(magic), 1, fp);

	if (magic != SAVE_MAGIC1)
		qcvm_error(qvm, "Not a save game");
	
	fread(&version, sizeof(version), 1, fp);

	if (version != SAVE_VERSION)
		qcvm_error(qvm, "Savegame from different version (got %d, expected %d)", version, SAVE_VERSION);
	
	fread(&edict_size, sizeof(edict_size), 1, fp);

	if (globals.edict_size != edict_size)
		qcvm_error(qvm, "Savegame has bad fields (%i vs %i)", globals.edict_size, edict_size);

	fread(&maxclients, sizeof(maxclients), 1, fp);

	// should agree with server's version
	if (game.num_clients != maxclients)
		qcvm_error(qvm, "Savegame has bad maxclients");

	// setup entities
	WipeEntities();
	
	// free any string refs inside of the entity structure
	qcvm_string_list_check_ref_unset(&qvm->dynamic_strings, globals.edicts, (globals.edict_size * globals.max_edicts) / sizeof(qcvm_global_t), false);

	// load game globals
	size_t len;

	while (true)
	{
		fread(&len, sizeof(len), 1, fp);

		if (!len)
			break;

		char *def_name = qcvm_temp_buffer(qvm, len);
		fread(def_name, sizeof(char), len, fp);
		def_name[len] = 0;

		qcvm_definition_hash_t *hashed = qvm->definition_hashes[Q_hash_string(def_name, qvm->definitions_size)];

		for (; hashed; hashed = hashed->hash_next)
			if (!strcmp(qcvm_get_string(qvm, hashed->def->name_index), def_name))
				break;

		if (!hashed)
			qcvm_error(qvm, "Bad definition %s", def_name);

		qcvm_definition_t *def = hashed->def;
		ReadDefinitionData(qvm, fp, def, qcvm_get_global(qvm, def->global_index));
	}

	// load client fields
	while (true)
	{
		fread(&len, sizeof(len), 1, fp);

		if (!len)
			break;
		
		char *def_name = qcvm_temp_buffer(qvm, len);
		fread(def_name, sizeof(char), len, fp);
		def_name[len] = 0;

		qcvm_string_t str;
		
		if (!qcvm_find_string(qvm, def_name, &str) || qcvm_string_list_is_ref_counted(&qvm->dynamic_strings, str))
			qcvm_error(qvm, "Bad string in save file");

		qcvm_definition_hash_t *hashed = qvm->field_hashes[Q_hash_string(def_name, qvm->fields_size)];

		for (; hashed; hashed = hashed->hash_next)
			if (!strcmp(qcvm_get_string(qvm, hashed->def->name_index), def_name))
				break;

		if (!hashed)
			qcvm_error(qvm, "Bad field %s", def_name);
		
		qcvm_definition_t *field = hashed->def;
		
		for (size_t i = 0; i < game.num_clients; i++)
			ReadEntityFieldData(qvm, fp, itoe(i + 1), field);
	}

	func = qcvm_get_function(qvm, qce.PostReadGame);
	qcvm_execute(qvm, func);

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

	qcvm_function_t *func = qcvm_get_function(qvm, qce.PreWriteLevel);
	qcvm_execute(qvm, func);
	
	fwrite(&SAVE_MAGIC2, sizeof(SAVE_MAGIC2), 1, fp);
	fwrite(&SAVE_VERSION, sizeof(SAVE_VERSION), 1, fp);
	fwrite(&globals.edict_size, sizeof(globals.edict_size), 1, fp);
	fwrite(&game.num_clients, sizeof(game.num_clients), 1, fp);

	// save "level." values
	size_t name_len;
	
	for (qcvm_definition_t *def = qvm->definitions; def < qvm->definitions + qvm->definitions_size; def++)
	{
		if (!(def->id & TYPE_GLOBAL))
			continue;

		const char *name = qcvm_get_string(qvm, def->name_index);

		if (strnicmp(name, "level.", 5))
			continue;

		name_len = strlen(name);
		fwrite(&name_len, sizeof(name_len), 1, fp);
		fwrite(name, sizeof(char), name_len, fp);

		WriteDefinitionData(fp, def, qcvm_get_global(qvm, def->global_index));
	}

	name_len = 0;
	fwrite(&name_len, sizeof(name_len), 1, fp);

	// save non-client structs
	for (qcvm_definition_t *def = qvm->fields; def < qvm->fields + qvm->fields_size; def++)
	{
		const char *name = qcvm_get_string(qvm, def->name_index);

		if (def->name_index == STRING_EMPTY || strnicmp(name, "client.", 6) == 0)
			continue;

		name_len = strlen(name);
		fwrite(&name_len, sizeof(name_len), 1, fp);
		fwrite(name, sizeof(char), name_len, fp);

		for (size_t i = 0; i < globals.num_edicts; i++)
		{
			edict_t *ent = itoe(i);

			if (!ent->inuse)
				continue;

			fwrite(&i, sizeof(i), 1, fp);
			WriteEntityFieldData(fp, ent, def);
		}

		size_t i = -1;
		fwrite(&i, sizeof(i), 1, fp);
	}

	name_len = 0;
	fwrite(&name_len, sizeof(name_len), 1, fp);
	
	func = qcvm_get_function(qvm, qce.PostWriteLevel);
	qcvm_execute(qvm, func);

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

	qcvm_function_t *func = qcvm_get_function(qvm, qce.PreReadLevel);
	qcvm_execute(qvm, func);

	uint32_t magic, version, edict_size, maxclients;

	fread(&magic, sizeof(magic), 1, fp);

	if (magic != SAVE_MAGIC2)
		qcvm_error(qvm, "Not a save game");

	fread(&version, sizeof(version), 1, fp);

	if (version != SAVE_VERSION)
		qcvm_error(qvm, "Savegame from different version (got %d, expected %d)", version, SAVE_VERSION);

	fread(&edict_size, sizeof(edict_size), 1, fp);

	if (globals.edict_size != edict_size)
		qcvm_error(qvm, "Savegame has bad fields");

	fread(&maxclients, sizeof(maxclients), 1, fp);

	// should agree with server's version
	if (game.num_clients != maxclients)
		qcvm_error(qvm, "Savegame has bad maxclients");

	// setup entities
	BackupClientData();

	WipeEntities();

	globals.num_edicts = game.num_clients + 1;
	
	// free any string refs inside of the entity structure
	qcvm_string_list_check_ref_unset(&qvm->dynamic_strings, globals.edicts, (globals.edict_size * globals.max_edicts) / sizeof(qcvm_global_t), false);

	// load level globals
	while (true)
	{
		size_t len;
		fread(&len, sizeof(len), 1, fp);

		if (!len)
			break;
		
		char *def_name = qcvm_temp_buffer(qvm, len);
		fread(def_name, sizeof(char), len, fp);
		def_name[len] = 0;

		qcvm_definition_hash_t *hashed = qvm->definition_hashes[Q_hash_string(def_name, qvm->definitions_size)];

		for (; hashed; hashed = hashed->hash_next)
			if (!strcmp(qcvm_get_string(qvm, hashed->def->name_index), def_name))
				break;

		if (!hashed)
			qcvm_error(qvm, "Bad definition %s", def_name);

		qcvm_definition_t *def = hashed->def;
		ReadDefinitionData(qvm, fp, def, qcvm_get_global(qvm, def->global_index));
	}

	// load entity fields
	while (true)
	{
		size_t len;
		fread(&len, sizeof(len), 1, fp);

		if (!len)
			break;
		
		char *def_name = qcvm_temp_buffer(qvm, len);
		fread(def_name, sizeof(char), len, fp);
		def_name[len] = 0;

		qcvm_string_t str;
		
		if (!qcvm_find_string(qvm, def_name, &str) || qcvm_string_list_is_ref_counted(&qvm->dynamic_strings, str))
			qcvm_error(qvm, "Bad string in save file");

		qcvm_definition_hash_t *hashed = qvm->field_hashes[Q_hash_string(def_name, qvm->fields_size)];

		for (; hashed; hashed = hashed->hash_next)
			if (!strcmp(qcvm_get_string(qvm, hashed->def->name_index), def_name))
				break;

		if (!hashed)
			qcvm_error(qvm, "Bad field %s", def_name);
		
		qcvm_definition_t *field = hashed->def;

		while (true)
		{
			size_t ent_id;
			fread(&ent_id, sizeof(ent_id), 1, fp);

			if (ent_id == -1u)
				break;

			if (ent_id >= globals.max_edicts)
				qcvm_error(qvm, "%s: bad entity number", __func__);

			if (ent_id >= globals.num_edicts)
				globals.num_edicts = ent_id + 1;

			edict_t *ent = itoe(ent_id);
			ReadEntityFieldData(qvm, fp, ent, field);
		}
	}

	AssignClientPointers();

	RestoreClientData();

	for (size_t i = 0; i < globals.num_edicts; i++)
	{
		edict_t *ent = itoe(i);

		// let the server rebuild world links for this ent
		ent->area = (list_t) { NULL, NULL };
		gi.linkentity(ent);
	}
	
	func = qcvm_get_function(qvm, qce.PostReadLevel);
	qcvm_set_global_typed_value(int32_t, qvm, GLOBAL_PARM0, globals.num_edicts);
	qcvm_execute(qvm, func);

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

game_t game;

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
