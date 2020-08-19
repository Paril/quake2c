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
#include "vm.h"
#include "vm_debug.h"
#include "vm_string.h"
#include "vm_gi.h"

#ifdef ALLOW_DEBUGGING
#include "g_thread.h"
#endif

#include "game.h"
#include "vm_game.h"

#include "g_main.h"
#include "g_save.h"

qcvm_t *qvm;

static void FieldCoord2Short(void *out, const void *in)
{
	*(int16_t *)(out) = (*(const vec_t *)(in) * coord2short);
}

#ifdef KMQUAKE2_ENGINE_MOD
static void FieldCoord2Int(void *out, const void *in)
{
	*(int32_t *)(out) = (*(const vec_t *)(in) * coord2short);
}
#endif

static void FieldCoord2Angle(void *out, const void *in)
{
	*(int16_t *)(out) = (*(const vec_t *)(in) * angle2short);
}

#define qcvm_field_wrap_to_type(name, T) \
static void name(void *out, const void *in) \
{ \
	*(T *)(out) = *(const int32_t *)(in); \
}

qcvm_field_wrap_to_type(qcvm_field_wrap_to_int16, int16_t)
qcvm_field_wrap_to_type(qcvm_field_wrap_to_uint8, uint8_t)

static void FieldEnt2Entity(void *out, const void *in)
{
	*(edict_t **)(out) = qcvm_ent_to_entity(qvm, (*(const qcvm_ent_t *)(in)), false);
}

