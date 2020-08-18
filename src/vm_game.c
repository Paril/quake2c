#include "shared/shared.h"
#include "vm.h"
#include "vm_string.h"

#include "game.h"
#include "vm_game.h"

static void QC_SetNumEdicts(qcvm_t *vm)
{
	globals.num_edicts = qcvm_argv_int32(vm, 0);
}

static void QC_ClearEntity(qcvm_t *vm)
{
	edict_t *entity = qcvm_argv_entity(vm, 0);
	const int32_t number = entity->s.number;

	memset(entity, 0, globals.edict_size);
	
	entity->s.number = number;

	qcvm_string_list_check_ref_unset(vm, entity, globals.edict_size / sizeof(qcvm_global_t), true);
	qcvm_field_wrap_list_check_set(&vm->field_wraps, entity, globals.edict_size / sizeof(qcvm_global_t));
}

static void QC_entitylinked(qcvm_t *vm)
{
	edict_t *entity = qcvm_argv_entity(vm, 0);
	const int32_t val = !!entity->area.prev;
	qcvm_return_int32(vm, val);
}

const char *ParseSlashes(const char *value)
{
	// no slashes to parse
	if (!strchr(value, '\\'))
		return value;

	// in Q2 this was 128, but just in case...
	// TODO: honestly I'd prefer to make this dynamic in future
	static char slashless_string[MAX_INFO_STRING];

	if (strlen(value) >= sizeof(slashless_string))
		gi.error("string overflow :(((((((");

	const char *src = value;
	char *dst = slashless_string;

	while (*src)
	{\
		const char c = *src;

		if (c == '\\')
		{
			const char next = *(src + 1);

			if (!next)
				gi.error("bad string");
			else if (next == '\\' || next == 'n')
			{
				*dst = (next == 'n') ? '\n' : '\\';
				src += 2;
				dst++;

				continue;
			}
		}

		*dst = c;
		src++;
		dst++;
	}

	*dst = 0;
	return slashless_string;
}

static inline void QC_parse_value_into_ptr(qcvm_t *vm, const qcvm_deftype_t type, const char *value, void *ptr)
{
	size_t data_span = 1;

	switch (type)
	{
	case TYPE_STRING: {
		value = ParseSlashes(value);
		*(qcvm_string_t *)ptr = qcvm_store_or_find_string(vm, value, strlen(value), true);
		break; }
	case TYPE_FLOAT:
		*(vec_t *)ptr = strtof(value, NULL);
		break;
	case TYPE_VECTOR:
		data_span = 3;
		sscanf(value, "%f %f %f", (vec_t *)ptr, (vec_t *)ptr + 1, (vec_t *)ptr + 2);
		break;
	case TYPE_INTEGER:
		*(int32_t *)ptr = strtol(value, NULL, 10);
		break;
	default:
		qcvm_error(vm, "Couldn't parse field, bad type %i", type);
	}
	
	qcvm_string_list_check_ref_unset(vm, ptr, data_span, false);
	qcvm_field_wrap_list_check_set(&vm->field_wraps, ptr, data_span);

	if (type == TYPE_STRING && qcvm_string_list_is_ref_counted(vm, *(qcvm_string_t *)ptr))
		qcvm_string_list_mark_ref_copy(vm, *(qcvm_string_t *)ptr, ptr);
}

static void QC_entity_key_parse(qcvm_t *vm)
{
	edict_t *ent = qcvm_argv_entity(vm, 0);
	const char *key = qcvm_argv_string(vm, 1);
	const char *value = qcvm_argv_string(vm, 2);

	qcvm_definition_hash_t *hashed = vm->definition_hashes[Q_hash_string(key, vm->definitions_size)];

	for (; hashed; hashed = hashed->hash_next)
		if ((hashed->def->id & ~TYPE_GLOBAL) == TYPE_FIELD && !strcmp(qcvm_get_string(vm, hashed->def->name_index), key))
			break;

	if (!hashed)
		qcvm_error(vm, "Bad field %s", key);

	const qcvm_global_t field = vm->global_data[hashed->def->global_index];

	void *ptr = qcvm_resolve_pointer(vm, qcvm_get_entity_field_pointer(vm, ent, field));

	qcvm_definition_t *f = vm->field_map_by_id[field];

	if (!f || strcmp(key, qcvm_get_string(vm, f->name_index)) || f->global_index != field)
		qcvm_error(vm, "Couldn't match field %u", field);

	QC_parse_value_into_ptr(vm, f->id, value, ptr);
}

static void QC_struct_key_parse(qcvm_t *vm)
{
	const char *struct_name = qcvm_argv_string(vm, 0);
	const char *key_name = qcvm_argv_string(vm, 1);
	const char *value = qcvm_argv_string(vm, 2);
	
	const char *full_name = qcvm_temp_format(vm, "%s.%s", struct_name, key_name);
	qcvm_definition_hash_t *hashed = vm->definition_hashes[Q_hash_string(full_name, vm->definitions_size)];

	for (; hashed; hashed = hashed->hash_next)
		if (!strcmp(qcvm_get_string(vm, hashed->def->name_index), full_name))
			break;

	if (!hashed)
	{
		qcvm_return_int32(vm, 0);
		return;
	}

	qcvm_definition_t *g = hashed->def;
	void *global = qcvm_get_global(vm, g->global_index);
	QC_parse_value_into_ptr(vm, g->id & ~TYPE_GLOBAL, value, global);
	qcvm_return_int32(vm, 1);
}

void qcvm_init_game_builtins(qcvm_t *vm)
{
	qcvm_register_builtin(SetNumEdicts);
	qcvm_register_builtin(ClearEntity);
	qcvm_register_builtin(entitylinked);
	
	qcvm_register_builtin(entity_key_parse);
	qcvm_register_builtin(struct_key_parse);
}