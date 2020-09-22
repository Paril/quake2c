#pragma once

char *qcvm_temp_buffer(const qcvm_t *vm, const size_t len);
char *qcvm_temp_format(const qcvm_t *vm, const char *format, ...);

void qcvm_init_string_builtins(qcvm_t *vm);