#include "shared/shared.h"
#include "game.h"
#include "g_vm.h"
#include "vm_string.h"

enum { NUM_ROTATING_BUFFERS = 4 };

typedef struct 
{
	char	*buffer;
	size_t	length;
} qcvm_temp_buffer_t;

static qcvm_temp_buffer_t qcvm_buffers[NUM_ROTATING_BUFFERS];
static int32_t buffer_index = 0;

char *qcvm_temp_buffer(const qcvm_t *vm, const size_t len)
{
	qcvm_temp_buffer_t *buf = &qcvm_buffers[buffer_index];
	buffer_index = (buffer_index + 1) % NUM_ROTATING_BUFFERS;

	if (len > buf->length)
	{
		buf->length = len;
		if (buf->buffer)
			qcvm_mem_free(vm, buf->buffer);
		qcvm_debug(vm, "Temp buffer %i expanded to %u\n", buffer_index, buf->length);
		buf->buffer = (char *)qcvm_alloc(vm, buf->length + 1);
	}

	return buf->buffer;
}

/*
============
va

does a varargs printf into a temp buffer, so I don't need to have
varargs versions of all text functions.
============
*/
char *qcvm_temp_format(const qcvm_t *vm, const char *format, ...)
{
	va_list	argptr;

	va_start(argptr, format);
	const int32_t my_index = buffer_index;
	qcvm_temp_buffer_t *buf = &qcvm_buffers[my_index];
	char *buffer = qcvm_temp_buffer(vm, MAX_INFO_STRING);
	const size_t needed_to_write = vsnprintf(buffer, buf->length, format, argptr) + 1;

	if (needed_to_write > buf->length)
	{
		buffer_index = my_index;
		buffer = qcvm_temp_buffer(vm, needed_to_write - 1);
		vsnprintf(buffer, needed_to_write, format, argptr);
	}
	va_end(argptr);
	return buffer;
}

static void QC_va(qcvm_t *vm)
{
	const qcvm_string_t fmtid = qcvm_argv_string_id(vm, 0);
	qcvm_return_string(vm, qcvm_parse_format(fmtid, vm, 1));
}

static void QC_stoi(qcvm_t *vm)
{
	const char *a = qcvm_argv_string(vm, 0);
	qcvm_return_int32(vm, strtol(a, NULL, 10));
}

static void QC_stof(qcvm_t *vm)
{
	const char *a = qcvm_argv_string(vm, 0);
	qcvm_return_float(vm, strtof(a, NULL));
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
	const qcvm_string_t a = qcvm_argv_string_id(vm, 0);
	qcvm_return_int32(vm, qcvm_get_string_length(vm, a));
}

static void QC_substr(qcvm_t *vm)
{
	const qcvm_string_t strid = qcvm_argv_string_id(vm, 0);
	const int32_t start = qcvm_argv_int32(vm, 1);
	const size_t str_len = qcvm_get_string_length(vm, strid);
	size_t length = SIZE_MAX;

	if (start < 0 || start >= str_len)
		qcvm_error(vm, "invalid start to substr");

	if (vm->state.argc >= 3)
		length = qcvm_argv_int32(vm, 2);

	length = minsz(str_len - start, length);

	char *buffer = qcvm_temp_buffer(vm, length);
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

	qcvm_return_int32(vm, c == NULL ? -1 : (c - a));
}

static void QC_strchr(qcvm_t *vm)
{
	const char *a = qcvm_argv_string(vm, 0);
	const int32_t b = qcvm_argv_int32(vm, 1);
	const char *c = strchr(a, b);
	
	qcvm_return_int32(vm, c == NULL ? -1 : (c - a));
}

#include <time.h>

static void QC_localtime(qcvm_t *vm)
{
	static struct tm empty_ltime;
	time_t gmtime = time(NULL);
	const struct tm *ltime = localtime(&gmtime);

	if (!ltime)
		ltime = &empty_ltime;

	qcvm_set_global_typed_ptr(struct tm, vm, GLOBAL_PARM0, ltime);
}

void qcvm_init_string_builtins(qcvm_t *vm)
{
	qcvm_register_builtin(va);
	
	qcvm_register_builtin(stoi);
	qcvm_register_builtin(stof);

	qcvm_register_builtin(stricmp);
	qcvm_register_builtin(strlen);
	qcvm_register_builtin(substr);
	qcvm_register_builtin(strncmp);
	qcvm_register_builtin(strconcat);
	qcvm_register_builtin(strstr);
	qcvm_register_builtin(strchr);

	qcvm_register_builtin(localtime);
}