#include "shared/shared.h"
#include "game.h"
#include "g_vm.h"

static void QC_traceon(QCVM &vm)
{
	vm.EnableTrace();
}

static void QC_traceoff(QCVM &vm)
{
	vm.StopTrace();
}

static void QC_stacktrace(QCVM &vm)
{
	const auto &str = vm.ArgvString(0);
	gi.dprintf("%s\n%s\n", str, vm.StackTrace().data());
}

static void QC_debugbreak(QCVM &vm)
{
	__debugbreak();
}

void InitDebugBuiltins(QCVM &vm)
{
	RegisterBuiltin(traceon);
	RegisterBuiltin(traceoff);
	RegisterBuiltin(stacktrace);
	RegisterBuiltin(debugbreak);
}