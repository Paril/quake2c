#include "shared/shared.h"
#include "game.h"
#include "g_vm.h"

static void QC_va(QCVM &vm)
{
	const auto &fmt = vm.ArgvString(0);
	vm.Return(vm.dynamic_strings.StoreStatic(va("%s", ParseFormat(fmt, vm, 1).data())));
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
		vm.SetGlobal<int32_t>(global_t::PARM1, -1);
		return;
	}

	const char *ptr = str + offset;
	const char *parsed = COM_Parse(&ptr);
	vm.Return(vm.dynamic_strings.StoreStatic(parsed));
	vm.SetGlobal<int32_t>(global_t::PARM1, (ptr == nullptr) ? -1 : (ptr - str));
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

	static char substr_buffer[2048];

	length = min(sizeof(substr_buffer) - start - 1, length);

	strncpy(substr_buffer, str + start, length);
	substr_buffer[length] = 0;

	vm.Return(vm.dynamic_strings.StoreStatic(substr_buffer));
}

static void QC_Info_ValueForKey(QCVM &vm)
{
	const auto &userinfo = vm.ArgvString(0);
	const auto &key = vm.ArgvString(1);

	vm.Return(vm.dynamic_strings.StoreStatic(Info_ValueForKey(userinfo, key)));
}

static void QC_Info_SetValueForKey(QCVM &vm)
{
	const auto &userinfo = vm.ArgvString(0);
	const auto &key = vm.ArgvString(1);
	const auto &value = vm.ArgvString(2);

	vm.Return(Info_SetValueForKey(const_cast<char *>(userinfo), key, value));
}

static void QC_dynstring_acquire(QCVM &vm)
{
	const auto &v = vm.ArgvStringID(0);
	vm.dynamic_strings.AcquireRefCounted(v);
}

static void QC_dynstring_release(QCVM &vm)
{
	const auto &v = vm.ArgvStringID(0);
	vm.dynamic_strings.ReleaseRefCounted(v);
}

static void QC_dynstring_create(QCVM &vm)
{
	const auto &v = vm.ArgvString(0);
	std::string str = v;
	vm.Return(vm.dynamic_strings.StoreRefCounted(v));
}

static void QC_dynstring_alloc(QCVM &vm)
{
	const auto &size = vm.ArgvInt32(0);
	std::string str;
	str.reserve(size);
	vm.Return(vm.dynamic_strings.StoreRefCounted(std::move(str)));
}

static void QC_dynstring_append(QCVM &vm)
{
	const auto &appendee = vm.ArgvStringID(0);
	const auto &appender = vm.ArgvString(1);

	vm.dynamic_strings.GetDynamic(appendee).append(appender);
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

	RegisterBuiltin(Info_ValueForKey);
	RegisterBuiltin(Info_SetValueForKey);
	
	RegisterBuiltin(dynstring_acquire);
	RegisterBuiltin(dynstring_release);
	RegisterBuiltin(dynstring_create);
	RegisterBuiltin(dynstring_alloc);
	RegisterBuiltin(dynstring_append);
}