#include "shared/shared.h"
#include "game.h"
#include "g_vm.h"

static void QC_stacktrace(qcvm_t *vm)
{
	gi.dprintf("%s\n", qcvm_stack_trace(vm).data());
}

static void QC_debugbreak(qcvm_t *vm)
{
	qcvm_break_on_current_statement(vm);
}

static void QC_dumpentity(qcvm_t *vm)
{
	FILE *fp = fopen(va("%sdumpentity.text", vm->path), "a");
	edict_t *ent = qcvm_argv_entity(vm, 0);
	
	for (QCDefinition *f = vm->fields; f < vm->fields + vm->fields_size; f++)
	{
		fprintf(fp, "%s: ", qcvm_get_string(vm, f->name_index));

		const size_t val = (size_t)qcvm_get_entity_field_pointer(ent, (int32_t)f->global_index);

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
	RegisterBuiltin(stacktrace);
	RegisterBuiltin(debugbreak);
	RegisterBuiltin(dumpentity);
}

#ifdef ALLOW_DEBUGGING
#include <iostream>
#include <thread>
#include <mutex>
#include <queue>

static size_t debuggerwnd;
static std::list<std::string> debugger_commands;
static std::mutex input_mutex;
static std::thread input_thread;
static bool running_thread = false;

static void qcvm_debugger_thread()
{
	std::string teststr;

	while (true)
	{
		while (true)
		{
			if (std::cin.peek() == EOF)
				return;

			char c = std::cin.get();
			teststr += c;
		
			if (c == '\n')
				break;
		}

		input_mutex.lock();
		gi.dprintf("FROM DEBUGGER: %s", teststr.data());
		debugger_commands.push_back(std::move(teststr));
		input_mutex.unlock();
	}
}

void qcvm_wait_for_debugger_commands(qcvm_t *vm);

#include <chrono>

using namespace std::chrono_literals;

static void qcvm_init_debugger(qcvm_t *vm)
{
	if (!*gi.cvar("qc_debugger", "", CVAR_NONE)->string)
		return;

	std::this_thread::sleep_for(5s);

	input_thread = std::thread(qcvm_debugger_thread);
	running_thread = true;
	vm->debug.state = DEBUG_BROKE;

	qcvm_wait_for_debugger_commands(vm);
}

/*
==============
strtok

Parse a token out of a string.
Handles C and C++ comments.
==============
*/
// FIXME: pull this from QC instead
std::string strtok(std::string data, int &start)
{
	int token_start = start, token_end;

	if (!data[0])
	{
		start = -1;
		return "";
	}

// skip whitespace
	int c;

skipwhite:
	while ((c = data[token_start]) <= ' ')
	{
		if (c == 0)
		{
			start = -1;
			return "";
		}
		token_start++;
	}

// skip // comments
	if (c == '/' && data[token_start + 1] == '/')
	{
		token_start += 2;

		while ((c = data[token_start]) && c != '\n')
			token_start++;
		
		goto skipwhite;
	}

// skip /* */ comments
	if (c == '/' && data[token_start + 1] == '*')
	{
		token_start += 2;
		while ((c = data[token_start]))
		{
			if (c == '*' && data[token_start + 1] == '/')
			{
				token_start += 2;
				break;
			}
			token_start++;
		}
		goto skipwhite;
	}

	token_end = token_start;

// handle quoted strings specially
	if (c == '\"')
	{
		token_end++;
		while ((c = data[token_end++]) && c != '\"') ;

		if (!data[token_end])
			start = -1;
		else
			start = token_end;

		return data.substr(token_start + 1, token_end - token_start - 2);
	}

// parse a regular word
	do
	{
		token_end++;
		c = data[token_end];
	} while (c > 32);

	if (!data[token_end])
		start = -1;
	else
		start = token_end;

	return data.substr(token_start, token_end - token_start);
}

void qcvm_send_debugger_command(const qcvm_t *vm, const char *cmd)
{
	gi.dprintf("TO DEBUGGER: %s\n", cmd);
	std::cout << cmd << '\n';
	std::cout.flush();
}

#include <regex>

void qcvm_check_debugger_commands(qcvm_t *vm)
{
	if (!running_thread)
		qcvm_init_debugger(vm);

	input_mutex.lock();

	if (debugger_commands.empty())
	{
		input_mutex.unlock();
		return;
	}

	std::string s = std::move(debugger_commands.front());
	while (s.length() && s.at(s.length() - 1) == '\n')
		s.pop_back();
	debugger_commands.pop_front();

	input_mutex.unlock();

	gi.dprintf("EXECUTING DEBUGGER CMD: %s\n", s.data());

	if (s.starts_with("debuggerwnd "))
	{
		vm->debug.attached = true;
		debuggerwnd = strtoul(s.data() + 12, NULL, 0);
		qcvm_send_debugger_command(vm, vas("qcreloaded \"%s\" \"%s\"\n", vm->engine_name, vm->path).data());
	}
	else if (s.starts_with("qcbreakpoint "))
	{
		int start = 13;
		int mode = strtol(strtok(s, start).data(), NULL, 10);
		std::string file = strtok(s, start);
		int line = strtol(strtok(s, start).data(), NULL, 10);

		qcvm_set_breakpoint(vm, mode, file.data(), line);
	}
	else if (s == "qcresume")
	{
		vm->debug.state = DEBUG_NONE;
	}
	else if (s.starts_with("qcstep "))
	{
		int start = 7;
		std::string mode = strtok(s, start);

		if (mode == "into")
			vm->debug.state = DEBUG_STEP_INTO;
		else if (mode == "out")
			vm->debug.state = DEBUG_STEP_OUT;
		else
			vm->debug.state = DEBUG_STEP_OVER;
	}
	else if (s.starts_with("qcinspect "))
	{
		int start = 10;
		std::string variable = strtok(s, start);

		evaluate_result_t result = qcvm_evaluate(vm, variable);
		std::string value;
		
		switch (result.type)
		{
		case TYPE_INTEGER:
			value = vas("%i", result.integer);
			break;
		case TYPE_FLOAT:
			value = vas("%g", result.single);
			break;
		case TYPE_VECTOR:
			value = vas("\"%f %f %f\"", result.vector.x, result.vector.y, result.vector.z);
			break;
		case TYPE_STRING:
			value = qcvm_get_string(vm, result.strid);

			if (value.find_first_of('\"', 0) != std::string::npos)
			{
				static std::regex slashes(R"(\\)", std::regex_constants::icase);
				value = std::regex_replace(value, slashes, R"(\\)");

				static std::regex quotes(R"(")", std::regex_constants::icase);
				value = std::regex_replace(value, quotes, R"(\\")");

				value = '"' + value + '"';
			}
			break;
		case TYPE_ENTITY:
			if (result.entid == ENT_INVALID)
				value = "\"invalid/null entity\"";
			else
				value = vas("\"entity %i\"", qcvm_ent_to_entity(result.entid, false)->s.number);
			break;
		case TYPE_FUNCTION:
			if (result.funcid == FUNC_VOID)
				value = "\"invalid/null function\"";
			else
			{
				QCFunction *func = qcvm_get_function(vm, result.funcid);

				if (!func || func->name_index == STRING_EMPTY)
					value = vas("\"can't resolve function: %i\"", result.funcid);
				else
					value = vas("%s", qcvm_get_string(vm, func->name_index));
			}
			break;
		default:
			value = "\"unable to evaluate\"";
			break;
		}

		qcvm_send_debugger_command(vm, vas("qcvalue \"%s\" %s\n", variable.data(), value.data()).data());
	}
}

void qcvm_wait_for_debugger_commands(qcvm_t *vm)
{
	while (vm->debug.state == DEBUG_BROKE)
	{
		qcvm_check_debugger_commands(vm);
		std::this_thread::sleep_for(1ms);
	}
}
#endif