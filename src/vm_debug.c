#include "shared/shared.h"
#include "g_vm.h"
#include "vm_game.h"
#include "vm_debug.h"
#include "vm_string.h"

static void QC_stacktrace(qcvm_t *vm)
{
	vm->debug_print(qcvm_stack_trace(vm));
}

static void QC_debugbreak(qcvm_t *vm)
{
#ifdef ALLOW_DEBUGGING
	qcvm_break_on_current_statement(vm);
#endif
}

static void QC_dumpentity(qcvm_t *vm)
{
	FILE *fp = fopen(qcvm_temp_format(vm, "%sdumpentity.text", vm->path), "a");
	edict_t *ent = qcvm_argv_entity(vm, 0);
	
	for (qcvm_definition_t *f = vm->fields; f < vm->fields + vm->fields_size; f++)
	{
		fprintf(fp, "%s: ", qcvm_get_string(vm, f->name_index));

		const ptrdiff_t val = (ptrdiff_t)(qcvm_resolve_pointer(vm, qcvm_get_entity_field_pointer(vm, ent, (int32_t)f->global_index)));

		switch (f->id)
		{
		case TYPE_FLOAT:
			fprintf(fp, "%f", *(vec_t *)val);
			break;
		case TYPE_VECTOR: {
			vec_t *v = (vec_t *)val;
			fprintf(fp, "%f %f %f", v[0], v[1], v[2]);
			break; }
		default:
			fprintf(fp, "%i", *(int32_t *)val);
			break;
		}

		fprintf(fp, "\r\n");
	}

	fclose(fp);
}

void qcvm_init_debug_builtins(qcvm_t *vm)
{
	qcvm_register_builtin(stacktrace);
	qcvm_register_builtin(debugbreak);
	qcvm_register_builtin(dumpentity);
}

#ifdef ALLOW_DEBUGGING
static size_t debuggerwnd;
static char *debugger_command;
static qcvm_mutex_t input_mutex;
static bool awaiting_steal = false;
static qcvm_thread_t input_thread;
static bool running_thread = false;
static qcvm_t *thread_vm;

static void qcvm_debugger_thread()
{
	static char debug_string[MAX_INFO_STRING * 4];
	static size_t debug_pos = 0;

	debugger_command = debug_string;

	while (true)
	{
		while (true)
		{
			char c = getc(stdin);

			if (c == 0)
				return;

			if (debug_pos >= sizeof(debug_string))
			{
				debug_pos = 0;
				debug_string[0] = 0;
				thread_vm->debug_print("DEBUGGER STRING OVERFLOW :(");
			}

			debug_string[debug_pos] = c;
		
			if (debug_string[debug_pos] == '\n')
			{
				debug_string[debug_pos] = 0;
				break;
			}

			debug_pos++;
		}

		thread_vm->debug.lock_mutex(input_mutex);
		debug_pos = 0;
		awaiting_steal = true;
		thread_vm->debug.unlock_mutex(input_mutex);

		while (true)
		{
			thread_vm->debug.thread_sleep(10);
			
			thread_vm->debug.lock_mutex(input_mutex);
			bool can_break = !awaiting_steal;
			thread_vm->debug.unlock_mutex(input_mutex);

			if (can_break)
				break;
		}
	}
}

void qcvm_wait_for_debugger_commands(qcvm_t *vm);

void qcvm_init_debugger(qcvm_t *vm)
{
	thread_vm = vm;
	input_mutex = thread_vm->debug.create_mutex();
	thread_vm->debug.thread_sleep(5000);

	input_thread = thread_vm->debug.create_thread(qcvm_debugger_thread);
	running_thread = true;
	vm->debug.state = DEBUG_BROKE;

	qcvm_wait_for_debugger_commands(vm);
}


void qcvm_send_debugger_command(const qcvm_t *vm, const char *cmd)
{
	printf("%s\n", cmd);
	fflush(stdout);
}

static const char *strtok_emulate(qcvm_t *vm, qcvm_function_t *func, const char *string, int *start)
{
	qcvm_set_global_str(vm, GLOBAL_PARM0, string);
	qcvm_set_global_typed_ptr(int32_t, vm, GLOBAL_PARM1, start);
	qcvm_execute(vm, func);

	*start = *qcvm_get_global_typed(int32_t, vm, GLOBAL_PARM1);
	return qcvm_get_string(vm, *qcvm_get_global_typed(qcvm_string_t, vm, GLOBAL_RETURN));
}

