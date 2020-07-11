#pragma once

//#define ALLOW_TRACING
//#define STACK_TRACING
#define ALLOW_PROFILING

#include <cassert>
#include <variant>
#include <string>
#include <stack>
#include <deque>
#include <unordered_map>
#include <filesystem>
#include <fstream>

enum class string_t
{
	STRING_EMPTY
};

// NOTE: in QC, the value 0 is used for the world.
// For compatibility sake, we also consider 0 to be world,
// but entities are direct memory addresses into globals.edicts.
// The value "1" is used to mean a null entity, which is required
// to differentiate things like groundentity that can be the world.
enum class ent_t
{
	ENT_WORLD = 0,
	ENT_INVALID = 1
};

enum class func_t
{
	FUNC_VOID
};

enum opcode_t : uint16_t
{
	OP_DONE,
	OP_MUL_F,
	OP_MUL_V,
	OP_MUL_FV,
	OP_MUL_VF,
	OP_DIV_F,
	OP_ADD_F,
	OP_ADD_V,
	OP_SUB_F,
	OP_SUB_V,
	
	OP_EQ_F,
	OP_EQ_V,
	OP_EQ_S,
	OP_EQ_E,
	OP_EQ_FNC,
	
	OP_NE_F,
	OP_NE_V,
	OP_NE_S,
	OP_NE_E,
	OP_NE_FNC,
	
	OP_LE,
	OP_GE,
	OP_LT,
	OP_GT,

	OP_LOAD_F,
	OP_LOAD_V,
	OP_LOAD_S,
	OP_LOAD_ENT,
	OP_LOAD_FLD,
	OP_LOAD_FNC,

	OP_ADDRESS,

	OP_STORE_F,
	OP_STORE_V,
	OP_STORE_S,
	OP_STORE_ENT,
	OP_STORE_FLD,
	OP_STORE_FNC,

	OP_STOREP_F,
	OP_STOREP_V,
	OP_STOREP_S,
	OP_STOREP_ENT,
	OP_STOREP_FLD,
	OP_STOREP_FNC,

	OP_RETURN,
	OP_NOT_F,
	OP_NOT_V,
	OP_NOT_S,
	OP_NOT_ENT,
	OP_NOT_FNC,
	OP_IF,
	OP_IFNOT,
	OP_CALL0,
	OP_CALL1,
	OP_CALL2,
	OP_CALL3,
	OP_CALL4,
	OP_CALL5,
	OP_CALL6,
	OP_CALL7,
	OP_CALL8,
	OP_STATE,
	OP_GOTO,
	OP_AND,
	OP_OR,
	
	OP_BITAND,
	OP_BITOR,

	// EXT
	OP_GLOBALADDRESS = 143
	// EXT
};

struct QCStatement
{
	opcode_t				opcode;
	std::array<uint16_t, 3>	args;
};

enum deftype_t : uint16_t
{
	TYPE_VOID,
	TYPE_STRING,
	TYPE_FLOAT,
	TYPE_VECTOR,
	TYPE_ENTITY,
	TYPE_FIELD,
	TYPE_FUNCTION,
	TYPE_POINTER,

	// EXT
	TYPE_INTEGER,
	TYPE_VARIANT,
	TYPE_STRUCT,
	TYPE_UNION,
	TYPE_ACCESSOR,
	TYPE_ENUM,
	TYPE_BOOLEAN,
	// EXT

	TYPE_GLOBAL		= bit(15)
};

struct QCDefinition
{
	deftype_t	id;
	uint16_t	global_index;
	string_t	name_index;
};

struct QCFunction
{
	int32_t					id;
	uint32_t				first_arg;
	uint32_t				num_args_and_locals;
	uint32_t				profile;
	string_t				name_index;
	uint32_t				file_index;
	int32_t					num_args;
	std::array<uint8_t, 8>	arg_sizes;
};

