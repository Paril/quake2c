#include "shared/shared.h"
#include "vm.h"
#include "vm_heap.h"
#include "vm_math.h"

typedef struct
{
	void	*ptr;
	size_t	size;
} qcvm_heap_t;

static void qcvm_heap_free(qcvm_t *vm, void *handle)
{
	qcvm_heap_t *list = (qcvm_heap_t *)handle;
	if (list->ptr)
		qcvm_mem_free(vm, list->ptr);
}

static bool	qcvm_heap_resolve_pointer(const qcvm_t *vm, void *handle, const size_t offset, const size_t len, void **address)
{
	qcvm_heap_t *list = (qcvm_heap_t *)handle;

	if ((offset + len) <= list->size)
	{
		if (address)
			*address = (uint8_t *)list->ptr + offset;
		return true;
	}
	return false;
}

static const qcvm_handle_descriptor_t list_descriptor =
{
	.free = qcvm_heap_free,
	.resolve_pointer = qcvm_heap_resolve_pointer
};

static void QC_heap_alloc(qcvm_t *vm)
{
	const int32_t size = qcvm_argv_int32(vm, 0);

	if (size < 0)
		qcvm_error(vm, "bad heap size");

	qcvm_heap_t *list = (qcvm_heap_t *)qcvm_alloc(vm, sizeof(qcvm_heap_t));

	list->ptr = qcvm_alloc(vm, size);
	list->size = size;

	int32_t handle = qcvm_handle_alloc(vm, list, &list_descriptor);
	
	qcvm_return_pointer(vm, (qcvm_pointer_t) { .handle = { .type = QCVM_POINTER_HANDLE, .index = handle, .offset = 0 } });
}

void qcvm_init_heap_builtins(qcvm_t *vm)
{
	qcvm_register_builtin(heap_alloc);
}