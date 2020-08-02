#include "shared/shared.h"
#include "game.h"
#include "g_vm.h"

static char *qcvm_buffer = nullptr;
static size_t qcvm_allocated_length = 0;

char *qcvm_temp_buffer(const qcvm_t *vm, const size_t len, char **old_buffer)
{
	if (old_buffer)
		*old_buffer = nullptr;

	if (len > qcvm_allocated_length)
	{
		qcvm_allocated_length = len;
		if (qcvm_buffer)
		{
			if (old_buffer)
				*old_buffer = qcvm_buffer;
			else
				qcvm_mem_free(vm, qcvm_buffer);
		}
		qcvm_debug(vm, "Temp buffer expanded to %u\n", qcvm_allocated_length);
		qcvm_buffer = (char *)qcvm_alloc(vm, qcvm_allocated_length + 1);
	}

	return qcvm_buffer;
}

size_t qcvm_temp_buffer_size(const qcvm_t *vm)
{
	return qcvm_allocated_length;
}

/*
============
va

does a varargs printf into a temp buffer, so I don't need to have
varargs versions of all text functions.

Does not use rotating buffers like other va implementations! It does
ensure that a temp buffer used as an argument properly renders, though
============
*/
char *qcvm_temp_format(const qcvm_t *vm, const char *format, ...)
{
	va_list	argptr;

	va_start(argptr, format);
	char *buffer = qcvm_temp_buffer(vm, MAX_INFO_STRING, nullptr);
	const size_t allocated_length = qcvm_temp_buffer_size(vm);
	const size_t needed_to_write = vsnprintf(buffer, allocated_length, format, argptr) + 1;

	if (needed_to_write > allocated_length)
	{
		char *old_buffer;
		buffer = qcvm_temp_buffer(vm, allocated_length + 1, &old_buffer);
		vsnprintf(buffer, allocated_length, format, argptr);
		qcvm_mem_free(vm, old_buffer);
	}
	va_end(argptr);

	return buffer;
}

static void QC_va(qcvm_t *vm)
{
	const string_t fmtid = qcvm_argv_string_id(vm, 0);
	qcvm_return_string(vm, qcvm_parse_format(fmtid, vm, 1));
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

	length = minsz(str_len - start, length);

	char *buffer = qcvm_temp_buffer(vm, length, nullptr);
	strncpy(buffer, qcvm_get_string(vm, strid) + start, length);
	buffer[length] = 0;
	qcvm_return_string(vm, buffer);
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

	const char *str = "";

	for (int32_t i = 0; i < vm->state.argc; i++)
		str = qcvm_temp_format(vm, "%s%s", str, qcvm_argv_string(vm, i));

	qcvm_return_string(vm, str);
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

#include <time.h>

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