enum class global_t : uint32_t
{
	QC_NULL	= 0,
	RETURN	= 1,
	PARM0	= 4,		// leave 3 ofs for each parm to hold vectors
	PARM1	= 7,
	PARM2	= 10,
	PARM3	= 13,
	PARM4	= 16,
	PARM5	= 19,
	PARM6	= 22,
	PARM7	= 25,
	QC_OFS	= 28
};

#ifdef ALLOW_PROFILING
#include <chrono>

using clock_type = std::chrono::high_resolution_clock;

inline clock_type::time_point perf_time()
{
	return clock_type::now();
}

enum profile_type_t
{
	NumSelfCalls,
	NumInstructions,
	NumUnconditionalJumps,
	NumConditionalJumps,
	NumGlobalsFetched,
	NumGlobalsSet,
	NumFuncCalls,

	TotalProfileFields
};

constexpr const char *profile_type_names[TotalProfileFields] =
{
	"# Calls",
	"# Instructions",
	"# Unconditional Jumps",
	"# Conditional Jumps",
	"# Globals Fetched",
	"# Globals Set",
	"# Func Calls"
};

struct QCProfile
{
	//std::unordered_map<const QCStatement *, size_t>	called_from;
	clock_type::duration	total, call_into;
	size_t					fields[TotalProfileFields];
};

using profile_key = std::tuple<int, int>;
#endif

struct QCStack
{
	QCFunction								*function = nullptr;
	const QCStatement						*statement = nullptr;
	std::unordered_map<uint32_t, global_t>	locals;

#ifdef ALLOW_PROFILING
	QCProfile				*profile;
	clock_type::time_point	start;
	clock_type::duration	time_callees;
#endif
};

using QCBuiltin = void (*) (struct QCVM &vm);

struct QCVMRefCountString
{
	std::string		str;
	size_t			ref_count;
};

class QCVMStringList
{
	using StringType = std::variant<std::string, std::string_view, const char**, QCVMRefCountString>;

	// The string list maps IDs (one-indexed) to either a custom string object or
	// a string view that points to existing memory.
	std::unordered_map<string_t, StringType>	strings;

	// stores a list of free indices that were explicitly freed
	std::stack<string_t>	free_indices;

	// static strings mapped to string_t's
	std::unordered_map<const void *, string_t> constant_storage;

	string_t Allocate();

public:
	const decltype(strings) &GetStrings() const
	{
		return strings;
	}

	string_t StoreStatic(const char **resolver);

	string_t StoreStatic(const std::string_view &view);

	string_t StoreDynamic(const std::string &&str);

	string_t StoreRefCounted(const std::string &&str);

	void Unstore(const string_t &id);

	size_t Length(const string_t &id) const;

	std::string &GetDynamic(const string_t &id);

	const char *GetStatic(const string_t &id) const;

	std::string &GetRefCounted(const string_t &id);

	const char *Get(const string_t &id) const;

	void AcquireRefCounted(const string_t &id);

	void ReleaseRefCounted(const string_t &id);
};

class QCVMBuiltinList
{
	struct QCVM								&vm;
	std::unordered_map<func_t, QCBuiltin>	builtins;
	int32_t									next_id;

public:
	QCVMBuiltinList(struct QCVM &invm);

	void SetFirstID(const int32_t &id);

	bool IsRegistered(const func_t &func);

	QCBuiltin Get(const func_t &func);

	void Register(const char *name, QCBuiltin builtin);
};

using QCVMFieldWrapper = void(*)(uint8_t *out, const int32_t *in);

struct QCVMFieldWrap
{
	const QCDefinition *field;
	const size_t		client_offset;
	QCVMFieldWrapper	setter;
};

class QCVMFieldWrapList
{
	struct QCVM									&vm;
	std::unordered_map<int32_t, QCVMFieldWrap>	wraps;

public:
	QCVMFieldWrapList(struct QCVM &invm);