static void InitFieldWraps()
{
#define RegisterSingle(field_name, strct, name) \
	qcvm_field_wrap_list_register(qvm, field_name, 0, offsetof(strct, name), NULL)

#define RegisterSingleWrapped(field_name, strct, name, wrap) \
	qcvm_field_wrap_list_register(qvm, field_name, 0, offsetof(strct, name), wrap)

#define RegisterArray(field_name, strct, name, fofs, sofs) \
	qcvm_field_wrap_list_register(qvm, field_name "[" #fofs "]", 0, offsetof(strct, name) + sofs, NULL)

#define RegisterVector(field_name, strct, name) \
	qcvm_field_wrap_list_register(qvm, field_name, 0, offsetof(strct, name), NULL); \
	qcvm_field_wrap_list_register(qvm, field_name, 1, offsetof(strct, name) + 4, NULL); \
	qcvm_field_wrap_list_register(qvm, field_name, 2, offsetof(strct, name) + 8, NULL)

#define RegisterVectorCoord2Short(field_name, strct, name) \
	qcvm_field_wrap_list_register(qvm, field_name, 0, offsetof(strct, name), FieldCoord2Short); \
	qcvm_field_wrap_list_register(qvm, field_name, 1, offsetof(strct, name) + 2, FieldCoord2Short); \
	qcvm_field_wrap_list_register(qvm, field_name, 2, offsetof(strct, name) + 4, FieldCoord2Short)
	
#ifdef KMQUAKE2_ENGINE_MOD
#define RegisterVectorCoord2Int(field_name, strct, name) \
	qcvm_field_wrap_list_register(qvm, field_name, 0, offsetof(strct, name), FieldCoord2Int); \
	qcvm_field_wrap_list_register(qvm, field_name, 1, offsetof(strct, name) + 4, FieldCoord2Int); \
	qcvm_field_wrap_list_register(qvm, field_name, 2, offsetof(strct, name) + 8, FieldCoord2Int)
#endif

#define RegisterVectorCoord2Angle(field_name, strct, name) \
	qcvm_field_wrap_list_register(qvm, field_name, 0, offsetof(strct, name), FieldCoord2Angle); \
	qcvm_field_wrap_list_register(qvm, field_name, 1, offsetof(strct, name) + 2, FieldCoord2Angle); \
	qcvm_field_wrap_list_register(qvm, field_name, 2, offsetof(strct, name) + 4, FieldCoord2Angle)

	// edict_t wraps
	RegisterSingleWrapped("owner", edict_t, owner, FieldEnt2Entity);

	// gclient_t wraps
	// gclient_t::ps
	RegisterVector("client.ps.viewangles", gclient_t, ps.viewangles);
	RegisterVector("client.ps.viewoffset", gclient_t, ps.viewoffset);
	RegisterVector("client.ps.kick_angles", gclient_t, ps.kick_angles);
	RegisterVector("client.ps.gunangles", gclient_t, ps.gunangles);
	RegisterVector("client.ps.gunoffset", gclient_t, ps.gunoffset);
	RegisterSingle("client.ps.gunindex", gclient_t, ps.gunindex);
	RegisterSingle("client.ps.gunframe", gclient_t, ps.gunframe);
	RegisterArray("client.ps.blend", gclient_t, ps.blend, 0, 0);
	RegisterArray("client.ps.blend", gclient_t, ps.blend, 1, 4);
	RegisterArray("client.ps.blend", gclient_t, ps.blend, 2, 8);
	RegisterArray("client.ps.blend", gclient_t, ps.blend, 3, 12);
	RegisterSingle("client.ps.fov", gclient_t, ps.fov);
	RegisterSingle("client.ps.rdflags", gclient_t, ps.rdflags);

	for (int32_t i = 0; i < MAX_STATS; i++)
		qcvm_field_wrap_list_register(qvm, qcvm_temp_format(qvm, "client.ps.stats[%i]", i), 0, offsetof(gclient_t, ps.stats) + (sizeof(player_stat_t) * i), qcvm_field_wrap_to_int16);
	
	// gclient_t::ps::pmove
	RegisterSingle("client.ps.pmove.pm_type", gclient_t, ps.pmove.pm_type);
	
#ifdef KMQUAKE2_ENGINE_MOD
	RegisterVectorCoord2Int("client.ps.pmove.origin", gclient_t, ps.pmove.origin);
#else
	RegisterVectorCoord2Short("client.ps.pmove.origin", gclient_t, ps.pmove.origin);
#endif
	RegisterVectorCoord2Short("client.ps.pmove.velocity", gclient_t, ps.pmove.velocity);
	
	RegisterSingleWrapped("client.ps.pmove.pm_flags", gclient_t, ps.pmove.pm_flags, qcvm_field_wrap_to_uint8);
	RegisterSingleWrapped("client.ps.pmove.pm_time", gclient_t, ps.pmove.pm_time, qcvm_field_wrap_to_uint8);
	RegisterSingleWrapped("client.ps.pmove.gravity", gclient_t, ps.pmove.gravity, qcvm_field_wrap_to_int16);

	RegisterVectorCoord2Angle("client.ps.pmove.delta_angles", gclient_t, ps.pmove.delta_angles);
}

qc_export_t qce;

static const char *GetProgsName(void)
{
	cvar_t *game_var = gi.cvar("game", "", 0);

#ifdef KMQUAKE2_ENGINE_MOD
	const char *kmq2_progs = qcvm_temp_format(qvm, "%s/kmq2progs.dat", game_var->string);

	if (access(kmq2_progs, F_OK) != -1)
		return kmq2_progs;
#endif

	return qcvm_temp_format(qvm, "%s/progs.dat", game_var->string);
}

#define sizeof_member(type, member) sizeof(((type *)0)->member)

// register system fields
static void InitFields(void)
{
#define RegisterField(name) \
	qcvm_register_system_field(qvm, #name, offsetof(edict_t, name) / sizeof(qcvm_global_t), sizeof_member(edict_t, name) / sizeof(qcvm_global_t))
	
	RegisterField(s.number);
	RegisterField(s.origin);
	RegisterField(s.angles);
	RegisterField(s.old_origin);
	RegisterField(s.modelindex);
	RegisterField(s.modelindex2);
	RegisterField(s.modelindex3);
	RegisterField(s.modelindex4);
	RegisterField(s.frame);
	RegisterField(s.skinnum);
	RegisterField(s.effects);
	RegisterField(s.renderfx);
	RegisterField(s.solid);
	RegisterField(s.sound);
	RegisterField(s.event);
	RegisterField(inuse);
	RegisterField(linkcount);
	RegisterField(areanum);
	RegisterField(areanum2);
	RegisterField(svflags);
	RegisterField(mins);
	RegisterField(maxs);
	RegisterField(absmin);
	RegisterField(absmax);
	RegisterField(size);
	RegisterField(solid);
	RegisterField(clipmask);
	
#ifdef KMQUAKE2_ENGINE_MOD
	RegisterField(s.modelindex5);
	RegisterField(s.modelindex6);
	RegisterField(s.alpha);
	RegisterField(s.attenuation);
#endif
}

#ifdef KMQUAKE2_ENGINE_MOD
static void InitKMQ2Constants(void)
{
#define SetConstant(name, value) \
	{ \
		qcvm_definition_t *def = qcvm_find_definition(qvm, name, TYPE_INTEGER); \
\
		if (def) \
		{ \
			const int32_t val = value; \
			qcvm_set_global_typed_value(vec_t, qvm, def->global_index, val); \
		} \
	}

	SetConstant("MAX_EDICTS", MAX_EDICTS);
	SetConstant("MAX_MODELS", 8192);
	SetConstant("MAX_SOUNDS", 8192);
	SetConstant("MAX_IMAGES", 2048);
}
#endif

void AssignClientPointer(edict_t *e, const bool assign)
{
	if (assign)
	{
		e->client = &game.clients[e->s.number - 1];
		((int32_t *)e)[game.fields.is_client] = true;
	}
	else
	{
		e->client = NULL;
		((int32_t *)e)[game.fields.is_client] = true;
	}
}

void WipeClientPointers(void)
{
	for (uint32_t i = 0; i < game.num_clients; i++)
		AssignClientPointer((edict_t *)qcvm_itoe(qvm, i + 1), false);
}

void WipeEntities(void)
{
	memset(globals.edicts, 0, globals.max_edicts * globals.edict_size);

	for (int32_t i = 0; i < globals.max_edicts; i++)
		((edict_t *)qcvm_itoe(qvm, i))->s.number = i;

	WipeClientPointers();
}

static __attribute__((noreturn)) void qvm_error(const char *str)
{
	gi.error("%s", str);
}

static void qvm_debug(const char *str)
{
	gi.dprintf("%s\n", str);
}

static void *qvm_alloc(const size_t sz)
{
	return gi.TagMalloc((uint32_t)sz, TAG_GAME);
}

static void InitGameField(qcvm_global_t *field_ptr, const char *name)
{
	const qcvm_definition_t *field = qcvm_find_field(qvm, name);

	if (!field)
		qcvm_error(qvm, "missing required field: \"is_client\"");

	*field_ptr = (qcvm_global_t)field->global_index;
}

static void InitGameFields(void)
{
	InitGameField(&game.fields.is_client, "is_client");
	InitGameField(&game.fields.owner, "owner");
	
	game.funcs.ClientConnect = qcvm_get_function(qvm, qce.ClientConnect);
	game.funcs.ClientBegin = qcvm_get_function(qvm, qce.ClientBegin);
	game.funcs.ClientUserinfoChanged = qcvm_get_function(qvm, qce.ClientUserinfoChanged);
	game.funcs.ClientDisconnect = qcvm_get_function(qvm, qce.ClientDisconnect);
	game.funcs.ClientCommand = qcvm_get_function(qvm, qce.ClientCommand);
	game.funcs.ClientThink = qcvm_get_function(qvm, qce.ClientThink);
	game.funcs.RunFrame = qcvm_get_function(qvm, qce.RunFrame);
	game.funcs.ServerCommand = qcvm_get_function(qvm, qce.ServerCommand);
}

#if defined(ALLOW_INSTRUMENTING) || defined(ALLOW_PROFILING)
#include <time.h>
#endif

/*
============
InitGame

This will be called when the dll is first loaded, which
only happens when a new game is started or a save game
is loaded.
============
*/
static void InitGame (void)
{
	qvm = (qcvm_t *)gi.TagMalloc(sizeof(qcvm_t), TAG_GAME);

	qvm->warning = gi.dprintf;
	qvm->error = qvm_error;
	qvm->debug_print = qvm_debug;
	qvm->alloc = qvm_alloc;
	qvm->free = gi.TagFree;
#if defined(ALLOW_INSTRUMENTING) || defined(ALLOW_PROFILING)
	qvm->profiler_mark = MARK_INIT;
#endif

	qvm->max_edicts = globals.max_edicts = MAX_EDICTS;
	qvm->system_edict_size = sizeof(edict_t);

#if defined(ALLOW_INSTRUMENTING) || defined(ALLOW_PROFILING)
	qvm->profile_flags = (int32_t)gi.cvar("qc_profile_flags", "0", CVAR_LATCH)->value;

	if (qvm->profile_flags & PROFILE_CONTINUOUS)
	{
		const cvar_t *qc_profile_name = gi.cvar("qc_profile_filename", qcvm_temp_format(qvm, "%u", time(NULL)), CVAR_NOSET);
		qvm->profile_name = qc_profile_name->string;
	}
#endif

#ifdef ALLOW_PROFILING
	qvm->sample_rate = (uint32_t)gi.cvar("qc_sample_rate", "32", CVAR_LATCH)->value;
	qvm->sample_id = qvm->sample_rate;
#endif

	qcvm_load(qvm, "Quake2C DLL", GetProgsName());

#ifdef KMQUAKE2_ENGINE_MOD
	// adjust constants in progs that KMQ2 change
	InitKMQ2Constants();
#endif

	qcvm_init_all_builtins(qvm);
	qcvm_init_gi_builtins(qvm);

	InitFields();

	qcvm_check(qvm);

#ifdef ALLOW_INSTRUMENTING
	const cvar_t *qc_profile_func = gi.cvar("qc_profile_func", "", CVAR_LATCH);

	if (*qc_profile_func->string)
		qvm->profiler_func = qcvm_find_function(qvm, qc_profile_func->string);
#endif

	InitFieldWraps();

#ifdef ALLOW_DEBUGGING
	qvm->debug.create_mutex = qcvm_cpp_create_mutex;
	qvm->debug.free_mutex = qcvm_cpp_free_mutex;
	qvm->debug.lock_mutex = qcvm_cpp_lock_mutex;
	qvm->debug.unlock_mutex = qcvm_cpp_unlock_mutex;
	qvm->debug.create_thread = qcvm_cpp_create_thread;
	qvm->debug.thread_sleep = qcvm_cpp_thread_sleep;

	if (gi.cvar("qc_debugger", "", CVAR_NONE)->value)
	{
		qcvm_init_debugger(qvm);
		qcvm_check_debugger_commands(qvm);
	}
#endif

	// Call GetGameAPI
	qcvm_function_t *func = qcvm_find_function(qvm, "GetGameAPI");
	qcvm_execute(qvm, func);
	qce = *qcvm_get_global_typed(qc_export_t, qvm, GLOBAL_PARM0);

	InitGameFields();

	// initialize all clients for this game
	const cvar_t *maxclients = gi.cvar ("maxclients", "4", CVAR_SERVERINFO | CVAR_LATCH);
	game.num_clients = (uint32_t)minsz(MAX_CLIENTS, (size_t)maxclients->value);
	game.clients = (gclient_t *)gi.TagMalloc(sizeof(gclient_t) * game.num_clients, TAG_GAME);

	// initialize all entities for this game
	qvm->edict_size = qvm->field_real_size * 4;
	globals.edict_size = (int32_t)qvm->edict_size;

	qcvm_debug(qvm, "Field size: %u bytes\n", globals.edict_size);

	globals.num_edicts = game.num_clients + 1;
	qvm->edicts = globals.edicts = (edict_t *)gi.TagMalloc(globals.max_edicts * globals.edict_size, TAG_GAME);

	WipeEntities();

	func = qcvm_get_function(qvm, qce.InitGame);
	qcvm_execute(qvm, func);
}

static void ShutdownGame(void)
{
#if defined(ALLOW_INSTRUMENTING) || defined(ALLOW_PROFILING)
	qvm->profiler_mark = MARK_SHUTDOWN;
#endif

#ifdef ALLOW_DEBUGGING
	qcvm_check_debugger_commands(qvm);
#endif

	qcvm_function_t *func = qcvm_get_function(qvm, qce.ShutdownGame);
	qcvm_execute(qvm, func);

	qcvm_shutdown(qvm);

	gi.FreeTags (TAG_LEVEL);
	gi.FreeTags (TAG_GAME);
}

void BackupClientData(void)
{
	// in Q2, gclient_t was stored in a separate pointer, but in Q2QC they're fields
	// and as such wiped with the entity structure. We have to mimic the original Q2 behavior of backing up
	// the gclient_t structures.
	edict_t *first_player = (edict_t *)qcvm_itoe(qvm, 1);

	game.client_load_data = (uint8_t *)gi.TagMalloc(globals.edict_size * game.num_clients, TAG_GAME);
	memcpy(game.client_load_data, first_player, globals.edict_size * game.num_clients);
	qcvm_string_list_mark_refs_copied(qvm, first_player, game.client_load_data, (globals.edict_size * game.num_clients) / sizeof(qcvm_global_t));
}

void RestoreClientData(void)
{
	// copy over any client-specific data back into the clients and re-sync
	for (uint32_t i = 0; i < game.num_clients; i++)
	{
		edict_t *ent = (edict_t *)qcvm_itoe(qvm, i + 1);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
		edict_t *backup = (edict_t *)((uint8_t *)game.client_load_data + (globals.edict_size * i));
#pragma GCC diagnostic pop

		AssignClientPointer(ent, true);

		// restore client structs
		for (qcvm_definition_t *def = qvm->fields; def < qvm->fields + qvm->fields_size; def++)
		{
			const char *name = qcvm_get_string(qvm, def->name_index);

			if (def->name_index == STRING_EMPTY || strnicmp(name, "client.", 6))
				continue;

			const uint32_t len = def->id == TYPE_VECTOR ? 3 : 1;

			void *dst = qcvm_resolve_pointer(qvm, qcvm_get_entity_field_pointer(qvm, ent, def->global_index));
			void *src = (int32_t *)backup + def->global_index;

			memcpy(dst, src, sizeof(qcvm_global_t) * len);
		}
	}
	
	qcvm_string_list_mark_refs_copied(qvm, game.client_load_data, qcvm_itoe(qvm, 1), (globals.edict_size * game.num_clients) / sizeof(qcvm_global_t));
	qcvm_field_wrap_list_check_set(qvm, qcvm_itoe(qvm, 1), (globals.edict_size * game.num_clients) / sizeof(qcvm_global_t));
	qcvm_string_list_check_ref_unset(qvm, game.client_load_data, (globals.edict_size * game.num_clients) / sizeof(qcvm_global_t), true);

	gi.TagFree(game.client_load_data);
	game.client_load_data = NULL;
}

static void SpawnEntities(const char *mapname, const char *entities, const char *spawnpoint)
{
#if defined(ALLOW_INSTRUMENTING) || defined(ALLOW_PROFILING)
	qvm->profiler_mark = MARK_SPAWNENTITIES;
#endif

#ifdef ALLOW_DEBUGGING
	qcvm_check_debugger_commands(qvm);
#endif

	gi.FreeTags(TAG_LEVEL);

	qcvm_function_t *func = qcvm_get_function(qvm, qce.PreSpawnEntities);
	qcvm_execute(qvm, func);

	BackupClientData();

	WipeEntities();

	func = qcvm_get_function(qvm, qce.SpawnEntities);
	qcvm_set_global_str(qvm, GLOBAL_PARM0, mapname, strlen(mapname), true);
	qcvm_set_global_str(qvm, GLOBAL_PARM1, entities, strlen(entities), true);
	qcvm_set_global_str(qvm, GLOBAL_PARM2, spawnpoint, strlen(spawnpoint), true);
	qcvm_execute(qvm, func);

	func = qcvm_get_function(qvm, qce.PostSpawnEntities);
	qcvm_execute(qvm, func);

	RestoreClientData();
}

static qboolean ClientConnect(edict_t *e, char *userinfo)
{
#if defined(ALLOW_INSTRUMENTING) || defined(ALLOW_PROFILING)
	qvm->profiler_mark = MARK_CLIENTCONNECT;
#endif

#ifdef ALLOW_DEBUGGING
	qcvm_check_debugger_commands(qvm);
#endif

	AssignClientPointer(e, true);

	const qcvm_ent_t ent = qcvm_entity_to_ent(qvm, e);
	qcvm_set_global_typed_value(qcvm_ent_t, qvm, GLOBAL_PARM0, ent);
	qcvm_set_global_str(qvm, GLOBAL_PARM1, userinfo, strlen(userinfo), true);
	qcvm_execute(qvm, game.funcs.ClientConnect);

	Q_strlcpy(userinfo, qcvm_get_string(qvm, *qcvm_get_global_typed(qcvm_string_t, qvm, GLOBAL_PARM1)), MAX_INFO_STRING);

	const qboolean succeed = *qcvm_get_global_typed(qboolean, qvm, GLOBAL_RETURN);

	if (!succeed)
		AssignClientPointer(e, false);

	return succeed;
}

static void ClientBegin(edict_t *e)
{
#if defined(ALLOW_INSTRUMENTING) || defined(ALLOW_PROFILING)
	qvm->profiler_mark = MARK_CLIENTBEGIN;
#endif

#ifdef ALLOW_DEBUGGING
	qcvm_check_debugger_commands(qvm);
#endif

	AssignClientPointer(e, true);

	const qcvm_ent_t ent = qcvm_entity_to_ent(qvm, e);
	qcvm_set_global_typed_value(qcvm_ent_t, qvm, GLOBAL_PARM0, ent);
	qcvm_execute(qvm, game.funcs.ClientBegin);
}

static void ClientUserinfoChanged(edict_t *e, char *userinfo)
{
#if defined(ALLOW_INSTRUMENTING) || defined(ALLOW_PROFILING)
	qvm->profiler_mark = MARK_CLIENTUSERINFOCHANGED;
#endif

#ifdef ALLOW_DEBUGGING
	qcvm_check_debugger_commands(qvm);
#endif

	const qcvm_ent_t ent = qcvm_entity_to_ent(qvm, e);
	qcvm_set_global_typed_value(qcvm_ent_t, qvm, GLOBAL_PARM0, ent);
	qcvm_set_global_str(qvm, GLOBAL_PARM1, userinfo, strlen(userinfo), true);
	qcvm_execute(qvm, game.funcs.ClientUserinfoChanged);
}

static void ClientDisconnect(edict_t *e)
{
#if defined(ALLOW_INSTRUMENTING) || defined(ALLOW_PROFILING)
	qvm->profiler_mark = MARK_CLIENTDISCONNECT;
#endif

#ifdef ALLOW_DEBUGGING
	qcvm_check_debugger_commands(qvm);
#endif

	const qcvm_ent_t ent = qcvm_entity_to_ent(qvm, e);
	qcvm_set_global_typed_value(qcvm_ent_t, qvm, GLOBAL_PARM0, ent);
	qcvm_execute(qvm, game.funcs.ClientDisconnect);
}

static void ClientCommand(edict_t *e)
{
#if defined(ALLOW_INSTRUMENTING) || defined(ALLOW_PROFILING)
	qvm->profiler_mark = MARK_CLIENTCOMMAND;
#endif

#ifdef ALLOW_DEBUGGING
	qcvm_check_debugger_commands(qvm);
#endif

	const qcvm_ent_t ent = qcvm_entity_to_ent(qvm, e);
	qcvm_set_global_typed_value(qcvm_ent_t, qvm, GLOBAL_PARM0, ent);
	qcvm_execute(qvm, game.funcs.ClientCommand);
}

static void ClientThink(edict_t *e, usercmd_t *ucmd)
{
#if defined(ALLOW_INSTRUMENTING) || defined(ALLOW_PROFILING)
	qvm->profiler_mark = MARK_CLIENTTHINK;
#endif

#ifdef ALLOW_DEBUGGING
	qcvm_check_debugger_commands(qvm);
#endif

	const qcvm_ent_t ent = qcvm_entity_to_ent(qvm, e);
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
	qcvm_execute(qvm, game.funcs.ClientThink);
}

static void RunFrame()
{
#if defined(ALLOW_INSTRUMENTING) || defined(ALLOW_PROFILING)
	qvm->profiler_mark = MARK_RUNFRAME;
#endif

#ifdef ALLOW_DEBUGGING
	qcvm_check_debugger_commands(qvm);
#endif

	qcvm_execute(qvm, game.funcs.RunFrame);
}

static void ServerCommand()
{
#if defined(ALLOW_INSTRUMENTING) || defined(ALLOW_PROFILING)
	qvm->profiler_mark = MARK_SERVERCOMMAND;
#endif

#ifdef _DEBUG
	const char *cmd = gi.argv(1);

	if (strcmp(cmd, "qc_dump_strings") == 0)
	{
		FILE *fp = fopen(qcvm_temp_format(qvm, "%sstrings.txt", qvm->path), "w");
		qcvm_string_list_dump_refs(fp, qvm);
		fclose(fp);
		return;
	}
#endif

#ifdef ALLOW_DEBUGGING
	qcvm_check_debugger_commands(qvm);
#endif

	qcvm_execute(qvm, game.funcs.ServerCommand);
}

game_import_t gi;

game_t game;

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
