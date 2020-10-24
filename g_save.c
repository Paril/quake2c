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
#include "game.h"
#include "g_main.h"
#include "vm_string.h"

//=========================================================

static const uint32_t	SAVE_MAGIC1		= (('V'<<24)|('S'<<16)|('C'<<8)|'Q');	// "QCSV"
static const uint32_t	SAVE_MAGIC2		= (('A'<<24)|('S'<<16)|('C'<<8)|'Q');	// "QCSA"
static const uint32_t	SAVE_VERSION	= 666;

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
	else if (type == TYPE_ENTITY)
	{
		const edict_t *ent = qcvm_ent_to_entity(qvm, (qcvm_ent_t)*value, false);
		int32_t number = -1;

		if (ent != NULL)
			number = ent->s.number;

		fwrite(&number, sizeof(number), 1, fp);
		return;
	}

	fwrite(value, sizeof(qcvm_global_t), qcvm_type_span(def->id), fp);
}

static void WriteEntityFieldData(FILE *fp, edict_t *ent, const qcvm_definition_t *def)
{
	int32_t *field;
	
	if (!qcvm_resolve_pointer(qvm, qcvm_get_entity_field_pointer(qvm, ent, (int32_t)def->global_index), false, qcvm_type_size(def->id), (void**)&field))
		qcvm_error(qvm, "bad pointer");

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
		qcvm_set_string_ptr(qvm, value, def_value, def_len, true);
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
	else if (type == TYPE_ENTITY)
	{
		int32_t number;
		fread(&number, sizeof(number), 1, fp);
		*value = (qcvm_global_t)qcvm_entity_to_ent(qvm, qcvm_itoe(qvm, number));
		return;
	}

	fread(value, sizeof(qcvm_global_t), qcvm_type_span(def->id), fp);
}

static void ReadEntityFieldData(qcvm_t *vm, FILE *fp, edict_t *ent, const qcvm_definition_t *def)
{
	int32_t *field;

	if (!qcvm_resolve_pointer(qvm, qcvm_get_entity_field_pointer(qvm, ent, (int32_t)def->global_index), false, qcvm_type_size(def->id), (void**)&field))
		qcvm_error(vm, "bad pointer");

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
void WriteGame(const char *filename, qboolean autosave)
{
#if defined(ALLOW_INSTRUMENTING) || defined(ALLOW_PROFILING)
	qvm->profiler_mark = MARK_WRITEGAME;
#endif

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

		for (uint32_t i = 0; i < game.num_clients; i++)
			WriteEntityFieldData(fp, qcvm_itoe(qvm, i + 1), def);
	}
	
	name_len = 0;
	fwrite(&name_len, sizeof(name_len), 1, fp);
	
	func = qcvm_get_function(qvm, qce.PostWriteGame);
	qcvm_set_global_typed_value(qboolean, qvm, GLOBAL_PARM0, autosave);
	qcvm_execute(qvm, func);

	fclose(fp);
}

void ReadGame(const char *filename)
{
#if defined(ALLOW_INSTRUMENTING) || defined(ALLOW_PROFILING)
	qvm->profiler_mark = MARK_READGAME;
#endif

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
	qcvm_string_list_check_ref_unset(qvm, globals.edicts, (globals.edict_size * globals.max_edicts) / sizeof(qcvm_global_t), false);

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

	for (uint32_t i = 0; i < game.num_clients; i++)
		AssignClientPointer(qcvm_itoe(qvm, i + 1), true);

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
		
		if (!qcvm_find_string(qvm, def_name, &str) || qcvm_string_list_is_ref_counted(qvm, str))
			qcvm_error(qvm, "Bad string in save file");

		qcvm_definition_hash_t *hashed = qvm->field_hashes[Q_hash_string(def_name, qvm->fields_size)];

		for (; hashed; hashed = hashed->hash_next)
			if (!strcmp(qcvm_get_string(qvm, hashed->def->name_index), def_name))
				break;

		if (!hashed)
			qcvm_error(qvm, "Bad field %s", def_name);
		
		qcvm_definition_t *field = hashed->def;
		
		for (uint32_t i = 0; i < game.num_clients; i++)
			ReadEntityFieldData(qvm, fp, qcvm_itoe(qvm, i + 1), field);
	}

	func = qcvm_get_function(qvm, qce.PostReadGame);
	qcvm_execute(qvm, func);

	qcvm_field_wrap_list_check_set(qvm, qcvm_itoe(qvm, 1), (globals.edict_size * game.num_clients) / sizeof(qcvm_global_t));

	fclose(fp);
}

//==========================================================

/*
=================
WriteLevel

=================
*/
void WriteLevel(const char *filename)
{
#if defined(ALLOW_INSTRUMENTING) || defined(ALLOW_PROFILING)
	qvm->profiler_mark = MARK_WRITELEVEL;
#endif

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

		for (uint32_t i = 0; i < globals.num_edicts; i++)
		{
			edict_t *ent = qcvm_itoe(qvm, i);

			if (!ent->inuse)
				continue;

			fwrite(&i, sizeof(i), 1, fp);
			WriteEntityFieldData(fp, ent, def);
		}

		uint32_t i = -1;
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
void ReadLevel(const char *filename)
{
#if defined(ALLOW_INSTRUMENTING) || defined(ALLOW_PROFILING)
	qvm->profiler_mark = MARK_READLEVEL;
#endif

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
	qcvm_string_list_check_ref_unset(qvm, globals.edicts, (globals.edict_size * globals.max_edicts) / sizeof(qcvm_global_t), false);

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
		
		if (!qcvm_find_string(qvm, def_name, &str) || qcvm_string_list_is_ref_counted(qvm, str))
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
			uint32_t ent_id;
			fread(&ent_id, sizeof(ent_id), 1, fp);

			if (ent_id == -1u)
				break;

			if (ent_id >= globals.max_edicts)
				qcvm_error(qvm, "%s: bad entity number", __func__);

			if (ent_id >= globals.num_edicts)
				globals.num_edicts = ent_id + 1;

			edict_t *ent = qcvm_itoe(qvm, ent_id);
			ReadEntityFieldData(qvm, fp, ent, field);
		}
	}

	WipeClientPointers();

	RestoreClientData();

	for (uint32_t i = 0; i < globals.num_edicts; i++)
	{
		edict_t *ent = qcvm_itoe(qvm, i);

		// let the server rebuild world links for this ent
		ent->area = (list_t) { NULL, NULL };
		gi.linkentity(ent);
	}
	
	func = qcvm_get_function(qvm, qce.PostReadLevel);
	qcvm_set_global_typed_value(int32_t, qvm, GLOBAL_PARM0, globals.num_edicts);
	qcvm_execute(qvm, func);

	fclose(fp);
}