	void Register(const char *field_name, const size_t &field_offset, const size_t &client_offset, QCVMFieldWrapper setter);

	inline void WrapField(const edict_t &ent, const int32_t &field, const void *src)
	{
		if (!wraps.contains(field))
			return;

		const auto &wrap = wraps.at(field);

		if (wrap.setter)
			wrap.setter(reinterpret_cast<uint8_t *>(ent.client) + wrap.client_offset, reinterpret_cast<const int32_t *>(src));
		else
			*reinterpret_cast<int32_t *>(reinterpret_cast<uint8_t *>(ent.client) + wrap.client_offset) = *reinterpret_cast<const int32_t *>(src);
	}

	constexpr std::unordered_map<int32_t, QCVMFieldWrap> &GetFields()
	{
		return wraps;
	}
};

struct QCVM
{
	// loaded from progs.dat
	std::vector<QCDefinition>					definitions;
	std::vector<QCDefinition>					fields;
	std::vector<QCStatement>					statements;
	std::vector<QCFunction>						functions;
#ifdef ALLOW_PROFILING
	std::vector<QCProfile>						profile_data;
#endif
	global_t									*global_data = nullptr;
	char										*string_data = nullptr;
	size_t										string_size;
	QCVMStringList								dynamic_strings;
	QCVMBuiltinList								builtins;
	QCVMFieldWrapList							field_wraps;
	std::unordered_map<global_t, global_t>		params_from;
	std::vector<int>							linenumbers;
#ifdef STACK_TRACING
	std::string									stack_pos;
#endif

	QCVM();

	template<typename ...T>
	[[noreturn]] void Error(T ...args)
	{
		__debugbreak();
		gi.error(args...);
		exit(0);
	}

#ifdef ALLOW_TRACING
	bool										enable_tracing;
	FILE										*trace_file;

	inline void EnableTrace()
	{
		enable_tracing = true;
		trace_file = fopen(vas("%s/trace.txt", gi.cvar("game", "", 0)->string).data(), "w+");
		fprintf(trace_file, "TRACING ENABLED\n");
	}

	inline void StopTrace()
	{
		enable_tracing = false;
		fprintf(trace_file, "TRACING DONE\n");
		fclose(trace_file);
		trace_file = nullptr;
	}

	template<typename ...T>
	inline void PrintTrace(const char *format, T ...args)
	{
		if (!enable_tracing)
			return;

		for (size_t i = 0; i < state.stack.size() - 1; i++)
			fprintf(trace_file, " ");

		fprintf(trace_file, "[%s] %s\n", StackEntry(state.current).data(), vas(format, args...).data());
		fflush(trace_file);
	}

	template<>
	inline void PrintTrace(const char *data)
	{
		if (!enable_tracing)
			return;

		for (size_t i = 0; i < state.stack.size() - 1; i++)
			fprintf(trace_file, " ");

		fprintf(trace_file, "[%s] %s\n", StackEntry(state.current).data(), data);
		fflush(trace_file);
	}

	inline std::string TraceEntity(const edict_t *e)
	{
		/*if (e.v.netname)
			return vas("%s[#%i]", GetString(e.v.netname), e.s.number);
		else if (e.v.classname)
			return vas("%s[#%i]", GetString(e.v.classname), e.s.number);*/

		if (e == nullptr)
			return "[NULL]";
		else if (e == &game.entity(1024))
			return "[INVALID]";

		return vas("[#%i]", e->s.number);
	}

	inline std::string TraceField(const int32_t &g)
	{
		for (auto &f : fields)
			if (f.global_index == g)
				return vas("%s[%i]", GetString(f.name_index), g);

		return vas("%i", g);
	}

	inline std::string TraceFunction(const int32_t &g)
	{
		if (g < 0 || g >= functions.size())
			return vas("INVALID FUNCTION (%i)", g);

		auto &f = functions[g];

		if (f.id < 0)
			return vas("BUILTIN \"%s\"", GetString(f.name_index));
		
		return vas("\"%s\"", GetString(f.name_index));
	}

