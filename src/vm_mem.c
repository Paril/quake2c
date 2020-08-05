#include "shared/shared.h"
#include "game.h"
#include "g_vm.h"

static void QC_memcpy(qcvm_t *vm)
{
	const qcvm_pointer_t dst = qcvm_argv_pointer(vm, 0);
	const qcvm_pointer_t src = qcvm_argv_pointer(vm, 1);
	const int32_t size = qcvm_argv_int32(vm, 2);
	const size_t span = size / sizeof(qcvm_global_t);

	if (!qcvm_pointer_valid(vm, dst, false, span) || !qcvm_pointer_valid(vm, src, false, span))
		qcvm_error(vm, "invalid pointer");
	
	void *dst_ptr = qcvm_resolve_pointer(vm, dst);
	const void *src_ptr = qcvm_resolve_pointer(vm, src);

	memcpy(dst_ptr, src_ptr, size);

	qcvm_string_list_mark_refs_copied(&vm->dynamic_strings, src_ptr, dst_ptr, span);
}

static void QC_memclear(qcvm_t *vm)
{
	const qcvm_pointer_t dst = qcvm_argv_pointer(vm, 0);
	const int32_t size = qcvm_argv_int32(vm, 1);
	const size_t span = size / sizeof(qcvm_global_t);
	
	if (!qcvm_pointer_valid(vm, dst, false, span))
		qcvm_error(vm, "invalid pointer");

	void *dst_ptr = qcvm_resolve_pointer(vm, dst);

	memset(dst_ptr, 0, size);

	qcvm_string_list_check_ref_unset(&vm->dynamic_strings, dst_ptr, span, false);
}

static void QC_memcmp(qcvm_t *vm)
{
	const qcvm_pointer_t dst = qcvm_argv_pointer(vm, 0);
	const qcvm_pointer_t src = qcvm_argv_pointer(vm, 1);
	const int32_t size = qcvm_argv_int32(vm, 2);
	const size_t span = size / sizeof(qcvm_global_t);

	if (!qcvm_pointer_valid(vm, dst, false, span) || !qcvm_pointer_valid(vm, src, false, span))
		qcvm_error(vm, "invalid pointer");
	
	void *dst_ptr = qcvm_resolve_pointer(vm, dst);
	const void *src_ptr = qcvm_resolve_pointer(vm, src);

	qcvm_return_int32(vm, memcmp(dst_ptr, src_ptr, size));
}

void qcvm_init_mem_builtins(qcvm_t *vm)
{
	qcvm_register_builtin(memcpy);
	qcvm_register_builtin(memclear);
	qcvm_register_builtin(memcmp);
}