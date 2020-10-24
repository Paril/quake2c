#include "shared/shared.h"
#include "vm.h"
#include "vm_list.h"
#include "vm_math.h"

#define STRUCTLIST_RESERVE 32

typedef struct
{
	qcvm_variant_t *values;
	size_t	size, allocated;
} qcvm_list_t;

static void qcvm_list_free(qcvm_t *vm, void *handle)
{
	qcvm_list_t *list = (qcvm_list_t *)handle;
	if (list->values)
		qcvm_mem_free(vm, list->values);
}

static bool	qcvm_list_resolve_pointer(const qcvm_t *vm, void *handle, const size_t offset, const size_t len, void **address)
{
	qcvm_list_t *list = (qcvm_list_t *)handle;

	if ((offset + len) <= list->size * sizeof(qcvm_variant_t))
	{
		if (address)
			*address = (uint8_t *)list->values + offset;
		return true;
	}
	return false;
}

static const qcvm_handle_descriptor_t list_descriptor =
{
	.free = qcvm_list_free,
	.resolve_pointer = qcvm_list_resolve_pointer
};

static void QC_list_alloc(qcvm_t *vm)
{
	qcvm_list_t *list = (qcvm_list_t *)qcvm_alloc(vm, sizeof(qcvm_list_t));

	list->allocated = vm->state.argc > 0 ? qcvm_argv_int32(vm, 0) : STRUCTLIST_RESERVE;

	if (list->allocated)
		list->values = qcvm_alloc(vm, sizeof(qcvm_variant_t) * list->allocated);

	qcvm_return_handle(vm, list, &list_descriptor);
}

static inline void list_insert(qcvm_t *vm, qcvm_list_t *list, const qcvm_variant_t value, const size_t index)
{
	if (index > list->size)
		qcvm_error(vm, "bad insert index");

	// re-allocate
	if (list->size == list->allocated)
	{
		void *old_values = list->values;

		if (!list->allocated)
			list->allocated = STRUCTLIST_RESERVE;
		else
			list->allocated *= 2;

		list->values = qcvm_alloc(vm, sizeof(qcvm_variant_t) * list->allocated);

		if (old_values)
		{
			memcpy(list->values, old_values, sizeof(qcvm_variant_t) * list->size);
			qcvm_mem_free(vm, old_values);
		}
	}

	// shift the list to accomodate new entry
	if (index < list->size)
	{
		const size_t shift_size = (list->size - index) * sizeof(qcvm_variant_t);
		memmove((uint8_t *)list->values + (sizeof(qcvm_variant_t) * (index + 1)), (uint8_t *)list->values + (sizeof(qcvm_variant_t) * index), shift_size);
	}

	memcpy((uint8_t *)list->values + (sizeof(qcvm_variant_t) * index), &value, sizeof(qcvm_variant_t));
	list->size++;
}

static void QC_list_insert(qcvm_t *vm)
{
	qcvm_list_t *list = qcvm_argv_handle(qcvm_list_t, vm, 0);
	const qcvm_variant_t value = qcvm_argv_variant(vm, 1);
	const size_t index = qcvm_argv_int32(vm, 2);
	list_insert(vm, list, value, index);
}

static void QC_list_push(qcvm_t *vm)
{
	qcvm_list_t *list = qcvm_argv_handle(qcvm_list_t, vm, 0);
	const qcvm_variant_t value = qcvm_argv_variant(vm, 1);
	list_insert(vm, list, value, list->size);
}

static void QC_list_unshift(qcvm_t *vm)
{
	qcvm_list_t *list = qcvm_argv_handle(qcvm_list_t, vm, 0);
	const qcvm_variant_t value = qcvm_argv_variant(vm, 1);
	list_insert(vm, list, value, 0);
}

static inline void list_delete(qcvm_t *vm, qcvm_list_t *list, const size_t index)
{
	if (index >= list->size)
		qcvm_error(vm, "bad delete index");

	// shift if we have more than 1
	if (list->size > 1)
	{
		const size_t shift_size = (list->size - index - 1) * sizeof(qcvm_variant_t);
		memmove((uint8_t *)list->values + (sizeof(qcvm_variant_t) * index), (uint8_t *)list->values + (sizeof(qcvm_variant_t) * index + 1), shift_size);
	}

	list->size--;
}

static void QC_list_delete(qcvm_t *vm)
{
	qcvm_list_t *list = qcvm_argv_handle(qcvm_list_t, vm, 0);
	const size_t index = qcvm_argv_int32(vm, 1);

	if (vm->state.argc > 2)
		qcvm_set_global_typed_ptr(qcvm_variant_t, vm, GLOBAL_PARM2, list->values + index);

	list_delete(vm, list, index);
}

static void QC_list_pop(qcvm_t *vm)
{
	qcvm_list_t *list = qcvm_argv_handle(qcvm_list_t, vm, 0);

	if (vm->state.argc > 1)
		qcvm_set_global_typed_ptr(qcvm_variant_t, vm, GLOBAL_PARM1, list->values + (list->size - 1));

	list_delete(vm, list, list->size - 1);
}

static void QC_list_shift(qcvm_t *vm)
{
	qcvm_list_t *list = qcvm_argv_handle(qcvm_list_t, vm, 0);

	if (vm->state.argc > 1)
		qcvm_set_global_typed_ptr(qcvm_variant_t, vm, GLOBAL_PARM1, list->values);

	list_delete(vm, list, 0);
}

static void QC_list_get_length(qcvm_t *vm)
{
	qcvm_list_t *list = qcvm_argv_handle(qcvm_list_t, vm, 0);
	qcvm_return_int32(vm, list->size);
}

static void QC_list_clear(qcvm_t *vm)
{
	qcvm_list_t *list = qcvm_argv_handle(qcvm_list_t, vm, 0);
	list->size = 0;
}

static void QC_list_at(qcvm_t *vm)
{
	qcvm_handle_id_t handleid = qcvm_argv_int32(vm, 0);
	qcvm_list_t *list = qcvm_fetch_handle_typed(qcvm_list_t, vm, handleid);
	const size_t index = qcvm_argv_int32(vm, 1);

	if (index >= list->size)
		qcvm_error(vm, "bad index");

	qcvm_return_variant(vm, *(list->values + index));
}

static void QC_list_set(qcvm_t *vm)
{
	qcvm_list_t *list = qcvm_argv_handle(qcvm_list_t, vm, 0);
	const size_t index = qcvm_argv_int32(vm, 1);
	const qcvm_variant_t value = qcvm_argv_variant(vm, 2);

	if (index >= list->size)
		qcvm_error(vm, "bad index");

	*(list->values + index) = value;
}

void qcvm_init_list_builtins(qcvm_t *vm)
{
	qcvm_register_builtin(list_alloc);
	qcvm_register_builtin(list_insert);
	qcvm_register_builtin(list_push);
	qcvm_register_builtin(list_unshift);
	qcvm_register_builtin(list_delete);
	qcvm_register_builtin(list_pop);
	qcvm_register_builtin(list_shift);
	qcvm_register_builtin(list_get_length);
	qcvm_register_builtin(list_clear);
	qcvm_register_builtin(list_at);
	qcvm_register_builtin(list_set);
}