	inline std::string DumpGlobalValue(const global_t &g, const deftype_t &type)
	{
		global_t *global = GetGlobalByIndex(g);

		switch (type)
		{
		case TYPE_VOID:
			return vas("VOID %x (int value: %i)", global, *reinterpret_cast<int32_t *>(global));
		case TYPE_STRING:
			return vas("STRING \"%s\"", GetString(*reinterpret_cast<string_t *>(global)));
		case TYPE_FLOAT:
			return vas("FLOAT %f", *reinterpret_cast<vec_t *>(global));
		case TYPE_VECTOR:
			return vas("VECTOR %s", vtoss(*reinterpret_cast<vec3_t *>(global)).data());
		case TYPE_ENTITY:
			return vas("ENTITY %s", TraceEntity(EntToEntity(*reinterpret_cast<ent_t*>(global))).data());
		case TYPE_FIELD:
			return vas("FIELD %s", TraceField(*reinterpret_cast<int32_t *>(global)).data());
		case TYPE_FUNCTION:
			return vas("FUNCTION %s", TraceFunction(*reinterpret_cast<int32_t *>(global)).data());
		case TYPE_POINTER:
			return vas("POINTER %x", global);
		case TYPE_INTEGER:
			return vas("INTEGER %x", *reinterpret_cast<int32_t *>(global));
		default:
			return vas("DUNNO @ %i", g);
//			Error("wat");
		}
	}

	inline std::string TraceGlobal(const global_t &g, const deftype_t &type = TYPE_VOID)
	{
		if (g < global_t::QC_OFS)
		{
			static const char *global_names[] = {
				"null",
				"return_0",
				"return_1",
				"return_2",
				"parm0_0",
				"parm0_1",
				"parm0_2",
				"parm1_0",
				"parm1_1",
				"parm1_2",
				"parm2_0",
				"parm2_1",
				"parm2_2",
				"parm3_0",
				"parm3_1",
				"parm3_2",
				"parm4_0",
				"parm4_1",
				"parm4_2",
				"parm5_0",
				"parm5_1",
				"parm5_2",
				"parm6_0",
				"parm6_1",
				"parm6_2",
				"parm7_0",
				"parm7_1",
				"parm7_2"
			};

			return vas("%s[%i] (%s)", global_names[static_cast<int32_t>(g)], g, DumpGlobalValue(g, type).data());
		}

		for (auto &f : definitions)
			if (static_cast<global_t>(f.global_index) == g && (type == TYPE_VOID || (f.id & ~TYPE_GLOBAL) == type))
				return vas("%s[%i] (%s)", GetString(f.name_index), g, DumpGlobalValue(g, (type == TYPE_VOID) ? static_cast<deftype_t>(f.id & ~TYPE_GLOBAL) : type).data());

		return vas("[%i] (%s)", g, DumpGlobalValue(g, type).data());
	}
#else
	static constexpr bool enable_tracing = false;

	constexpr void EnableTrace() { }
	constexpr void StopTrace() { }
	template<typename ...T>
	constexpr void PrintTrace() { }

	// no-op them
#define PrintTrace(...)
#endif

	inline const global_t *GetGlobalByIndex(const global_t &g) const
	{
		return global_data + static_cast<uint32_t>(g);
	}

	inline global_t *GetGlobalByIndex(const global_t &g)
	{
		return global_data + static_cast<uint32_t>(g);
	}

	inline global_t GlobalOffset(const global_t &base, const int32_t &offset) const
	{
		return static_cast<global_t>(static_cast<int32_t>(base) + offset);
	}

	template<typename T>
	[[nodiscard]] inline const T &GetGlobal(const global_t &global) const
	{
#ifdef ALLOW_PROFILING
		if (state.current.profile)
			state.current.profile->fields[NumGlobalsFetched]++;
#endif

		const T *y = reinterpret_cast<const T*>(GetGlobalByIndex(global));
		return *y;
	}

