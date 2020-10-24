#include "shared/shared.h"
#include "vm.h"
#include "vm_string.h"
#include "vm_file.h"
#include "g_file.h"

#include "game.h"

#ifndef KMQUAKE2_ENGINE_MOD
typedef size_t fileHandle_t;

typedef enum
{
	FS_READ,
	FS_WRITE,
	FS_APPEND
} fsMode_t;

static FILE *OpenFile(qcvm_t *vm, const char *name, const fsMode_t mode)
{
	return fopen(qcvm_temp_format(vm, "%s%s", vm->path, name), mode == FS_READ ? "rb" : mode == FS_APPEND ? "ab" : "wb");
}
#endif

static void QC_LoadFile(qcvm_t *vm)
{
	const char *name = qcvm_argv_string(vm, 0);
	void *buffer = NULL;
	int32_t len = 0;

#ifdef KMQUAKE2_ENGINE_MOD
	len = gi.LoadFile((char *)name, &buffer);
#else
	FILE *handle = OpenFile(vm, name, FS_READ);

	if (handle)
	{
		fseek(handle, 0, SEEK_END);
		len = ftell(handle);
		fseek(handle, 0, SEEK_SET);
		buffer = qcvm_alloc(vm, len);
		fread(buffer, sizeof(char), len, handle);
		fclose(handle);
	}
	else
		len = -1;
#endif

	if (len >= 1)
		qcvm_set_global_str(vm, GLOBAL_PARM1, (const char *)buffer, len, true);
	else
		qcvm_set_global_str(vm, GLOBAL_PARM1, "", 0, false);
	qcvm_return_int32(vm, len);

#ifdef KMQUAKE2_ENGINE_MOD
	if (buffer)
		gi.FreeFile(buffer);
#else
	if (buffer)
		qcvm_mem_free(vm, buffer);
#endif
}

static void CloseFile(qcvm_t *vm, void *ptr)
{
#ifdef KMQUAKE2_ENGINE_MOD
	gi.CloseFile((fileHandle_t)ptr);
#else
	fclose((FILE *)ptr);
#endif
}

static qcvm_handle_descriptor_t fileHandle_descriptor =
{
	.free = CloseFile
};

static void QC_OpenFile(qcvm_t *vm)
{
	const char *name = qcvm_argv_string(vm, 0);
	const fsMode_t mode = qcvm_argv_int32(vm, 2);
	int32_t len;
	int32_t qhandle;

#ifdef KMQUAKE2_ENGINE_MOD
	fileHandle_t handle;
	len = gi.OpenFile(name, &handle, mode);

	if (len != -1)
		qhandle = qcvm_handle_alloc(vm, (void *)(size_t)handle, &fileHandle_descriptor);
#else
	FILE *handle = OpenFile(vm, name, mode);

	if (handle)
	{
		fseek(handle, 0, SEEK_END);
		len = ftell(handle);
		fseek(handle, 0, SEEK_SET);
		qhandle = qcvm_handle_alloc(vm, handle, &fileHandle_descriptor);
	}
	else
		len = -1;
#endif

	if (len != -1)
		qcvm_set_global_typed_value(int32_t, vm, GLOBAL_PARM1, qhandle);
	qcvm_return_int32(vm, len);
}

static void QC_OpenCompressedFile(qcvm_t *vm)
{
#ifdef KMQUAKE2_ENGINE_MOD
	const char *zipName = qcvm_argv_string(vm, 0);
	const char *name = qcvm_argv_string(vm, 1);
	const fsMode_t mode = qcvm_argv_int32(vm, 3);
	int32_t len;
	fileHandle_t handle;

	len = gi.OpenCompressedFile(zipName, name, &handle, mode);

	if (len != -1)
	{
		int32_t qhandle = qcvm_handle_alloc(vm, (void *)(size_t)handle, &fileHandle_descriptor);
		qcvm_set_global_typed_value(fileHandle_t, vm, GLOBAL_PARM1, qhandle);
	}

	qcvm_return_int32(vm, len);
#else
	qcvm_return_int32(vm, -1);
#endif
}

static void QC_FRead(qcvm_t *vm)
{
	const qcvm_pointer_t pointer = qcvm_argv_pointer(vm, 0);
	const int32_t size = qcvm_argv_int32(vm, 1);
	const fileHandle_t *handle = qcvm_argv_handle(fileHandle_t, vm, 2);
	int32_t written;
	void *address;

	if (!qcvm_resolve_pointer(vm, pointer, false, size, &address))
		qcvm_error(vm, "bad pointer");

#ifdef KMQUAKE2_ENGINE_MOD
	written = gi.FRead(address, size, (fileHandle_t)handle);
#else
	written = fread(address, size, 1, (FILE *)handle);
#endif

	qcvm_return_int32(vm, written);
}

