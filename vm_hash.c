#include "shared/shared.h"
#include "vm.h"
#include "vm_hash.h"
#include "vm_math.h"

qcvm_hashset_t *hashset_alloc(const qcvm_t *vm, const size_t reserve)
{
	qcvm_hashset_t *set = (qcvm_hashset_t *)qcvm_alloc(vm, sizeof(qcvm_hashset_t));

	set->allocated = reserve;

	if (set->allocated)
	{
		set->values = (qcvm_hash_value_t *)qcvm_alloc(vm, sizeof(qcvm_hash_value_t) * set->allocated);
		set->hashed = (qcvm_hash_value_t **)qcvm_alloc(vm, sizeof(qcvm_hash_value_t *) * set->allocated);
		set->indexed = (qcvm_hash_value_t **)qcvm_alloc(vm, sizeof(qcvm_hash_value_t *) * set->allocated);

		for (qcvm_hash_value_t *v = set->values; v < set->values + set->allocated; v++)
		{
			v->hash_next = set->free;
			set->free = v;
		}
	}

	return set;
}

void hashset_free(const qcvm_t *vm, qcvm_hashset_t *set)
{
	if (set->allocated)
	{
		qcvm_mem_free(vm, set->values);
		qcvm_mem_free(vm, set->hashed);
	}

	qcvm_mem_free(vm, set);
}

bool hashset_add(qcvm_t *vm, qcvm_hashset_t *set, const qcvm_variant_t variant)
{
	uint32_t hash = 0;
	
	if (set->allocated)
	{
		hash = Q_hash_variant(variant, set->allocated);
	
		for (qcvm_hash_value_t *v = set->hashed[hash]; v; v = v->hash_next)
			if (qcvm_variant_equals(v->value, variant))
				return false;
	}

	if (set->size == set->allocated)
	{
		const size_t old_size = set->allocated;

		if (!set->allocated)
			set->allocated = HASH_RESERVE;
		else
			set->allocated *= 2;

		qcvm_hash_value_t *old_values = set->values;

		if (old_values)
		{
			qcvm_mem_free(vm, set->hashed);
			qcvm_mem_free(vm, set->indexed);
		}

		set->values = (qcvm_hash_value_t *)qcvm_alloc(vm, sizeof(qcvm_hash_value_t) * set->allocated);
		set->hashed = (qcvm_hash_value_t **)qcvm_alloc(vm, sizeof(qcvm_hash_value_t *) * set->allocated);
		set->indexed = (qcvm_hash_value_t **)qcvm_alloc(vm, sizeof(qcvm_hash_value_t *) * set->allocated);

		if (old_values)
		{
			memcpy(set->values, old_values, sizeof(qcvm_hash_value_t) * old_size);
			qcvm_mem_free(vm, old_values);
		}

		set->free = NULL;

		size_t i = 0;

		// re-hash since hashs changed
		for (qcvm_hash_value_t *v = set->values; v < set->values + set->allocated; v++)
		{
			// we're free, so ignore us
			if (v->value.type == TYPE_VOID)
			{
				v->hash_next = set->free;
				set->free = v;
				continue;
			}

			v->hash_value = Q_hash_variant(v->value, set->allocated);
			v->hash_next = set->hashed[v->hash_value];
			set->hashed[v->hash_value] = v;
			set->indexed[i++] = v;
		}

		hash = Q_hash_variant(variant, set->allocated);
	}

	qcvm_hash_value_t *v = set->free;
	set->free = v->hash_next;

	assert(v->value.type == TYPE_VOID);

	v->value = variant;
	v->hash_value = hash;
	v->hash_next = set->hashed[v->hash_value];
	set->hashed[v->hash_value] = v;

	if (variant.type == TYPE_STRING)
		qcvm_string_list_acquire(vm, variant.value.str);

	set->indexed[set->size] = v;
	v->index = set->size;
	set->size++;
	return true;
}

bool hashset_remove(qcvm_t *vm, qcvm_hashset_t *set, const qcvm_variant_t variant)
{
	if (!set->allocated)
		return false;

	const uint32_t hash = Q_hash_variant(variant, set->allocated);
	
	for (qcvm_hash_value_t **v = &set->hashed[hash], **p = NULL; *v; p = v, v = &(*v)->hash_next)
	{
		if (!qcvm_variant_equals((*v)->value, variant))
			continue;

		qcvm_hash_value_t *old_v = (*v);

		old_v->value.type = TYPE_VOID;

		if ((*v)->index < set->size - 1)
		{
			memmove(set->indexed + (*v)->index, set->indexed + ((*v)->index + 1), sizeof(qcvm_hash_value_t *) * ((set->size - 1) - (*v)->index));

			for (size_t i = (*v)->index; i < set->size - 1; i++)
				set->indexed[i]->index = i;
		}

		*v = (*v)->hash_next;
		
		if (p)
			(*p)->hash_next = *v;
		
		old_v->hash_next = set->free;
		set->free = old_v;

		if (variant.type == TYPE_STRING)
			qcvm_string_list_release(vm, variant.value.str);

		set->size--;
		return true;
	}

	return false;
}