	template<typename T>
	inline void SetGlobal(const global_t &global, const T &value)
	{
#ifdef ALLOW_PROFILING
		if (state.current.profile)
			state.current.profile->fields[NumGlobalsSet]++;
#endif

		*reinterpret_cast<T*>(GetGlobalByIndex(global)) = value;

		if (enable_tracing)
			PrintTrace("  SetGlobal: %i -> %s", global, TraceGlobal(global).data());
	}

	[[nodiscard]] inline edict_t *ArgvEntity(const uint8_t &d) const
	{
 		return EntToEntity(GetGlobal<ent_t>(GlobalOffset(global_t::PARM0, d * 3)));
	}
	
	[[nodiscard]] inline edict_t *EntToEntity(const ent_t &ent, bool allow_invalid = false) const
	{
		if (ent == ent_t::ENT_INVALID)
		{
			if (!allow_invalid)
				return nullptr;
			else
				return &game.entity(MAX_EDICTS);
		}
		else if (ent == ent_t::ENT_WORLD)
			return &game.entity(0);

		return &game.entity((reinterpret_cast<uint8_t *>(ent) - reinterpret_cast<uint8_t *>(globals.edicts)) / globals.edict_size);
	}
	
	[[nodiscard]] inline ent_t EntityToEnt(edict_t *ent) const
	{
		if (ent == nullptr)
			return ent_t::ENT_INVALID;
		else if (ent->s.number == 0)
			return ent_t::ENT_WORLD;

		return static_cast<ent_t>(reinterpret_cast<ptrdiff_t>(ent));
	}

	[[nodiscard]] inline const char *ArgvString(const uint8_t &d) const
	{
		const int32_t &str = GetGlobal<int32_t>(GlobalOffset(global_t::PARM0, d * 3));
		return GetString(static_cast<string_t>(str));
	}

	[[nodiscard]] inline const string_t &ArgvStringID(const uint8_t &d) const
	{
		const int32_t &str = GetGlobal<int32_t>(GlobalOffset(global_t::PARM0, d * 3));
		return *reinterpret_cast<const string_t *>(&str);
	}

	[[nodiscard]] inline const int32_t &ArgvInt32(const uint8_t &d) const
	{
		return GetGlobal<int32_t>(GlobalOffset(global_t::PARM0, d * 3));
	}

	[[nodiscard]] inline const vec_t &ArgvFloat(const uint8_t &d) const
	{
		return GetGlobal<vec_t>(GlobalOffset(global_t::PARM0, d * 3));
	}

	[[nodiscard]] inline const vec3_t &ArgvVector(const uint8_t &d) const
	{
		return GetGlobal<vec3_t>(GlobalOffset(global_t::PARM0, d * 3));
	}
	
	inline void Return(const vec_t &value)
	{
		SetGlobal<vec_t>(global_t::RETURN, value);

		PrintTrace("BUILTIN RETURN F %f", value);
	}

	inline void Return(const vec3_t &value)
	{
		for (size_t i = 0; i < value.size(); i++)
			SetGlobal<vec_t>(GlobalOffset(global_t::RETURN, i), value.at(i));

		PrintTrace("BUILTIN RETURN VEC %f %f %f", value[0], value[1], value[2]);
	}
	
	inline void Return(const edict_t &value)
	{
		SetGlobal<int32_t>(global_t::RETURN, reinterpret_cast<int32_t>(&value));

		PrintTrace("BUILTIN RETURN ENT %s", TraceEntity(&value).data());
	}
	
	inline void Return(const int32_t &value)
	{
		SetGlobal<int32_t>(global_t::RETURN, value);

		PrintTrace("BUILTIN RETURN I %i", value);
	}