void qcvm_check_debugger_commands(qcvm_t *vm)
{
	if (!thread_vm)
		return;
	
	thread_vm->debug.lock_mutex(input_mutex);

	if (!awaiting_steal)
	{
		thread_vm->debug.unlock_mutex(input_mutex);
		return;
	}

	while (*debugger_command && debugger_command[strlen(debugger_command) - 1] == '\n')
		debugger_command[strlen(debugger_command) - 1] = 0;

	qcvm_function_t *qc_strtok = qcvm_find_function(vm, "strtok");

	if (!qc_strtok)
		qcvm_error(vm, "Can't find strtok :(");
	else if (strncmp(debugger_command, "debuggerwnd ", 12) == 0)
	{
		vm->debug.attached = true;
		debuggerwnd = strtoul(debugger_command + 12, NULL, 0);
		qcvm_send_debugger_command(vm, qcvm_temp_format(vm, "qcreloaded \"%s\" \"%s\"\n", vm->engine_name, vm->path));
	}
	else if (strncmp(debugger_command, "qcbreakpoint ", 13) == 0)
	{
		int start = 13;
		int mode = strtol(strtok_emulate(vm, qc_strtok, debugger_command, &start), NULL, 10);
		const char *file = strtok_emulate(vm, qc_strtok, debugger_command, &start);
		int line = strtol(strtok_emulate(vm, qc_strtok, debugger_command, &start), NULL, 10);

		qcvm_set_breakpoint(vm, mode, file, line);
	}
	else if (strcmp(debugger_command, "qcresume") == 0)
	{
		vm->debug.state = DEBUG_NONE;
	}
	else if (strncmp(debugger_command, "qcstep ", 7) == 0)
	{
		int start = 7;
		const char *mode = strtok_emulate(vm, qc_strtok, debugger_command, &start);

		if (!strcmp(mode, "into"))
			vm->debug.state = DEBUG_STEP_INTO;
		else if (!strcmp(mode, "out"))
			vm->debug.state = DEBUG_STEP_OUT;
		else
			vm->debug.state = DEBUG_STEP_OVER;
	}
	else if (strncmp(debugger_command, "qcinspect ", 10) == 0)
	{
		int start = 10;
		const char *variable = strtok_emulate(vm, qc_strtok, debugger_command, &start);

		qcvm_eval_result_t result = qcvm_evaluate(vm, variable);
		const char *value;
		char *slashed = NULL;
		
		switch (result.type)
		{
		case TYPE_INTEGER:
			value = qcvm_temp_format(vm, "%i", result.integer);
			break;
		case TYPE_FLOAT:
			value = qcvm_temp_format(vm, "%g", result.single);
			break;
		case TYPE_VECTOR:
			value = qcvm_temp_format(vm, "\"%f %f %f\"", result.vector.x, result.vector.y, result.vector.z);
			break;
		case TYPE_STRING:
			value = qcvm_get_string(vm, result.strid);

			if (strchr(value, '"') || strchr(value, '\\'))
			{
				size_t newlen = strlen(value);

				for (const char *c = value; *c; c++)
					if (*c == '"' || *c == '\\')
						newlen++;

				slashed = (char *)qcvm_alloc(vm, newlen + 2 + 1);
				*slashed = '"';
				strcpy(slashed + 1, value);

				const char *end = slashed + newlen + 1;
				char *c;

				for (c = slashed + 1; *c; c++)
				{
					if (*c != '"' && *c != '\\')
						continue;

					memmove(c + 1, c, end - c);
					*c = '\\';
					c++;
				}

				*c++ = '"';
				*c = 0;

				value = slashed;
			}
			break;
		case TYPE_ENTITY:
			if (result.entid == ENT_INVALID)
				value = "\"invalid/null entity\"";
			else
				value = qcvm_temp_format(vm, "\"entity %i\"", qcvm_ent_to_entity(vm, result.entid, false)->s.number);
			break;
		case TYPE_FUNCTION:
			if (result.funcid == FUNC_VOID)
				value = "\"invalid/null function\"";
			else
			{
				qcvm_function_t *func = qcvm_get_function(vm, result.funcid);

				if (!func || func->name_index == STRING_EMPTY)
					value = qcvm_temp_format(vm, "\"can't resolve function: %i\"", result.funcid);
				else
					value = qcvm_temp_format(vm, "%s", qcvm_get_string(vm, func->name_index));
			}
			break;
		default:
			value = "\"unable to evaluate\"";
			break;
		}

		qcvm_send_debugger_command(vm, qcvm_temp_format(vm, "qcvalue \"%s\" %s\n", variable, value));

		if (slashed)
			qcvm_mem_free(vm, slashed);
	}

	awaiting_steal = false;
	thread_vm->debug.unlock_mutex(input_mutex);
}

void qcvm_wait_for_debugger_commands(qcvm_t *vm)
{
	while (vm->debug.state == DEBUG_BROKE)
	{
		qcvm_check_debugger_commands(vm);
		vm->debug.thread_sleep(1);
	}
}
#endif