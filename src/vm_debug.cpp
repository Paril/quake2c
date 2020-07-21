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

static void QC_dumpentity(QCVM &vm)
{
	std::filesystem::path file_path(vas("%s/dumpentity.text", game_var->string).data());
	std::ofstream stream(file_path, std::ios::binary);

	auto ent = vm.ArgvEntity(0);

	for (auto f : vm.fields)
	{
		stream << vm.GetString(f.name_index) << ": ";

		auto val = reinterpret_cast<ptrdiff_t>(vm.GetEntityFieldPointer(*ent, static_cast<int32_t>(f.global_index)));

		switch (f.id)
		{
		case TYPE_FLOAT:
			stream << *reinterpret_cast<vec_t *>(val);
			break;
		case TYPE_VECTOR:
			stream << vtoss(*reinterpret_cast<vec3_t *>(val));
			break;
		default:
			stream << *reinterpret_cast<int32_t *>(val);
			break;
		}

		stream << "\r\n";
	}
}

void InitDebugBuiltins(QCVM &vm)
{
	RegisterBuiltin(traceon);
	RegisterBuiltin(traceoff);
	RegisterBuiltin(stacktrace);
	RegisterBuiltin(debugbreak);
	RegisterBuiltin(dumpentity);
}