	inline void Return(const string_t &str)
	{
		SetGlobal<string_t>(global_t::RETURN, str);
		
		PrintTrace("BUILTIN RETURN S %s", GetString(str));
	}

	inline void Return(const func_t &str)
	{
		SetGlobal<func_t>(global_t::RETURN, str);
		
		if (str != func_t::FUNC_VOID)
			PrintTrace("BUILTIN RETURN FUNC %s", GetString(FindFunction(str)->name_index));
		else
			PrintTrace("BUILTIN RETURN FUNC NULL");
	}

	inline void Return(const char *value)
	{
		if (!(value >= string_data && value < string_data + string_size))
			Error("attempt to return dynamic string from %s", __func__);

		Return(static_cast<string_t>(value - string_data));
	}

	inline bool MatchString(const std::string_view &value, string_t &output) const
	{
		// check built-ins
		for (const char *s = string_data; s < string_data + string_size; s += strlen(s) + 1)
		{
			if (value.compare(s) == 0)
			{
				output = static_cast<string_t>(s - string_data);
				return true;
			}
		}
		
		// check temp strings
		for (auto &s : dynamic_strings.GetStrings())
		{
			if (value.compare(dynamic_strings.Get(s.first)) == 0)
			{
				output = static_cast<string_t>(-static_cast<int32_t>(s.first));
				return true;
			}
		}

		return false;
	}

	struct {
		std::deque<QCStack>			stack;
		QCStack						current;
		uint8_t						argc = 0;
	} state;

	inline void Enter(QCFunction &function)
	{
		// save current stack space that will be overwritten by the new function
		if (state.current.function && state.current.function->num_args_and_locals)
		{
			state.current.locals.reserve(function.num_args_and_locals);

			for (size_t i = 0, arg = function.first_arg; i < static_cast<size_t>(function.num_args_and_locals); i++, arg++)
				state.current.locals[arg] = global_data[arg];

			PrintTrace("Backup locals %u -> %u", function.first_arg, function.first_arg + function.num_args_and_locals);
		}

		state.stack.push_back(std::move(state.current));

		// set up current stack
		state.current.function = &function;
		state.current.statement = &statements[function.id - 1];

		// copy parameters
		for (size_t i = 0, arg_id = static_cast<size_t>(function.first_arg); i < static_cast<size_t>(function.num_args); i++)
			for (size_t s = 0; s < function.arg_sizes[i]; s++, arg_id++)
				global_data[arg_id] = *GetGlobalByIndex(GlobalOffset(global_t::PARM0, (i * 3) + s));

#ifdef ALLOW_PROFILING
		state.current.profile = &profile_data[&function - functions.data()];
		state.current.profile->fields[NumSelfCalls]++;
		state.current.start = perf_time();
#endif
	}

	inline void Leave()
	{
		// restore stack
		auto prev_stack = std::move(state.stack.back());
		state.stack.pop_back();

		if (prev_stack.function)
		{
			auto &current_func = *state.current.function;

			for (size_t i = 0, arg = current_func.first_arg; i < static_cast<size_t>(current_func.num_args_and_locals); i++, arg++)
				global_data[arg] = prev_stack.locals[arg];

			PrintTrace("Restore locals %u -> %u", current_func.first_arg, current_func.first_arg + current_func.num_args_and_locals);
		}
		
#ifdef ALLOW_PROFILING
		auto time_spent = perf_time() - state.current.start;
		state.current.profile->total += time_spent;

		// add time we spent in this function into the parent's call_into time
		if (prev_stack.profile)
			prev_stack.profile->call_into += time_spent;
#endif

		// copy it over
		state.current = std::move(prev_stack);
	}

	inline QCFunction *FindFunction(const char *name)
	{
		for (auto &func : functions)
			if (!strcmp(GetString(func.name_index), name))
				return &func;

		return nullptr;
	}

