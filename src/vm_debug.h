#pragma once

#ifdef ALLOW_DEBUGGING
qcvm_eval_result_t qcvm_evaluate(qcvm_t *vm, const char *variable);
void qcvm_break_on_current_statement(qcvm_t *vm);
void qcvm_set_breakpoint(qcvm_t *vm, const bool is_set, const char *file, const int line);
void qcvm_check_debugger_commands(qcvm_t *vm);
void qcvm_send_debugger_command(const qcvm_t *vm, const char *cmd);
void qcvm_wait_for_debugger_commands(qcvm_t *vm);
#endif

void qcvm_init_debug_builtins(qcvm_t *vm);