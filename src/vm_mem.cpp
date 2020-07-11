#include "shared/shared.h"
#include "game.h"
#include "g_vm.h"

static void QC_memcpy(QCVM &vm)
{
	const auto &dst = vm.ArgvInt32(0);
	const auto &src = vm.ArgvInt32(1);
	const auto &sz = vm.ArgvInt32(2);
	
	auto dst_ptr = vm.AddressToEntityField(dst);
	auto src_ptr = vm.AddressToEntityField(src);

	memcpy(dst_ptr, src_ptr, sz);
}

static void QC_memset(QCVM &vm)
{
	const auto &dst = vm.ArgvInt32(0);
	const auto &val = vm.ArgvInt32(1);
	const auto &sz = vm.ArgvInt32(2);
	
	auto dst_ptr = vm.AddressToEntityField(dst);

	memset(dst_ptr, val, sz);
}

static void QC_struct_copy_to(QCVM &vm)
{
	const auto &src = vm.ArgvInt32(0);
	const auto &dst = vm.params_from.at(global_t::PARM1);
	const auto &sz = vm.ArgvInt32(2);
	
	auto src_ptr = vm.AddressToEntityField(src);
	auto dst_ptr = const_cast<uint8_t *>(&vm.GetGlobal<uint8_t>(dst));

	memcpy(dst_ptr, src_ptr, sz);
}

static void QC_struct_copy_from(QCVM &vm)
{
	const auto &src = vm.params_from.at(global_t::PARM0);
	const auto &dst = vm.ArgvInt32(1);
	const auto &sz = vm.ArgvInt32(2);
	
	auto src_ptr = &vm.GetGlobal<uint8_t>(src);
	auto dst_ptr = vm.AddressToEntityField(dst);

	memcpy(dst_ptr, src_ptr, sz);
}

static void QC_struct_cmp_to(QCVM &vm)
{
	const auto &src = vm.ArgvInt32(0);
	const auto &dst = vm.params_from.at(global_t::PARM1);
	const auto &sz = vm.ArgvInt32(2);
	
	auto src_ptr = vm.AddressToEntityField(src);
	auto dst_ptr = &vm.GetGlobal<uint8_t>(dst);

	vm.Return(memcmp(dst_ptr, src_ptr, sz));
}

static void QC_struct_clear(QCVM &vm)
{
	const auto &dst = vm.params_from.at(global_t::PARM0);
	const auto &sz = vm.ArgvInt32(1);
	
	auto dst_ptr = const_cast<uint8_t *>(&vm.GetGlobal<uint8_t>(dst));

	memset(dst_ptr, 0, sz);
}

void InitMemBuiltins(QCVM &vm)
{
	RegisterBuiltin(memcpy);
	RegisterBuiltin(memset);
	RegisterBuiltin(struct_copy_to);
	RegisterBuiltin(struct_copy_from);
	RegisterBuiltin(struct_cmp_to);
	RegisterBuiltin(struct_clear);
}