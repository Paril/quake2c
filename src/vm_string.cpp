#include "shared/shared.h"
#include "game.h"
#include "g_vm.h"

static void QC_va(QCVM &vm)
{
	const auto &fmt = vm.ArgvString(0);
	vm.Return(ParseFormat(fmt, vm, 1));
}

static void QC_itos(QCVM &vm)
{
	const auto &a = vm.ArgvInt32(0);
	static char buffer[65];
	Q_snprintf(buffer, sizeof(buffer), "%i", a);
	vm.Return(vm.dynamic_strings.StoreStatic(buffer));
}

static void QC_stoi(QCVM &vm)
{
	const auto &a = vm.ArgvString(0);
	vm.Return(strtol(a, nullptr, 10));
}

static void QC_ftos(QCVM &vm)
{
	const auto &a = vm.ArgvFloat(0);
	static char buffer[129];
	Q_snprintf(buffer, sizeof(buffer), "%f", a);
	vm.Return(vm.dynamic_strings.StoreStatic(buffer));
}

static void QC_stof(QCVM &vm)
{
	const auto &a = vm.ArgvString(0);
	vm.Return(strtof(a, nullptr));
}

static void QC_stricmp(QCVM &vm)
{
	const auto &a = vm.ArgvString(0);
	const auto &b = vm.ArgvString(1);
	vm.Return(Q_stricmp(a, b));
}

static void QC_strncmp(QCVM &vm)
{
	const auto &a = vm.ArgvString(0);
	const auto &b = vm.ArgvString(1);
	const auto &c = vm.ArgvInt32(2);

	vm.Return(strncmp(a, b, c));
}

static void QC_strlen(QCVM &vm)
{
	const auto &a = vm.ArgvStringID(0);

	vm.Return(static_cast<int32_t>(vm.dynamic_strings.Length(a)));
}

static void QC_strtok(QCVM &vm)
{
	const auto &str = vm.ArgvString(0);
	const auto &offset = vm.ArgvInt32(1);

	if (offset == -1)
	{
		vm.Return(vm.string_data);
		vm.SetGlobal(global_t::PARM1, -1);
		return;
	}

	const char *ptr = str + offset;
	const char *parsed = COM_Parse(&ptr);
	vm.Return(std::string(parsed));
	vm.SetGlobal(global_t::PARM1, (ptr == nullptr) ? -1 : (ptr - str));
}

static void QC_strat(QCVM &vm)
{
	const auto &str = vm.ArgvString(0);
	const auto &offset = vm.ArgvInt32(1);

	vm.Return(str[offset]);
}

static void QC_substr(QCVM &vm)
{
	const auto &str = vm.ArgvString(0);
	const auto &start = vm.ArgvInt32(1);
	const size_t str_len = strlen(str);
	size_t length = SIZE_MAX;

	if (start < 0 || start >= str_len)
		vm.Error("invalid start to substr");

	if (vm.state.argc >= 3)
		length = vm.ArgvInt32(2);

	length = min(str_len - start, length);

	vm.Return(std::string(str + start, length));
}

static void QC_strconcat(QCVM &vm)
{
	if (vm.state.argc == 0)
	{
		vm.Return(vm.string_data);
		return;
	}
	else if (vm.state.argc == 1)
	{
		vm.Return(vm.ArgvStringID(0));
		return;
	}

	std::string str;

	for (int32_t i = 0; i < vm.state.argc; i++)
		str += vm.ArgvString(i);

	vm.Return(str);
}

static void QC_Info_ValueForKey(QCVM &vm)
{
	const auto &userinfo = vm.ArgvString(0);
	const auto &key = vm.ArgvString(1);

	vm.Return(std::string(Info_ValueForKey(userinfo, key)));
}

static void QC_Info_SetValueForKey(QCVM &vm)
{
	const auto &userinfo = vm.ArgvString(0);
	const auto &key = vm.ArgvString(1);
	const auto &value = vm.ArgvString(2);

	vm.Return(Info_SetValueForKey(const_cast<char *>(userinfo), key, value));
}

void InitStringBuiltins(QCVM &vm)
{
	RegisterBuiltin(va);
	
	RegisterBuiltin(ftos);
	RegisterBuiltin(itos);
	RegisterBuiltin(stoi);
	RegisterBuiltin(stof);

	RegisterBuiltin(stricmp);
	RegisterBuiltin(strlen);
	RegisterBuiltin(strtok);
	RegisterBuiltin(strat);
	RegisterBuiltin(substr);
	RegisterBuiltin(strncmp);
	RegisterBuiltin(strconcat);

	RegisterBuiltin(Info_ValueForKey);
	RegisterBuiltin(Info_SetValueForKey);
}