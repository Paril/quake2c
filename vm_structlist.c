#include "shared/shared.h"
#include "vm.h"
#include "vm_structlist.h"
#include "vm_math.h"

#define STRUCTLIST_RESERVE 32

typedef struct
{
	void	*values;
	size_t	element_size;
	size_t	size, allocated;
} qcvm_structlist_t;

static void qcvm_structlist_free(qcvm_t *vm, void *handle)
{
	qcvm_structlist_t *list = (qcvm_structlist_t *)handle;
	if (list->values)
		qcvm_mem_free(vm, list->values);
}

static bool	qcvm_structlist_resolve_pointer(const qcvm_t *vm, void *handle, const size_t offset, const size_t len, void **address)
{
	qcvm_structlist_t *list = (qcvm_structlist_t *)handle;

	if ((offset + len) <= list->size * list->element_size)
	{
		if (address)
			*address = (uint8_t *)list->values + offset;
		return true;
	}
	return false;
}

static const qcvm_handle_descriptor_t structlist_descriptor =
{
	.free = qcvm_structlist_free,
	.resolve_pointer = qcvm_structlist_resolve_pointer
};

static void QC_structlist_alloc(qcvm_t *vm)
{
	qcvm_structlist_t *list = (qcvm_structlist_t *)qcvm_alloc(vm, sizeof(qcvm_structlist_t));
	list->element_size = qcvm_argv_int32(vm, 0);

	if (!list->element_size)
		qcvm_error(vm, "bad element size");

	list->allocated = vm->state.argc > 1 ? qcvm_argv_int32(vm, 1) : STRUCTLIST_RESERVE;

	if (list->allocated)
		list->values = qcvm_alloc(vm, list->element_size * list->allocated);

	qcvm_return_handle(vm, list, &structlist_descriptor);
}

static inline void structlist_insert(qcvm_t *vm, qcvm_structlist_t *list, const qcvm_pointer_t value, const size_t index)
{
	void *address;

	if (!qcvm_resolve_pointer(vm, value, false, list->element_size, &address))
		qcvm_error(vm, "bad pointer");

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

		list->values = qcvm_alloc(vm, list->element_size * list->allocated);

		if (old_values)
		{
			memcpy(list->values, old_values, list->element_size * list->size);
			qcvm_mem_free(vm, old_values);
		}
	}

	// shift the list to accomodate new entry
	if (index < list->size)
	{
		const size_t shift_size = (list->size - index) * list->element_size;
		memmove((uint8_t *)list->values + (list->element_size * (index + 1)), (uint8_t *)list->values + (list->element_size * index), shift_size);
	}

	memcpy((uint8_t *)list->values + (list->element_size * index), address, list->element_size);
	list->size++;
}

static void QC_structlist_insert(qcvm_t *vm)
{
	qcvm_structlist_t *list = qcvm_argv_handle(qcvm_structlist_t, vm, 0);
	const qcvm_pointer_t value = qcvm_argv_pointer(vm, 1);
	const size_t index = qcvm_argv_int32(vm, 2);
	structlist_insert(vm, list, value, index);
}

static void QC_structlist_push(qcvm_t *vm)
{
	qcvm_structlist_t *list = qcvm_argv_handle(qcvm_structlist_t, vm, 0);
	const qcvm_pointer_t value = qcvm_argv_pointer(vm, 1);
	structlist_insert(vm, list, value, list->size);
}

static void QC_structlist_unshift(qcvm_t *vm)
{
	qcvm_structlist_t *list = qcvm_argv_handle(qcvm_structlist_t, vm, 0);
	const qcvm_pointer_t value = qcvm_argv_pointer(vm, 1);
	structlist_insert(vm, list, value, 0);
}

static inline void structlist_delete(qcvm_t *vm, qcvm_structlist_t *list, const size_t index, void *store)
{
	if (index >= list->size)
		qcvm_error(vm, "bad delete index");

	if (store)
		memcpy(store, (uint8_t *)list->values + (list->element_size * index), list->element_size);

	// shift if we have more than 1
	if (list->size > 1)
	{
		const size_t shift_size = (list->size - index - 1) * list->element_size;
		memmove((uint8_t *)list->values + (list->element_size * index), (uint8_t *)list->values + (list->element_size * index + 1), shift_size);
	}

	list->size--;
}

