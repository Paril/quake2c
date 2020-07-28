#include "shared/shared.h"
#include "game.h"
#include "g_vm.h"

static void QC_va(QCVM &vm)
{
	const auto &fmtid = vm.ArgvStringID(0);
	vm.ReturnString(ParseFormat(fmtid, vm, 1));
}

static void QC_stoi(QCVM &vm)
{
	const auto &a = vm.ArgvString(0);
	vm.ReturnInt(strtol(a, nullptr, 10));
}

static void QC_stof(QCVM &vm)
{
	const auto &a = vm.ArgvString(0);
	vm.ReturnFloat(strtof(a, nullptr));
}

static void QC_stricmp(QCVM &vm)
{
	const auto &a = vm.ArgvString(0);
	const auto &b = vm.ArgvString(1);
	vm.ReturnInt(stricmp(a, b));
}

static void QC_strncmp(QCVM &vm)
{
	const auto &a = vm.ArgvString(0);
	const auto &b = vm.ArgvString(1);
	const auto &c = vm.ArgvInt32(2);

	vm.ReturnInt(strncmp(a, b, c));
}

static void QC_strlen(QCVM &vm)
{
	const auto &a = vm.ArgvStringID(0);
	vm.ReturnInt(static_cast<int32_t>(vm.StringLength(a)));
}

static void QC_substr(QCVM &vm)
{
	const auto &strid = vm.ArgvStringID(0);
	const auto &start = vm.ArgvInt32(1);
	const size_t str_len = vm.StringLength(strid);
	size_t length = SIZE_MAX;

	if (start < 0 || start >= str_len)
		vm.Error("invalid start to substr");

	if (vm.state.argc >= 3)
		length = vm.ArgvInt32(2);

	length = min(str_len - start, length);

	vm.ReturnString(std::string(vm.GetString(strid) + start, length));
}

static void QC_strconcat(QCVM &vm)
{
	if (vm.state.argc == 0)
	{
		vm.ReturnString(vm.string_data.data());
		return;
	}
	else if (vm.state.argc == 1)
	{
		vm.ReturnString(vm.ArgvStringID(0));
		return;
	}

	std::string str;

	for (int32_t i = 0; i < vm.state.argc; i++)
		str += vm.ArgvString(i);

	vm.ReturnString(std::move(str));
}

static void QC_strstr(QCVM &vm)
{
	const auto &a = vm.ArgvString(0);
	const auto &b = vm.ArgvString(1);
	const char *c = strstr(a, b);

	vm.ReturnInt(c == nullptr ? -1 : (c - a));
}

static void QC_strchr(QCVM &vm)
{
	const auto &a = vm.ArgvString(0);
	const auto &b = vm.ArgvInt32(1);
	const char *c = strchr(a, b);
	
	vm.ReturnInt(c == nullptr ? -1 : (c - a));
}

#include <ctime>

static void QC_localtime(QCVM &vm)
{
	static tm empty_ltime;
	time_t gmtime = time(NULL);
	const tm *ltime = localtime(&gmtime);

	if (!ltime)
		ltime = &empty_ltime;

	vm.SetGlobal(GLOBAL_PARM0, *ltime);
}

void InitStringBuiltins(QCVM &vm)
{
	RegisterBuiltin(va);
	
	RegisterBuiltin(stoi);
	RegisterBuiltin(stof);

	RegisterBuiltin(stricmp);
	RegisterBuiltin(strlen);
	RegisterBuiltin(substr);
	RegisterBuiltin(strncmp);
	RegisterBuiltin(strconcat);
	RegisterBuiltin(strstr);
	RegisterBuiltin(strchr);

	RegisterBuiltin(localtime);
}