#pragma once

const char *qcvm_cpp_absolute_path(const qcvm_t *vm, const char *relative);

char **qcvm_get_file_list(const qcvm_t *vm, const char *path, const char *ext, int32_t *num);
