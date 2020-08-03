#include "shared/shared.h"
#include "game.h"
#include "g_vm.h"

static void QC_ModInt(qcvm_t *vm)
{
	const int a = qcvm_argv_int32(vm, 0);
	const int b = qcvm_argv_int32(vm, 1);

	qcvm_return_int32(vm, a % b);
}

static void QC_func_get(qcvm_t *vm)
{
	const char *s = qcvm_argv_string(vm, 0);
	qcvm_func_t func = qcvm_find_function_id(vm, s);
	qcvm_return_func(vm, func);
}

void qcvm_init_ext_builtins(qcvm_t *vm)
{
	qcvm_register_builtin(ModInt);
	qcvm_register_builtin(func_get);
}