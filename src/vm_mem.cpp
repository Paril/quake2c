#include "shared/shared.h"
#include "game.h"
#include "g_vm.h"

static void QC_memcpy(qcvm_t *vm)
{
	const int32_t dst = qcvm_argv_int32(vm, 0);
	const int32_t src = qcvm_argv_int32(vm, 1);
	const int32_t sz = qcvm_argv_int32(vm, 2);
	
	void *dst_ptr = qcvm_address_to_entity_field(dst);
	const void *src_ptr = qcvm_address_to_entity_field(src);

	memcpy(dst_ptr, src_ptr, sz);

	const size_t span = sz / sizeof(global_t);

	qcvm_string_list_mark_refs_copied(&vm->dynamic_strings, src_ptr, dst_ptr, span);
}

static void QC_memclear(qcvm_t *vm)
{
	const int32_t dst = qcvm_argv_int32(vm, 0);
	const int32_t sz = qcvm_argv_int32(vm, 1);
	
	void *dst_ptr = qcvm_address_to_entity_field(dst);

	memset(dst_ptr, 0, sz);

	qcvm_string_list_check_ref_unset(&vm->dynamic_strings, dst_ptr, sz / sizeof(global_t), false);
}

static void QC_memcmp(qcvm_t *vm)
{
	const int32_t src = qcvm_argv_int32(vm, 0);
	const int32_t dst = qcvm_argv_int32(vm, 1);
	const int32_t sz = qcvm_argv_int32(vm, 2);
	
	const void *src_ptr = qcvm_address_to_entity_field(src);
	const void *dst_ptr = qcvm_address_to_entity_field(dst);

	qcvm_return_int32(vm, memcmp(dst_ptr, src_ptr, sz));
}

void InitMemBuiltins(qcvm_t *vm)
{
	RegisterBuiltin(memcpy);
	RegisterBuiltin(memclear);
	RegisterBuiltin(memcmp);
}