static void QC_structlist_delete(qcvm_t *vm)
{
	qcvm_structlist_t *list = qcvm_argv_handle(qcvm_structlist_t, vm, 0);
	const size_t index = qcvm_argv_int32(vm, 1);
	void *store = NULL;

	if (vm->state.argc > 2)
		if (!qcvm_resolve_pointer(vm, qcvm_argv_pointer(vm, 2), false, list->element_size, &store))
			qcvm_error(vm, "bad pointer");

	structlist_delete(vm, list, index, store);
}

static void QC_structlist_pop(qcvm_t *vm)
{
	qcvm_structlist_t *list = qcvm_argv_handle(qcvm_structlist_t, vm, 0);
	void *store = NULL;

	if (vm->state.argc > 1)
		if (!qcvm_resolve_pointer(vm, qcvm_argv_pointer(vm, 1), false, list->element_size, &store))
			qcvm_error(vm, "bad pointer");

	structlist_delete(vm, list, list->size - 1, store);
}

static void QC_structlist_shift(qcvm_t *vm)
{
	qcvm_structlist_t *list = qcvm_argv_handle(qcvm_structlist_t, vm, 0);
	void *store = NULL;

	if (vm->state.argc > 1)
		if (!qcvm_resolve_pointer(vm, qcvm_argv_pointer(vm, 1), false, list->element_size, &store))
			qcvm_error(vm, "bad pointer");

	structlist_delete(vm, list, 0, store);
}

static void QC_structlist_get_length(qcvm_t *vm)
{
	qcvm_structlist_t *list = qcvm_argv_handle(qcvm_structlist_t, vm, 0);
	qcvm_return_int32(vm, list->size);
}

static void QC_structlist_clear(qcvm_t *vm)
{
	qcvm_structlist_t *list = qcvm_argv_handle(qcvm_structlist_t, vm, 0);
	list->size = 0;
}

static void QC_structlist_at(qcvm_t *vm)
{
	qcvm_handle_id_t handleid = qcvm_argv_int32(vm, 0);
	qcvm_structlist_t *list = qcvm_fetch_handle_typed(qcvm_structlist_t, vm, handleid);
	const size_t index = qcvm_argv_int32(vm, 1);

	if (index >= list->size)
		qcvm_error(vm, "bad index");

	qcvm_return_pointer(vm, (qcvm_pointer_t) { .handle = { .type = QCVM_POINTER_HANDLE, .index = handleid, .offset = list->element_size * index } });
}

static void QC_structlist_value_at(qcvm_t *vm)
{
	qcvm_handle_id_t handleid = qcvm_argv_int32(vm, 0);
	qcvm_structlist_t *list = qcvm_fetch_handle_typed(qcvm_structlist_t, vm, handleid);
	const size_t index = qcvm_argv_int32(vm, 1);

	if (index >= list->size)
		qcvm_error(vm, "bad index");

	void *store;

	if (!qcvm_resolve_pointer(vm, qcvm_argv_pointer(vm, 2), false, list->element_size, &store))
		qcvm_error(vm, "bad pointer");

	memcpy(store, (uint8_t *)list->values + (list->element_size * index), list->element_size);
}

static void QC_structlist_resize(qcvm_t *vm)
{
	qcvm_structlist_t *list = qcvm_argv_handle(qcvm_structlist_t, vm, 0);
	const size_t index = qcvm_argv_int32(vm, 1);

	if (index <= list->allocated)
		list->size = index;
	else
	{
		void *old_values = list->values;

		if (!list->allocated)
			list->allocated = STRUCTLIST_RESERVE;
		else while (index > list->allocated)
			list->allocated *= 2;

		list->values = qcvm_alloc(vm, list->element_size * list->allocated);

		if (old_values)
		{
			memcpy(list->values, old_values, list->element_size * list->size);
			qcvm_mem_free(vm, old_values);
		}
	}
}

void qcvm_init_structlist_builtins(qcvm_t *vm)
{
	qcvm_register_builtin(structlist_alloc);
	qcvm_register_builtin(structlist_insert);
	qcvm_register_builtin(structlist_push);
	qcvm_register_builtin(structlist_unshift);
	qcvm_register_builtin(structlist_delete);
	qcvm_register_builtin(structlist_pop);
	qcvm_register_builtin(structlist_shift);
	qcvm_register_builtin(structlist_get_length);
	qcvm_register_builtin(structlist_clear);
	qcvm_register_builtin(structlist_at);
	qcvm_register_builtin(structlist_value_at);
	qcvm_register_builtin(structlist_resize);
}