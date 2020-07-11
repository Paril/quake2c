#include "shared/shared.h"
#include "game.h"
#include "g_vm.h"

static void QC_BitorInt(QCVM &vm)
{
	const auto &a = vm.ArgvInt32(0);
	const auto &b = vm.ArgvInt32(1);

	vm.Return(a | b);
}

static void QC_BitandInt(QCVM &vm)
{
	const auto &a = vm.ArgvInt32(0);
	const auto &b = vm.ArgvInt32(1);

	vm.Return(a & b);
}

static void QC_AddInt(QCVM &vm)
{
	const auto &a = vm.ArgvInt32(0);
	const auto &b = vm.ArgvInt32(1);

	vm.Return(a + b);
}

static void QC_SubInt(QCVM &vm)
{
	const auto &a = vm.ArgvInt32(0);
	const auto &b = vm.ArgvInt32(1);

	vm.Return(a - b);
}

static void QC_MulInt(QCVM &vm)
{
	const auto &a = vm.ArgvInt32(0);
	const auto &b = vm.ArgvInt32(1);

	vm.Return(a * b);
}

static void QC_DivInt(QCVM &vm)
{
	const auto &a = vm.ArgvInt32(0);
	const auto &b = vm.ArgvInt32(1);

	vm.Return(a / b);
}

static void QC_ModInt(QCVM &vm)
{
	const auto &a = vm.ArgvInt32(0);
	const auto &b = vm.ArgvInt32(1);

	vm.Return(a % b);
}

static void QC_BitxorInt(QCVM &vm)
{
	const auto &a = vm.ArgvInt32(0);
	const auto &b = vm.ArgvInt32(1);

	vm.Return(a ^ b);
}

static void QC_TwosCompInt(QCVM &vm)
{
	const auto &a = vm.ArgvInt32(0);

	vm.Return(~a);
}

static void QC_LShiftInt(QCVM &vm)
{
	const auto &a = vm.ArgvInt32(0);
	const auto &b = vm.ArgvInt32(1);

	vm.Return(a << b);
}

static void QC_RShiftInt(QCVM &vm)
{
	const auto &a = vm.ArgvInt32(0);
	const auto &b = vm.ArgvInt32(1);

	vm.Return(a >> b);
}

static void QC_itof(QCVM &vm)
{
	const auto &a = vm.ArgvInt32(0);

	vm.Return(static_cast<float>(a));
}

static void QC_ftoi(QCVM &vm)
{
	const auto &a = vm.ArgvFloat(0);

	vm.Return(static_cast<int32_t>(a));
}

static void QC_func_get(QCVM &vm)
{
	const auto &s = vm.ArgvString(0);
	auto func = vm.FindFunctionID(s);

	if (func == func_t::FUNC_VOID)
		vm.Return(func_t::FUNC_VOID);
	else
		vm.Return(func);
}

void InitExtBuiltins(QCVM &vm)
{
	RegisterBuiltin(BitorInt);
	RegisterBuiltin(BitandInt);
	RegisterBuiltin(BitxorInt);
	RegisterBuiltin(AddInt);
	RegisterBuiltin(SubInt);
	RegisterBuiltin(MulInt);
	RegisterBuiltin(DivInt);
	RegisterBuiltin(ModInt);
	RegisterBuiltin(TwosCompInt);
	RegisterBuiltin(LShiftInt);
	RegisterBuiltin(RShiftInt);
	RegisterBuiltin(itof);
	RegisterBuiltin(ftoi);
	RegisterBuiltin(func_get);
}