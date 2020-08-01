#include "shared/shared.h"
#include "game.h"
#include "g_vm.h"

static void QC_va(qcvm_t *vm)
{
	const string_t fmtid = qcvm_argv_string_id(vm, 0);
	qcvm_return_string(vm, ParseFormat(fmtid, vm, 1).data());
}

static void QC_stoi(qcvm_t *vm)
{
	const char *a = qcvm_argv_string(vm, 0);
	qcvm_return_int32(vm, strtol(a, nullptr, 10));
}

static void QC_stof(qcvm_t *vm)
{
	const char *a = qcvm_argv_string(vm, 0);
	qcvm_return_float(vm, strtof(a, nullptr));
}

static void QC_stricmp(qcvm_t *vm)
{
	const char *a = qcvm_argv_string(vm, 0);
	const char *b = qcvm_argv_string(vm, 1);
	qcvm_return_int32(vm, stricmp(a, b));
}

static void QC_strncmp(qcvm_t *vm)
{
	const char *a = qcvm_argv_string(vm, 0);
	const char *b = qcvm_argv_string(vm, 1);
	const int32_t c = qcvm_argv_int32(vm, 2);

	qcvm_return_int32(vm, strncmp(a, b, c));
}

static void QC_strlen(qcvm_t *vm)
{
	const string_t a = qcvm_argv_string_id(vm, 0);
	qcvm_return_int32(vm, qcvm_get_string_length(vm, a));
}

static void QC_substr(qcvm_t *vm)
{
	const string_t strid = qcvm_argv_string_id(vm, 0);
	const int32_t start = qcvm_argv_int32(vm, 1);
	const size_t str_len = qcvm_get_string_length(vm, strid);
	size_t length = SIZE_MAX;

	if (start < 0 || start >= str_len)
		qcvm_error(vm, "invalid start to substr");

	if (vm->state.argc >= 3)
		length = qcvm_argv_int32(vm, 2);

	length = min(str_len - start, length);

	qcvm_return_string(vm, std::string(qcvm_get_string(vm, strid) + start, length).data());
}

static void QC_strconcat(qcvm_t *vm)
{
	if (vm->state.argc == 0)
	{
		qcvm_return_string_id(vm, STRING_EMPTY);
		return;
	}
	else if (vm->state.argc == 1)
	{
		qcvm_return_string_id(vm, qcvm_argv_string_id(vm, 0));
		return;
	}

	std::string str;

	for (int32_t i = 0; i < vm->state.argc; i++)
		str += qcvm_argv_string(vm, i);

	qcvm_return_string(vm, str.data());
}

static void QC_strstr(qcvm_t *vm)
{
	const char *a = qcvm_argv_string(vm, 0);
	const char *b = qcvm_argv_string(vm, 1);
	const char *c = strstr(a, b);

	qcvm_return_int32(vm, c == nullptr ? -1 : (c - a));
}

static void QC_strchr(qcvm_t *vm)
{
	const char *a = qcvm_argv_string(vm, 0);
	const int32_t b = qcvm_argv_int32(vm, 1);
	const char *c = strchr(a, b);
	
	qcvm_return_int32(vm, c == nullptr ? -1 : (c - a));
}

#include <ctime>

static void QC_localtime(qcvm_t *vm)
{
	static tm empty_ltime;
	time_t gmtime = time(NULL);
	const tm *ltime = localtime(&gmtime);

	if (!ltime)
		ltime = &empty_ltime;

	qcvm_set_global_typed_ptr(tm, vm, GLOBAL_PARM0, ltime);
}

void InitStringBuiltins(qcvm_t *vm)
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