	inline func_t FindFunctionID(const char *name)
	{
		size_t i = 0;

		for (auto &func : functions)
		{
			if (!strcmp(GetString(func.name_index), name))
				return static_cast<func_t>(i);

			i++;
		}

		return func_t::FUNC_VOID;
	}
	
	inline const char *GetString(const string_t &str) const
	{
		if (static_cast<int32_t>(str) < 0)
			return dynamic_strings.Get(str);

 		return string_data + static_cast<int>(str);
	}

	inline QCFunction *FindFunction(const func_t &id)
	{
		return &functions[static_cast<int32_t>(id)];
	}

	inline deftype_t OpcodeType(const opcode_t &code)
	{
		switch (code)
		{
		case OP_STORE_F:
		case OP_LOAD_F:
		case OP_STOREP_F:
			return TYPE_FLOAT;
		case OP_STORE_V:
		case OP_LOAD_V:
		case OP_STOREP_V:
			return TYPE_VECTOR;
		case OP_STORE_ENT:
		case OP_LOAD_ENT:
		case OP_STOREP_ENT:
		case OP_NOT_ENT:
		case OP_EQ_E:
		case OP_NE_E:
			return TYPE_ENTITY;
		case OP_STORE_FLD:
		case OP_LOAD_FLD:
		case OP_STOREP_FLD:
			return TYPE_FIELD;
		case OP_STORE_FNC:
		case OP_LOAD_FNC:
		case OP_STOREP_FNC:
		case OP_NOT_FNC:
		case OP_EQ_FNC:
		case OP_NE_FNC:
			return TYPE_FUNCTION;
		case OP_STORE_S:
		case OP_LOAD_S:
		case OP_STOREP_S:
			return TYPE_STRING;
		default:
			Error("what dis");
		}
	}

	inline int32_t *GetEntityFieldPointer(edict_t &ent, const int32_t &field)
	{
		return reinterpret_cast<int32_t *>(&ent) + field;
	}

	inline int32_t EntityFieldAddress(edict_t &ent, const int32_t &field)
	{
		return reinterpret_cast<uint8_t*>(GetEntityFieldPointer(ent, field)) - reinterpret_cast<uint8_t *>(globals.edicts);
	}

	template<typename T>
	inline T &AddressToEntityField(const int32_t &address)
	{
		return *reinterpret_cast<T *>(reinterpret_cast<uint8_t *>(globals.edicts) + address);
	}

	inline void *AddressToEntityField(const int32_t &address)
	{
		return reinterpret_cast<uint8_t *>(globals.edicts) + address;
	}

	inline ptrdiff_t AddressToField(edict_t &entity, const int32_t &address)
	{
		return &AddressToEntityField<uint8_t>(address) - reinterpret_cast<uint8_t *>(&entity);
	}

	inline edict_t &AddressToEntity(const int32_t &address)
	{
		return game.entity(address / globals.edict_size);
	}

	std::string StackEntry(const QCStack &stack);
	std::string StackTrace();

	inline void CallBuiltin(QCFunction &function)
	{
		const func_t builtin = static_cast<func_t>(function.id);

		if (!builtins.IsRegistered(builtin))
			Error("Bad builtin call number");

		auto profile = &profile_data[&function - functions.data()];
		profile->fields[NumSelfCalls]++;
		auto start = perf_time();
		builtins.Get(builtin)(*this);
		auto time_spent = perf_time() - start;
		profile->total += time_spent;

		// add time we spent in this function into the parent's call_into time
		if (state.current.profile)
			state.current.profile->call_into += time_spent;

		params_from.clear();
	}

	void Execute(QCFunction &function);
};

std::string ParseFormat(const char *format, QCVM &vm, const uint8_t &start);

// Ideally we'd not be static, but I don't expect to run more
// than one VM
extern QCVM qvm;

void InitVM();
void CheckVM();
void ShutdownVM();

// Helpful macro for quickly registering a builtin
#define RegisterBuiltin(name) \
	vm.builtins.Register(#name, QC_ ## name)