bool hashset_contains(qcvm_t *vm, qcvm_hashset_t *set, const qcvm_variant_t variant)
{
	if (!set->allocated)
		return false;

	const uint32_t hash = Q_hash_variant(variant, set->allocated);
	
	for (qcvm_hash_value_t *v = set->hashed[hash]; v; v = v->hash_next)
		if (qcvm_variant_equals(v->value, variant))
			return true;

	return false;
}

void hashset_clear(qcvm_t *vm, qcvm_hashset_t *set)
{
	if (!set->size)
		return;

	for (qcvm_hash_value_t *v = set->values; v < set->values + set->size; v++)
		if (v->value.type == TYPE_STRING)
			qcvm_string_list_release(vm, v->value.value.str);

	memset(set->values, 0, sizeof(qcvm_hash_value_t) * set->allocated);
	memset(set->hashed, 0, sizeof(qcvm_hash_value_t *) * set->allocated);
	set->size = 0;
	set->free = NULL;

	for (qcvm_hash_value_t *v = set->values; v < set->values + set->allocated; v++)
	{
		v->hash_next = set->free;
		set->free = v;
	}
}

static void qcvm_hashset_free(qcvm_t *vm, void *handle)
{
	qcvm_hashset_t *set = (qcvm_hashset_t *)handle;
	hashset_free(vm, set);
}

const qcvm_handle_descriptor_t hashset_descriptor =
{
	.free = qcvm_hashset_free
};

static void QC_hashset_alloc(qcvm_t *vm)
{
	const size_t reserve = vm->state.argc ? qcvm_argv_int32(vm, 0) : HASH_RESERVE;
	qcvm_hashset_t *set = hashset_alloc(vm, reserve);
	qcvm_return_handle(vm, set, &hashset_descriptor);
}

static qcvm_always_inline void QC_hashset_func(qcvm_t *vm, bool (*func)(qcvm_t *vm, qcvm_hashset_t *set, const qcvm_variant_t variant))
{
	qcvm_hashset_t *set = qcvm_argv_handle(qcvm_hashset_t, vm, 0);
	qcvm_variant_t variant = {
		.type = qcvm_argv_int32(vm, 1)
	};

	assert(variant.type > TYPE_VOID && variant.type <= TYPE_INTEGER);

	if (variant.type == TYPE_VECTOR)
		variant.value.vec = qcvm_argv_vector(vm, 2);
	else
		variant.value.itg = qcvm_argv_int32(vm, 2);

	const bool added = func(vm, set, variant);
	qcvm_return_int32(vm, added);
}

static void QC_hashset_add(qcvm_t *vm)
{
	QC_hashset_func(vm, hashset_add);
}

static void QC_hashset_remove(qcvm_t *vm)
{
	QC_hashset_func(vm, hashset_remove);
}

static void QC_hashset_contains(qcvm_t *vm)
{
	QC_hashset_func(vm, hashset_contains);
}

static void QC_hashset_get_length(qcvm_t *vm)
{
	qcvm_hashset_t *set = qcvm_argv_handle(qcvm_hashset_t, vm, 0);
	qcvm_return_int32(vm, (int32_t)set->size);
}

static void QC_hashset_clear(qcvm_t *vm)
{
	qcvm_hashset_t *set = qcvm_argv_handle(qcvm_hashset_t, vm, 0);
	hashset_clear(vm, set);
}

static void QC_hashset_at(qcvm_t *vm)
{
	qcvm_hashset_t *set = qcvm_argv_handle(qcvm_hashset_t, vm, 0);
	int32_t index = qcvm_argv_int32(vm, 1);

	if (index < 0 || index >= set->size)
		qcvm_error(vm, "set index overrun");

	qcvm_hash_value_t *v = set->indexed[index];

	if (v->value.type == TYPE_VECTOR)
		qcvm_return_vector(vm, v->value.value.vec);
	else
		qcvm_return_int32(vm, v->value.value.itg);

	if (vm->state.argc > 2)
		qcvm_set_global_typed_value(int32_t, vm, GLOBAL_PARM2, v->value.type);
}

void qcvm_init_hash_builtins(qcvm_t *vm)
{
	qcvm_register_builtin(hashset_alloc);
	qcvm_register_builtin(hashset_add);
	qcvm_register_builtin(hashset_remove);
	qcvm_register_builtin(hashset_contains);
	qcvm_register_builtin(hashset_get_length);
	qcvm_register_builtin(hashset_clear);
	qcvm_register_builtin(hashset_at);
}