#include "shared/shared.h"
#include "game.h"
#include "g_vm.h"

static void QC_stacktrace(QCVM &vm)
{
	gi.dprintf("%s\n", vm.StackTrace().data());
}

static void QC_debugbreak(QCVM &vm)
{
	__debugbreak();
}

static void QC_dumpentity(QCVM &vm)
{
	FILE *fp = fopen(va("%s/dumpentity.text", game_var->string), "a");
	edict_t *ent = vm.ArgvEntity(0);
	
	for (QCDefinition *f = vm.fields; f < vm.fields + vm.fields_size; f++)
	{
		fprintf(fp, "%s: ", vm.GetString(f->name_index));

		const size_t val = (size_t)vm.GetEntityFieldPointer(ent, (int32_t)f->global_index);

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
}

void InitDebugBuiltins(QCVM &vm)
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

static void DebuggerThread()
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

void WaitForDebuggerCommands();

#include <chrono>

using namespace std::chrono_literals;

void InitDebugger()
{
	if (!*gi.cvar("qc_debugger", "", CVAR_NONE)->string)
		return;

	std::this_thread::sleep_for(5s);

	input_thread = std::thread(DebuggerThread);
	running_thread = true;
	qvm.debug.state = DEBUG_BROKE;

	WaitForDebuggerCommands();
}

/*
==============
strtok

Parse a token out of a string.
Handles C and C++ comments.
==============
*/
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

void SendDebuggerCommand(const std::string &cmd)
{
	gi.dprintf("TO DEBUGGER: %s\n", cmd.data());
	std::cout << cmd << '\n';
	std::cout.flush();
}

#include <regex>

void CheckDebuggerCommands()
{
	if (!running_thread)
		InitDebugger();

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
		debuggerwnd = strtoul(s.data() + 12, NULL, 0);
		SendDebuggerCommand(vas("qcreloaded \"%s\" \"%s\"\n", "QuakeII", "progs.dat"));
	}
	else if (s.starts_with("qcbreakpoint "))
	{
		int start = 13;
		int mode = strtol(strtok(s, start).data(), NULL, 10);
		std::string file = strtok(s, start);
		int line = strtol(strtok(s, start).data(), NULL, 10);

		qvm.SetBreakpoint(mode, file, line);
	}
	else if (s == "qcresume")
	{
		qvm.debug.state = DEBUG_NONE;
	}
	else if (s.starts_with("qcstep "))
	{
		int start = 7;
		std::string mode = strtok(s, start);

		if (mode == "into")
			qvm.debug.state = DEBUG_STEP_INTO;
		else if (mode == "out")
			qvm.debug.state = DEBUG_STEP_OUT;
		else
			qvm.debug.state = DEBUG_STEP_OVER;
	}
	else if (s.starts_with("qcinspect "))
	{
		int start = 10;
		std::string variable = strtok(s, start);

		evaluate_result_t result = qvm.Evaluate(variable);
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
			value = vas("\"%f %f %f\"", result.vector[0], result.vector[1], result.vector[2]);
			break;
		case TYPE_STRING:
			value = qvm.GetString(result.strid);

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
				value = vas("\"entity %i\"", qvm.EntToEntity(result.entid)->s.number);
			break;
		case TYPE_FUNCTION:
			if (result.funcid == FUNC_VOID)
				value = "\"invalid/null function\"";
			else
			{
				QCFunction *func = qvm.FindFunction(result.funcid);

				if (!func || func->name_index == STRING_EMPTY)
					value = vas("\"can't resolve function: %i\"", result.funcid);
				else
					value = vas("%s", qvm.GetString(func->name_index));
			}
			break;
		default:
			value = "\"unable to evaluate\"";
			break;
		}

		SendDebuggerCommand(vas("qcvalue \"%s\" %s\n", variable.data(), value.data()));
	}
}

void WaitForDebuggerCommands()
{
	while (qvm.debug.state == DEBUG_BROKE)
	{
		CheckDebuggerCommands();
		std::this_thread::sleep_for(1ms);
	}
}
#endif