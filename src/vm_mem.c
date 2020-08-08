#include "shared/shared.h"
#include "g_vm.h"

static void QC_memcpy(qcvm_t *vm)
{
	const qcvm_pointer_t dst = qcvm_argv_pointer(vm, 0);
	const qcvm_pointer_t src = qcvm_argv_pointer(vm, 1);
	const int32_t size = qcvm_argv_int32(vm, 2);
	const size_t span = size / sizeof(qcvm_global_t);

	if (!qcvm_pointer_valid(vm, dst, false, size) || !qcvm_pointer_valid(vm, src, false, size))
		qcvm_error(vm, "invalid pointer");
	
	void *dst_ptr = qcvm_resolve_pointer(vm, dst);
	const void *src_ptr = qcvm_resolve_pointer(vm, src);

	memcpy(dst_ptr, src_ptr, size);

	qcvm_string_list_mark_refs_copied(&vm->dynamic_strings, src_ptr, dst_ptr, span);
}

static void QC_memmove(qcvm_t *vm)
{
	const qcvm_pointer_t dst = qcvm_argv_pointer(vm, 0);
	const qcvm_pointer_t src = qcvm_argv_pointer(vm, 1);
	const int32_t size = qcvm_argv_int32(vm, 2);
	const size_t span = size / sizeof(qcvm_global_t);

	if (!qcvm_pointer_valid(vm, dst, false, size) || !qcvm_pointer_valid(vm, src, false, size))
		qcvm_error(vm, "invalid pointer");
	
	void *dst_ptr = qcvm_resolve_pointer(vm, dst);
	const void *src_ptr = qcvm_resolve_pointer(vm, src);

	memmove(dst_ptr, src_ptr, size);

	qcvm_string_list_mark_refs_copied(&vm->dynamic_strings, src_ptr, dst_ptr, span);
}

static void QC_memset(qcvm_t *vm)
{
	const qcvm_pointer_t dst = qcvm_argv_pointer(vm, 0);
	const int32_t val = qcvm_argv_int32(vm, 1);
	const int32_t size = qcvm_argv_int32(vm, 2);
	const size_t span = size / sizeof(qcvm_global_t);
	
	if (!qcvm_pointer_valid(vm, dst, false, size))
		qcvm_error(vm, "invalid pointer");

	void *dst_ptr = qcvm_resolve_pointer(vm, dst);

	memset(dst_ptr, val, size);

	qcvm_string_list_check_ref_unset(&vm->dynamic_strings, dst_ptr, span, true);
}

static void QC_memcmp(qcvm_t *vm)
{
	const qcvm_pointer_t dst = qcvm_argv_pointer(vm, 0);
	const qcvm_pointer_t src = qcvm_argv_pointer(vm, 1);
	const int32_t size = qcvm_argv_int32(vm, 2);

	if (!qcvm_pointer_valid(vm, dst, false, size) || !qcvm_pointer_valid(vm, src, false, size))
		qcvm_error(vm, "invalid pointer");
	
	void *dst_ptr = qcvm_resolve_pointer(vm, dst);
	const void *src_ptr = qcvm_resolve_pointer(vm, src);

	qcvm_return_int32(vm, memcmp(dst_ptr, src_ptr, size));
}

void qcvm_init_mem_builtins(qcvm_t *vm)
{
	qcvm_register_builtin(memcpy);
	qcvm_register_builtin(memmove);
	qcvm_register_builtin(memset);
	qcvm_register_builtin(memcmp);
}