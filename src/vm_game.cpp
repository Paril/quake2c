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

	if (number > 0 && number <= game.num_clients)
		entity->client = &game.clients[number - 1];

	vm.dynamic_strings.CheckRefUnset(entity, sizeof(*entity) / sizeof(global_t));
}

void SyncPlayerState(QCVM &vm, edict_t *ent)
{
	for (auto &wrap : vm.field_wraps.GetFields())
	{
		const auto &field = vm.GetEntityFieldPointer(ent, wrap.first / sizeof(global_t));
		vm.field_wraps.WrapField(ent, wrap.first, field);
	}
}

static void QC_SyncPlayerState(QCVM &vm)
{
	auto ent = vm.ArgvEntity(0);
	SyncPlayerState(vm, ent);
}

std::string ParseSlashes(const char *value)
{
	std::string v(value);

	size_t index = 0;

	while (true)
	{
		index = v.find_first_of('\\', index);

		if (index == v.npos || index + 1 >= v.length())
			break;

		if (v.at(index + 1) == 'n')
		{
			v.replace(index, 2, 1, '\n');
			index++;
		}
		else if (v.at(index + 1) == '\\')
		{
			v.replace(index, 2, 1, '\\');
			index++;
		}
		else
			index += 2;
	}

	return v;
}

static inline void QC_parse_value_into_ptr(QCVM &vm, const deftype_t &type, const char *value, void *ptr)
{
	size_t data_span = 1;

	switch (type)
	{
	case TYPE_STRING:
		*reinterpret_cast<string_t *>(ptr) = vm.StoreOrFind(ParseSlashes(value));
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

	void *ptr = vm.GetEntityFieldPointer(ent, field);

	auto f = vm.field_map.find(static_cast<global_t>(field));

	if (f == vm.field_map.end())
		vm.Error("Couldn't match field %i", field);

	QC_parse_value_into_ptr(vm, (*f).second->id, value, ptr);
}

static void QC_struct_key_parse(QCVM &vm)
{
	const auto &struct_name = vm.ArgvString(0);
	const auto &key_name = vm.ArgvString(1);
	const auto &value = vm.ArgvString(2);
	
	auto full_name = vas("%s.%s", struct_name, key_name);
	auto hashed = vm.definition_map_by_name.find(full_name);

	if (hashed == vm.definition_map_by_name.end())
	{
		vm.ReturnInt(0);
		return;
	}

	auto g = (*hashed).second;
	auto global = vm.GetGlobalByIndex(static_cast<global_t>(g->global_index));
	QC_parse_value_into_ptr(vm, static_cast<deftype_t>(g->id & ~TYPE_GLOBAL), value, global);
	vm.ReturnInt(1);
}

static void QC_itoe(QCVM &vm)
{
	const auto &number = vm.ArgvInt32(0);
	vm.ReturnEntity(itoe(number));
}

static void QC_etoi(QCVM &vm)
{
	const auto &address = vm.ArgvInt32(0);
	vm.ReturnInt((reinterpret_cast<uint8_t *>(address) - reinterpret_cast<uint8_t *>(globals.edicts)) / globals.edict_size);
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