static void QC_FWrite(qcvm_t *vm)
{
	const qcvm_pointer_t pointer = qcvm_argv_pointer(vm, 0);
	const int32_t size = qcvm_argv_int32(vm, 1);
	const fileHandle_t *handle = qcvm_argv_handle(fileHandle_t, vm, 2);
	int32_t written;
	void *address;

	if (!qcvm_resolve_pointer(vm, pointer, false, size, &address))
		qcvm_error(vm, "bad pointer");

#ifdef KMQUAKE2_ENGINE_MOD
	written = gi.FWrite(address, size, (fileHandle_t)handle);
#else
	written = fwrite(address, size, 1, (FILE *)handle);
#endif

	qcvm_return_int32(vm, written);
}

static void QC_CreatePath(qcvm_t *vm)
{
	const char *name = qcvm_argv_string(vm, 0);

	name = qcvm_cpp_absolute_path(vm, name);

#ifdef KMQUAKE2_ENGINE_MOD
	gi.CreatePath((char *)name);
#else
	mkdir(name);
#endif
}

static void QC_GameDir(qcvm_t *vm)
{
#ifdef KMQUAKE2_ENGINE_MOD
	qcvm_return_string(vm, gi.FS_GameDir());
#else
	const cvar_t *game_var = gi.cvar("game", "", 0);
	qcvm_return_string(vm, qcvm_temp_format(vm, "./%s", *game_var->string ? game_var->string : BASEDIR));
#endif
}

static void QC_SaveGameDir(qcvm_t *vm)
{
#ifdef KMQUAKE2_ENGINE_MOD
	qcvm_return_string(vm, gi.FS_SaveGameDir());
#else
	QC_GameDir(vm);
#endif
}

typedef struct
{
	char	**entries;
	int32_t	num;
} qc_file_list_t;

static void QC_file_list_get(qcvm_t *vm)
{
	const qc_file_list_t *list = qcvm_argv_handle(qc_file_list_t, vm, 0);
	const int32_t index = qcvm_argv_int32(vm, 1);
	
	if (index < 0 || index >= list->num)
		qcvm_error(vm, "overrun file list");

	qcvm_return_string(vm, list->entries[index]);
}

static void QC_file_list_length(qcvm_t *vm)
{
	const qc_file_list_t *list = qcvm_argv_handle(qc_file_list_t, vm, 0);
	qcvm_return_int32(vm, list->num);
}

static void QC_file_list_free(qcvm_t *vm, void *ptr)
{
	qc_file_list_t *list = (qc_file_list_t *)ptr;

#ifdef KMQUAKE2_ENGINE_MOD
	gi.FreeFileList(list->entries, list->num);
#else
	for (int32_t i = 0; i < list->num; i++)
		qcvm_mem_free(vm, list->entries[i]);
#endif

	qcvm_mem_free(vm, list);
}

static qcvm_handle_descriptor_t file_list_descriptor =
{
	.free = QC_file_list_free
};

static void QC_GetFileList(qcvm_t *vm)
{
	const char *path = qcvm_argv_string(vm, 0);
	const char *ext = qcvm_argv_string(vm, 1);
	qc_file_list_t *list = (qc_file_list_t *)qcvm_alloc(vm, sizeof(qc_file_list_t));

#ifdef KMQUAKE2_ENGINE_MOD
	list->entries = gi.GetFileList(path, ext, &list->num);
#else
	list->entries = qcvm_get_file_list(vm, path, ext, &list->num);
#endif

	qcvm_return_handle(vm, list, &file_list_descriptor);
}

void qcvm_init_file_builtins(qcvm_t *vm)
{
	qcvm_register_builtin(LoadFile);
	qcvm_register_builtin(OpenFile);
	qcvm_register_builtin(OpenCompressedFile);
	qcvm_register_builtin(FRead);
	qcvm_register_builtin(FWrite);
	qcvm_register_builtin(CreatePath);
	qcvm_register_builtin(GameDir);
	qcvm_register_builtin(SaveGameDir);
	qcvm_register_builtin(GetFileList);
	qcvm_register_builtin(file_list_get);
	qcvm_register_builtin(file_list_length);
}