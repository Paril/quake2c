#include "shared/shared.h"
#include "game.h"
#include "g_vm.h"

static void QC_ModInt(QCVM &vm)
{
	const int a = vm.ArgvInt32(0);
	const int b = vm.ArgvInt32(1);

	vm.ReturnInt(a % b);
}

static void QC_func_get(QCVM &vm)
{
	const char *s = vm.ArgvString(0);
	func_t func = vm.FindFunctionID(s);
	vm.ReturnFunc(func);
}

void InitExtBuiltins(QCVM &vm)
{
	RegisterBuiltin(ModInt);
	RegisterBuiltin(func_get);
}