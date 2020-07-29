#include "shared/shared.h"
#include "game.h"
#include "g_vm.h"

static void QC_memcpy(QCVM &vm)
{
	const int32_t dst = vm.ArgvInt32(0);
	const int32_t src = vm.ArgvInt32(1);
	const int32_t sz = vm.ArgvInt32(2);
	
	void *dst_ptr = vm.AddressToEntityField(dst);
	const void *src_ptr = vm.AddressToEntityField(src);

	memcpy(dst_ptr, src_ptr, sz);

	// unref any strings that were in dst
	vm.dynamic_strings.CheckRefUnset(dst_ptr, sz / sizeof(global_t));

	// grab list of fields that have strings
	std::unordered_map<string_t, size_t> ids;

	if (!vm.dynamic_strings.HasRef(src_ptr, sz / sizeof(global_t), ids))
		return;

	// mark them as being inside of src as well now
	for (auto &s : ids)
		vm.dynamic_strings.MarkRefCopy(s.first, (global_t *)(dst_ptr) + s.second);
}

static void QC_memclear(QCVM &vm)
{
	const int32_t dst = vm.ArgvInt32(0);
	const int32_t sz = vm.ArgvInt32(1);
	
	void *dst_ptr = vm.AddressToEntityField(dst);

	memset(dst_ptr, 0, sz);

	vm.dynamic_strings.CheckRefUnset(dst_ptr, sz / sizeof(global_t));
}

static void QC_memcmp(QCVM &vm)
{
	const int32_t src = vm.ArgvInt32(0);
	const int32_t dst = vm.ArgvInt32(1);
	const int32_t sz = vm.ArgvInt32(2);
	
	const void *src_ptr = vm.AddressToEntityField(src);
	const void *dst_ptr = vm.AddressToEntityField(dst);

	vm.ReturnInt(memcmp(dst_ptr, src_ptr, sz));
}

void InitMemBuiltins(QCVM &vm)
{
	RegisterBuiltin(memcpy);
	RegisterBuiltin(memclear);
	RegisterBuiltin(memcmp);
}