#include "shared/shared.h"
#include "game.h"
#include "g_vm.h"

static void QC_SetNumEdicts(QCVM &vm)
{
	globals.num_edicts = vm.ArgvInt32(0);
}

static void QC_ClearEntity(QCVM &vm)
{
	auto entity = vm.ArgvEntity(0);
	int32_t number = entity->s.number;

	memset(entity, 0, globals.edict_size);
	
	entity->s.number = number;

	if (number > 0 && number <= game.clients.size())
		entity->client = &game.clients[number - 1];

	vm.dynamic_strings.CheckRefUnset(entity, sizeof(*entity) / sizeof(global_t));
}

static void QC_SyncPlayerState(QCVM &vm)
{
	auto ent = vm.ArgvEntity(0);

	for (auto &wrap : vm.field_wraps.GetFields())
	{
		const auto &field = vm.GetEntityFieldPointer(*ent, wrap.first / sizeof(global_t));
		vm.field_wraps.WrapField(*ent, wrap.first, field);
	}
}

static inline void QC_parse_value_into_ptr(QCVM &vm, const deftype_t &type, const char *value, void *ptr)
{
	size_t data_span = 1;

	switch (type)
	{
	case TYPE_STRING:
		*reinterpret_cast<string_t *>(ptr) = vm.StoreOrFind(value);
		break;
	case TYPE_FLOAT:
		*reinterpret_cast<vec_t *>(ptr) = strtof(value, nullptr);
		break;
	case TYPE_VECTOR:
		data_span = 3;
		sscanf(value, "%f %f %f", reinterpret_cast<vec_t *>(ptr), reinterpret_cast<vec_t *>(ptr) + 1, reinterpret_cast<vec_t *>(ptr) + 2);
		break;
	case TYPE_INTEGER:
		*reinterpret_cast<int32_t *>(ptr) = strtol(value, nullptr, 10);
		break;
	default:
		vm.Error("Couldn't parse field, bad type %i", type);
	}
	
	vm.dynamic_strings.CheckRefUnset(ptr, data_span);

	if (type == TYPE_STRING && vm.dynamic_strings.IsRefCounted(*reinterpret_cast<string_t *>(ptr)))
		vm.dynamic_strings.MarkRefCopy(*reinterpret_cast<string_t *>(ptr), ptr);
}

static void QC_entity_key_parse(QCVM &vm)
{
	auto ent = vm.ArgvEntity(0);
	const auto &field = vm.ArgvInt32(1);
	const auto &value = vm.ArgvString(2);

	void *ptr = vm.GetEntityFieldPointer(*ent, field);

	// FIXME: this should be constant time
	for (auto &f : vm.fields)
	{
		if (f.global_index != field)
			continue;

		QC_parse_value_into_ptr(vm, f.id, value, ptr);
		return;
	}

	vm.Error("Couldn't match field %i", field);
}

static void QC_struct_key_parse(QCVM &vm)
{
	const auto &struct_name = vm.ArgvString(0);
	const auto &key_name = vm.ArgvString(1);
	const auto &value = vm.ArgvString(2);
	
	const size_t struct_len = strlen(struct_name);
	const size_t key_len = strlen(key_name);
	
	// FIXME: this should be constant time
	for (auto &g : vm.definitions)
	{
		if (g.name_index == string_t::STRING_EMPTY)
			continue;

		const auto &name = vm.GetString(g.name_index);

		if (Q_stricmpn(name, struct_name, struct_len))
			continue;

		if (name[struct_len] != '.')
			continue;

		if (Q_stricmpn(name + struct_len + 1, key_name, key_len))
			continue;

		// match!
		auto global = vm.GetGlobalByIndex(static_cast<global_t>(g.global_index));
		QC_parse_value_into_ptr(vm, static_cast<deftype_t>(g.id & ~TYPE_GLOBAL), value, global);
		vm.Return(1);
		return;
	}

	vm.Return(0);
}

static void QC_itoe(QCVM &vm)
{
	const auto &number = vm.ArgvInt32(0);
	vm.Return(reinterpret_cast<int32_t>(reinterpret_cast<uint8_t *>(globals.edicts) + (globals.edict_size * number)));
}

static void QC_etoi(QCVM &vm)
{
	const auto &address = vm.ArgvInt32(0);
	vm.Return((reinterpret_cast<uint8_t *>(address) - reinterpret_cast<uint8_t *>(globals.edicts)) / globals.edict_size);
}

void InitGameBuiltins(QCVM &vm)
{
	RegisterBuiltin(SetNumEdicts);
	RegisterBuiltin(ClearEntity);
	RegisterBuiltin(SyncPlayerState);
	
	RegisterBuiltin(entity_key_parse);
	RegisterBuiltin(struct_key_parse);

	RegisterBuiltin(itoe);
	RegisterBuiltin(etoi);
}