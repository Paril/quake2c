#include "shared/shared.h"
#include "vm.h"

static void QC_memcpy(qcvm_t *vm)
{
	const qcvm_pointer_t dst = qcvm_argv_pointer(vm, 0);
	const qcvm_pointer_t src = qcvm_argv_pointer(vm, 1);
	const int32_t size = qcvm_argv_int32(vm, 2);
	void *dst_address, *src_address;

	if (size < 0 || !qcvm_resolve_pointer(vm, dst, false, size, &dst_address) || !qcvm_resolve_pointer(vm, src, false, size, &src_address))
		qcvm_error(vm, "invalid pointer");
	else if (!size)
		return;

	memcpy(dst_address, src_address, size);
	
	const size_t span = size / sizeof(qcvm_global_t);
	qcvm_string_list_mark_refs_copied(vm, src_address, dst_address, span);
	qcvm_field_wrap_list_check_set(vm, dst_address, span);
}

static void QC_memmove(qcvm_t *vm)
{
	const qcvm_pointer_t dst = qcvm_argv_pointer(vm, 0);
	const qcvm_pointer_t src = qcvm_argv_pointer(vm, 1);
	const int32_t size = qcvm_argv_int32(vm, 2);
	void *dst_address, *src_address;

	if (size < 0 || !qcvm_resolve_pointer(vm, dst, false, size, &dst_address) || !qcvm_resolve_pointer(vm, src, false, size, &src_address))
		qcvm_error(vm, "invalid pointer");
	else if (!size)
		return;

	memmove(dst_address, src_address, size);
	
	const size_t span = size / sizeof(qcvm_global_t);
	qcvm_string_list_mark_refs_copied(vm, src_address, dst_address, span);
	qcvm_field_wrap_list_check_set(vm, dst_address, span);
}

static void QC_memset(qcvm_t *vm)
{
	const qcvm_pointer_t dst = qcvm_argv_pointer(vm, 0);
	const int32_t val = qcvm_argv_int32(vm, 1);
	const int32_t size = qcvm_argv_int32(vm, 2);
	void *dst_address;
	
	if (size < 0 || !qcvm_resolve_pointer(vm, dst, false, size, &dst_address))
		qcvm_error(vm, "invalid pointer");
	else if (!size)
		return;

	memset(dst_address, val, size);
	
	const size_t span = size / sizeof(qcvm_global_t);
	qcvm_string_list_check_ref_unset(vm, dst_address, span, true);
	qcvm_field_wrap_list_check_set(vm, dst_address, span);
}

static void QC_memcmp(qcvm_t *vm)
{
	const qcvm_pointer_t dst = qcvm_argv_pointer(vm, 0);
	const qcvm_pointer_t src = qcvm_argv_pointer(vm, 1);
	const int32_t size = qcvm_argv_int32(vm, 2);
	void *dst_address, *src_address;

	if (size < 0 || !qcvm_resolve_pointer(vm, dst, false, size, &dst_address) || !qcvm_resolve_pointer(vm, src, false, size, &src_address))
		qcvm_error(vm, "invalid pointer");
	
	qcvm_return_int32(vm, memcmp(dst_address, src_address, size));
}

void qcvm_init_mem_builtins(qcvm_t *vm)
{
	qcvm_register_builtin(memcpy);
	qcvm_register_builtin(memmove);
	qcvm_register_builtin(memset);
	qcvm_register_builtin(memcmp);
}