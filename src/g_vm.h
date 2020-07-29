#pragma once

#define ALLOW_DEBUGGING
#define ALLOW_PROFILING

#ifdef ALLOW_DEBUGGING
enum debug_state_t
{
	DEBUG_NONE,
	DEBUG_STEP_INTO,
	DEBUG_STEP_OUT,
	DEBUG_STEP_OVER,

	DEBUG_BROKE
};
#endif

#include <cassert>
#include <variant>
#include <string>
#include <stack>
#include <deque>
#include <unordered_map>
#include <unordered_set>

enum
{
	STRING_EMPTY
};

typedef int string_t;

// NOTE: in QC, the value 0 is used for the world.
// For compatibility sake, we also consider 0 to be world,
// but entities are direct memory addresses into globals.edicts.
// The value "1" is used to mean a null entity, which is required
// to differentiate things like groundentity that can be the world.
enum
{
	ENT_WORLD = 0,
	ENT_INVALID = 1
};

typedef int ent_t;

enum
{
	FUNC_VOID
};

typedef int func_t;

enum
{
	OP_DONE,	//0
	OP_MUL_F,
	OP_MUL_V,
	OP_MUL_FV,
	OP_MUL_VF,
	OP_DIV_F,
	OP_ADD_F,
	OP_ADD_V,
	OP_SUB_F,
	OP_SUB_V,
	
	OP_EQ_F,	//10
	OP_EQ_V,
	OP_EQ_S,
	OP_EQ_E,
	OP_EQ_FNC,
	
	OP_NE_F,
	OP_NE_V,
	OP_NE_S,
	OP_NE_E,
	OP_NE_FNC,
	
	OP_LE_F,	//20
	OP_GE_F,
	OP_LT_F,
	OP_GT_F,

	OP_LOAD_F,
	OP_LOAD_V,
	OP_LOAD_S,
	OP_LOAD_ENT,
	OP_LOAD_FLD,
	OP_LOAD_FNC,

	OP_ADDRESS,	//30

	OP_STORE_F,
	OP_STORE_V,
	OP_STORE_S,
	OP_STORE_ENT,
	OP_STORE_FLD,
	OP_STORE_FNC,

	OP_STOREP_F,
	OP_STOREP_V,
	OP_STOREP_S,
	OP_STOREP_ENT,	//40
	OP_STOREP_FLD,
	OP_STOREP_FNC,

	OP_RETURN,
	OP_NOT_F,
	OP_NOT_V,
	OP_NOT_S,
	OP_NOT_ENT,
	OP_NOT_FNC,
	OP_IF_I,
	OP_IFNOT_I,		//50
	OP_CALL0,		//careful... hexen2 and q1 have different calling conventions
	OP_CALL1,		//remap hexen2 calls to OP_CALL2H
	OP_CALL2,
	OP_CALL3,
	OP_CALL4,
	OP_CALL5,
	OP_CALL6,
	OP_CALL7,
	OP_CALL8,
	OP_STATE,		//60
	OP_GOTO,
	OP_AND_F,
	OP_OR_F,
	
	OP_BITAND_F,
	OP_BITOR_F,

	//these following ones are Hexen 2 constants.	
	OP_MULSTORE_F,	//66 redundant, for h2 compat
	OP_MULSTORE_VF,	//67 redundant, for h2 compat
	OP_MULSTOREP_F,	//68
	OP_MULSTOREP_VF,//69

	OP_DIVSTORE_F,	//70 redundant, for h2 compat
	OP_DIVSTOREP_F,	//71

	OP_ADDSTORE_F,	//72 redundant, for h2 compat
	OP_ADDSTORE_V,	//73 redundant, for h2 compat
	OP_ADDSTOREP_F,	//74
	OP_ADDSTOREP_V,	//75

	OP_SUBSTORE_F,	//76 redundant, for h2 compat
	OP_SUBSTORE_V,	//77 redundant, for h2 compat
	OP_SUBSTOREP_F,	//78
	OP_SUBSTOREP_V,	//79

	OP_FETCH_GBL_F,	//80 has built-in bounds check
	OP_FETCH_GBL_V,	//81 has built-in bounds check
	OP_FETCH_GBL_S,	//82 has built-in bounds check
	OP_FETCH_GBL_E,	//83 has built-in bounds check
	OP_FETCH_GBL_FNC,//84 has built-in bounds check

	OP_CSTATE,		//85
	OP_CWSTATE,		//86

	OP_THINKTIME,	//87 shortcut for OPA.nextthink=time+OPB

	OP_BITSETSTORE_F,	//88 redundant, for h2 compat
	OP_BITSETSTOREP_F,	//89
	OP_BITCLRSTORE_F,	//90
	OP_BITCLRSTOREP_F,	//91

	OP_RAND0,		//92	OPC = random()
	OP_RAND1,		//93	OPC = random()*OPA
	OP_RAND2,		//94	OPC = random()*(OPB-OPA)+OPA
	OP_RANDV0,		//95	//3d/box versions of the above.
	OP_RANDV1,		//96
	OP_RANDV2,		//97

	OP_SWITCH_F,	//98	switchref=OPA; PC += OPB   --- the jump allows the jump table (such as it is) to be inserted after the block.
	OP_SWITCH_V,	//99
	OP_SWITCH_S,	//100
	OP_SWITCH_E,	//101
	OP_SWITCH_FNC,	//102

	OP_CASE,		//103	if (OPA===switchref) PC += OPB
	OP_CASERANGE,	//104   if (OPA<=switchref&&switchref<=OPB) PC += OPC

	//the rest are added
	//mostly they are various different ways of adding two vars with conversions.

	//hexen2 calling convention (-TH2 requires us to remap OP_CALLX to these on load, -TFTE just uses these directly.)
	OP_CALL1H,	//OFS_PARM0=OPB
	OP_CALL2H,	//OFS_PARM0,1=OPB,OPC
	OP_CALL3H,	//no extra args
	OP_CALL4H,
	OP_CALL5H,
	OP_CALL6H,		//110
	OP_CALL7H,
	OP_CALL8H,


	OP_STORE_I,
	OP_STORE_IF,			//OPB.f = (float)OPA.i (makes more sense when written as a->b)
	OP_STORE_FI,			//OPB.i = (int)OPA.f
	
	OP_ADD_I,
	OP_ADD_FI,				//OPC.f = OPA.f + OPB.i
	OP_ADD_IF,				//OPC.f = OPA.i + OPB.f	-- redundant...
  
	OP_SUB_I,				//OPC.i = OPA.i - OPB.i
	OP_SUB_FI,		//120	//OPC.f = OPA.f - OPB.i
	OP_SUB_IF,				//OPC.f = OPA.i - OPB.f

	OP_CONV_ITOF,			//OPC.f=(float)OPA.i
	OP_CONV_FTOI,			//OPC.i=(int)OPA.f
	OP_CP_ITOF,				//OPC.f=(float)(*OPA).i
	OP_CP_FTOI,				//OPC.i=(int)(*OPA).f
	OP_LOAD_I,
	OP_STOREP_I,
	OP_STOREP_IF,
	OP_STOREP_FI,

	OP_BITAND_I,	//130
	OP_BITOR_I,

	OP_MUL_I,
	OP_DIV_I,
	OP_EQ_I,
	OP_NE_I,

	OP_IFNOT_S,	//compares string empty, rather than just null.
	OP_IF_S,

	OP_NOT_I,

	OP_DIV_VF,

	OP_BITXOR_I,		//140
	OP_RSHIFT_I,
	OP_LSHIFT_I,

	OP_GLOBALADDRESS,	//C.p = &A + B.i*4
	OP_ADD_PIW,			//C.p = A.p + B.i*4

	OP_LOADA_F,
	OP_LOADA_V,	
	OP_LOADA_S,
	OP_LOADA_ENT,
	OP_LOADA_FLD,
	OP_LOADA_FNC,	//150
	OP_LOADA_I,

	OP_STORE_P,	//152... erm.. wait...
	OP_LOAD_P,

	OP_LOADP_F,
	OP_LOADP_V,	
	OP_LOADP_S,
	OP_LOADP_ENT,
	OP_LOADP_FLD,
	OP_LOADP_FNC,
	OP_LOADP_I,		//160

	OP_LE_I,
	OP_GE_I,
	OP_LT_I,
	OP_GT_I,

	OP_LE_IF,
	OP_GE_IF,
	OP_LT_IF,
	OP_GT_IF,

	OP_LE_FI,
	OP_GE_FI,		//170
	OP_LT_FI,
	OP_GT_FI,

	OP_EQ_IF,
	OP_EQ_FI,

	//-------------------------------------
	//string manipulation.
	OP_ADD_SF,	//(char*)c = (char*)a + (float)b    add_fi->i
	OP_SUB_S,	//(float)c = (char*)a - (char*)b    sub_ii->f
	OP_STOREP_C,//(float)c = *(char*)b = (float)a
	OP_LOADP_C,	//(float)c = *(char*)
	//-------------------------------------


	OP_MUL_IF,
	OP_MUL_FI,		//180
	OP_MUL_VI,
	OP_MUL_IV,
	OP_DIV_IF,
	OP_DIV_FI,
	OP_BITAND_IF,
	OP_BITOR_IF,
	OP_BITAND_FI,
	OP_BITOR_FI,
	OP_AND_I,
	OP_OR_I,		//190
	OP_AND_IF,
	OP_OR_IF,
	OP_AND_FI,
	OP_OR_FI,
	OP_NE_IF,
	OP_NE_FI,

//erm... FTEQCC doesn't make use of these (doesn't model separate pointer types). These are for DP.
	OP_GSTOREP_I,
	OP_GSTOREP_F,
	OP_GSTOREP_ENT,
	OP_GSTOREP_FLD,		//200
	OP_GSTOREP_S,
	OP_GSTOREP_FNC,		
	OP_GSTOREP_V,
	OP_GADDRESS,
	OP_GLOAD_I,
	OP_GLOAD_F,
	OP_GLOAD_FLD,
	OP_GLOAD_ENT,
	OP_GLOAD_S,
	OP_GLOAD_FNC,		//210

//back to ones that we do use.
	OP_BOUNDCHECK,
	OP_UNUSED,	//used to be OP_STOREP_P, which is now emulated with OP_STOREP_I, fteqcc nor fte generated it
	OP_PUSH,	//push 4octets onto the local-stack (which is ALWAYS poped on function return). Returns a pointer.
	OP_POP,		//pop those ones that were pushed (don't over do it). Needs assembler.

	OP_SWITCH_I,//hmm.
	OP_GLOAD_V,
//r3349+
	OP_IF_F,		//compares as an actual float, instead of treating -0 as positive.
	OP_IFNOT_F,

//r5697+
	OP_STOREF_V,	//3 elements...
	OP_STOREF_F,	//1 fpu element...
	OP_STOREF_S,	//1 string reference
	OP_STOREF_I,	//1 non-string reference/int

	OP_NUMOPS,

	OP_BREAKPOINT = 0x10000000
};

typedef uint32_t opcode_t;

enum : uint32_t
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
	// EXT

	TYPE_GLOBAL		= bit(15)
};

typedef uint32_t deftype_t;

enum : uint32_t
{
	GLOBAL_NULL		= 0,
	GLOBAL_RETURN	= 1,
	GLOBAL_PARM0	= 4,		// leave 3 ofs for each parm to hold vectors
	GLOBAL_PARM1	= 7,
	GLOBAL_PARM2	= 10,
	GLOBAL_PARM3	= 13,
	GLOBAL_PARM4	= 16,
	GLOBAL_PARM5	= 19,
	GLOBAL_PARM6	= 22,
	GLOBAL_PARM7	= 25,
	GLOBAL_QC		= 28
};
typedef uint32_t global_t;

struct QCStatement
{
	opcode_t				opcode;
	std::array<global_t, 3>	args;
};

struct QCDefinition
{
	deftype_t	id;
	global_t	global_index;
	string_t	name_index;
};

struct QCFunction
{
	func_t					id;
	global_t				first_arg;
	uint32_t				num_args_and_locals;
	uint32_t				profile;
	string_t				name_index;
	string_t				file_index;
	uint32_t				num_args;
	std::array<uint8_t, 8>	arg_sizes;
};

#ifdef ALLOW_PROFILING
enum
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

typedef size_t profile_type_t;

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

enum
{
	StringAcquire,
	StringRelease,
	StringMark,
	StringCheckUnset,
	StringHasRef,
	StringMarkIfHasRef,

	TotalTimerFields
};

typedef size_t profile_timer_type_t;

constexpr const char *timer_type_names[TotalTimerFields] =
{
	"String Acquire",
	"String Release",
	"String Mark",
	"String Check Unset",
	"String HasRef",
	"String MarkIfHasRef"
};

struct profile_timer_t
{
	size_t		count;
	uint64_t	time;
};

struct active_timer_t
{
	profile_timer_t	&timer;
	uint64_t		start;

	active_timer_t(profile_timer_t &time) :
		timer(time),
		start(Q_time())
	{
		timer.count++;
	}

	~active_timer_t()
	{
		timer.time += Q_time() - start;
	}
};

struct QCProfile
{
	uint64_t	total, call_into;
	size_t		fields[TotalProfileFields];
};
#else
#define CreateTimer(...) \
		Timer_()
#define CreateOpcodeTimer(...) \
		OpcodeTimer_()
#endif

#include <optional>

struct QCVMRefCountBackup
{
	const void					*ptr;
	string_t					id;
};

struct QCStack
{
	QCFunction									*function = nullptr;
	const QCStatement							*statement = nullptr;
	std::vector<std::tuple<global_t, global_t>>	locals;
	std::vector<QCVMRefCountBackup>				ref_strings;

#ifdef ALLOW_PROFILING
	QCProfile	*profile;
	uint64_t	start;
	uint64_t	time_callees;
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
	struct QCVM	&vm;

	// Mapped list to dynamic strings
	std::unordered_map<string_t, QCVMRefCountString>	strings;

	// stores a list of free indices that were explicitly freed
	std::stack<string_t>	free_indices;

	// mapped list of addresses that contain(ed) strings
	std::unordered_map<const void *, string_t> ref_storage;

	string_t Allocate();

public:
	QCVMStringList(struct QCVM &invm);

	const decltype(strings) &GetStrings() const
	{
		return strings;
	}

	string_t StoreRefCounted(std::string &&str);

	void Unstore(const string_t &id, const bool &free_index = true);

	size_t Length(const string_t &id) const;

	std::string &GetRefCounted(const string_t &id);

	const char *Get(const string_t &id) const;

	void AcquireRefCounted(const string_t &id);

	void ReleaseRefCounted(const string_t &id);

	void MarkRefCopy(const string_t &id, const void *ptr);

	void CheckRefUnset(const void *ptr, const size_t &span, const bool &assume_changed = false);

	bool HasRef(const void *ptr);

	bool HasRef(const void *ptr, string_t &id);

	inline bool HasRef(const void *ptr, const size_t &span, std::unordered_map<string_t, size_t> &ids)
	{
		for (size_t i = 0; i < span; i++)
		{
			string_t str;

			if (HasRef((const global_t *)ptr + i, str))
				ids.emplace(str, i);
		}

		return ids.size();
	}

	template<size_t span>
	bool HasRef(const void *ptr, std::unordered_map<string_t, size_t> &ids)
	{
		for (size_t i = 0; i < span; i++)
		{
			string_t str;

			if (HasRef((const global_t *)ptr + i, str))
				ids.emplace(str, i);
		}

		return ids.size();
	}

	template<size_t span = 1>
	void MarkIfHasRef(const void *src_ptr, const void *dst_ptr)
	{
		__attribute__((unused)) auto timer = vm.CreateTimer(StringMarkIfHasRef);

		for (size_t i = 0; i < span; i++)
		{
			const global_t *src_gptr = (const global_t *)src_ptr + i;
			const global_t *dst_gptr = (const global_t *)dst_ptr + i;

			if (ref_storage.contains(src_gptr))
				MarkRefCopy(ref_storage.at(src_gptr), dst_gptr);
		}
	}

	void MarkIfHasRef(const void *src_ptr, const void *dst_ptr, const size_t &span);

	bool IsRefCounted(const string_t &id);

	QCVMRefCountBackup PopRef(const void *ptr);

	void PushRef(const QCVMRefCountBackup &backup);
	
	void WriteState(FILE *fp);
	void ReadState(FILE *fp);
};

class QCVMBuiltinList
{
	struct QCVM								&vm;
	std::unordered_map<func_t, QCBuiltin>	builtins;
	int32_t									next_id;

public:
	QCVMBuiltinList(struct QCVM &invm);
	
	inline void SetFirstID(const int32_t &id)
	{
		next_id = id;
	}

	inline bool IsRegistered(const func_t &func, QCBuiltin &builtin) const
	{
		auto found = builtins.find(func);

		if (found == builtins.end())
			return false;

		builtin = (*found).second;
		return true;
	}

	void Register(const char *name, QCBuiltin builtin);
};

using QCVMFieldWrapper = void(*)(void *out, const void *in);

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

	inline void WrapField(const edict_t *ent, const int32_t &field, const void *src)
	{
		if (!wraps.contains(field))
			return;

		const QCVMFieldWrap *wrap = &wraps.at(field);
		
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
		if (wrap->setter)
			wrap->setter((uint8_t *)(ent->client) + wrap->client_offset, (const int32_t *)(src));
		else
			*(int32_t *)((uint8_t *)(ent->client) + wrap->client_offset) = *(const int32_t *)(src);
#pragma GCC diagnostic pop
	}

	constexpr std::unordered_map<int32_t, QCVMFieldWrap> &GetFields()
	{
		return wraps;
	}
};

#ifdef ALLOW_DEBUGGING
typedef struct
{
	deftype_t		type;

	union
	{
		int	integer;
		float single;
		vec3_t vector;
		string_t strid;
		ent_t entid;
		func_t funcid;
		void *ptr;
	};
} evaluate_result_t;
#endif

struct QCVM
{
	// loaded from progs.dat
	std::vector<QCDefinition>					definitions;
	std::unordered_map<global_t, QCDefinition*> definition_map_by_id;
	std::unordered_map<std::string_view, QCDefinition*> definition_map_by_name;
	std::vector<QCDefinition>					fields;
	std::unordered_map<global_t, QCDefinition*>	field_map;
	std::unordered_map<std::string_view, QCDefinition*> field_map_by_name;
	std::vector<QCStatement>					statements;
	std::vector<QCFunction>						functions;
#ifdef ALLOW_PROFILING
	std::vector<QCProfile>						profile_data;
	profile_timer_t								timers[TotalTimerFields];
	profile_timer_t								opcode_timers[OP_NUMOPS];
#endif
	global_t									*global_data = nullptr;
	size_t										global_size;
	char										*string_data;
	size_t										*string_lengths;
	size_t										string_size;
	std::unordered_set<std::string_view>		string_hashes;
	QCVMStringList								dynamic_strings;
	QCVMBuiltinList								builtins;
	QCVMFieldWrapList							field_wraps;
	std::vector<int>							linenumbers;
	const void									*allowed_stack;
	size_t										allowed_stack_size;

	QCVM();

	template<typename ...T>
	[[noreturn]] void Error(T ...args) const
	{
		std::string str = vas(args...);
		__debugbreak();
		gi.error(str.data());
		exit(0);
	}

#ifdef ALLOW_PROFILING
	[[nodiscard]] active_timer_t CreateTimer(const profile_timer_type_t &type)
	{
		return active_timer_t(timers[type]);
	}

	[[nodiscard]] active_timer_t CreateOpcodeTimer(const opcode_t &code)
	{
		return active_timer_t(opcode_timers[code]);
	}
#else
	// no-op
	constexpr nullptr_t Timer_() { return nullptr; }
	constexpr nullptr_t OpcodeTimer_() { return nullptr; }
#endif

	inline void SetAllowedStack(const void *ptr, const size_t &length)
	{
		allowed_stack = ptr;
		allowed_stack_size = length;
	}

	inline const global_t *GetGlobalByIndex(const global_t &g) const
	{
		return global_data + g;
	}

	inline global_t *GetGlobalByIndex(const global_t &g)
	{
		return global_data + g;
	}

	inline global_t GlobalOffset(const global_t &base, const int32_t &offset) const
	{
		return (global_t)((int32_t)base + offset);
	}

	template<typename T>
	[[nodiscard]] inline const T &GetGlobal(const global_t &global) const
	{
#ifdef ALLOW_PROFILING
		if (state.current->profile)
			state.current->profile->fields[NumGlobalsFetched]++;
#endif

		const T *y = (const T*)GetGlobalByIndex(global);
		return *y;
	}

	template<typename T>
	[[nodiscard]] inline T *GetGlobalPtr(const global_t &global) const
	{
#ifdef ALLOW_PROFILING
		if (state.current->profile)
			state.current->profile->fields[NumGlobalsFetched]++;
#endif

		const int32_t address = *(const int32_t*)GetGlobalByIndex(global);

		if (!PointerValid((size_t)address, false, sizeof(T)))
			Error("bad address");

		return (T *)address;
	}

	template<typename T>
	inline void SetGlobal(const global_t &global, const T &value)
	{
#ifdef ALLOW_PROFILING
		if (state.current->profile)
			state.current->profile->fields[NumGlobalsSet]++;
#endif

		if (global == GLOBAL_NULL)
			Error("attempt to overwrite 0");

		static_assert(std::is_standard_layout_v<T> && std::is_trivial_v<T> && (sizeof(T) % 4) == 0);

		*(T*)(GetGlobalByIndex(global)) = value;
		dynamic_strings.CheckRefUnset(GetGlobalByIndex(global), sizeof(T) / sizeof(global_t));
	}

	inline string_t SetGlobalStr(const global_t &global, std::string &&value)
	{
		string_t str = StoreOrFind(std::move(value));
		SetGlobal(global, str);

		if (dynamic_strings.IsRefCounted(str))
			dynamic_strings.MarkRefCopy(str, GetGlobalByIndex(global));

		return str;
	}

	inline string_t SetStringPtr(global_t *ptr, std::string &&value)
	{
		if (!PointerValid((ptrdiff_t)ptr))
			Error("bad pointer");

		string_t str = StoreOrFind(std::move(value));
		*(string_t *)ptr = str;
		dynamic_strings.CheckRefUnset(ptr, sizeof(string_t) / sizeof(global_t));

		if (dynamic_strings.IsRefCounted(str))
			dynamic_strings.MarkRefCopy(str, ptr);

		return str;
	}

	// safe way of copying globals between other globals
	template<typename TType>
	inline void CopyGlobal(const global_t &dst, const global_t &src)
	{
		constexpr size_t count = sizeof(TType) / sizeof(global_t);

		const void *src_ptr = GetGlobalByIndex(src);
		void *dst_ptr = GetGlobalByIndex(dst);

		memcpy(dst_ptr, src_ptr, sizeof(TType));
		dynamic_strings.CheckRefUnset(dst_ptr, count);

		// if there were any ref strings in src, make sure they are
		// reffed in dst too
		dynamic_strings.MarkIfHasRef<count>(src_ptr, dst_ptr);
	}

	template<typename TDst, typename TSrc>
	inline void CopyGlobal(const global_t &dst, const global_t &src)
	{
		constexpr size_t count = sizeof(TDst) / sizeof(global_t);

		static_assert(sizeof(TDst) == sizeof(TSrc));

		const TSrc *src_ptr = (TSrc *)GetGlobalByIndex(src);
		TDst *dst_ptr = (TDst *)GetGlobalByIndex(dst);

		*(dst_ptr) = *(src_ptr);

		dynamic_strings.CheckRefUnset(dst_ptr, count);

		// if there were any ref strings in src, make sure they are
		// reffed in dst too
		dynamic_strings.MarkIfHasRef<count>(src_ptr, dst_ptr);
	}

	[[nodiscard]] inline edict_t *ArgvEntity(const uint8_t &d) const
	{
		return EntToEntity(GetGlobal<ent_t>(GlobalOffset(GLOBAL_PARM0, d * 3)));
	}
	
	[[nodiscard]] inline edict_t *EntToEntity(const ent_t &ent, bool allow_invalid = false) const
	{
		if (ent == ENT_INVALID)
		{
			if (!allow_invalid)
				return nullptr;
			else
				return itoe(MAX_EDICTS);
		}
		else if (ent == ENT_WORLD)
			return itoe(0);

		return itoe(((uint8_t *)(ent) - (uint8_t *)(globals.edicts)) / globals.edict_size);
	}
	
	[[nodiscard]] inline ent_t EntityToEnt(edict_t *ent) const
	{
		if (ent == nullptr)
			return ENT_INVALID;
		else if (ent->s.number == 0)
			return ENT_WORLD;

		return (ent_t)(ptrdiff_t)ent;
	}

	[[nodiscard]] inline const char *ArgvString(const uint8_t &d) const
	{
		const int32_t &str = GetGlobal<int32_t>(GlobalOffset(GLOBAL_PARM0, d * 3));
		return GetString((string_t)str);
	}

	[[nodiscard]] inline const string_t &ArgvStringID(const uint8_t &d) const
	{
		const int32_t &str = GetGlobal<int32_t>(GlobalOffset(GLOBAL_PARM0, d * 3));
		return *(const string_t *)&str;
	}

	[[nodiscard]] inline const int32_t &ArgvInt32(const uint8_t &d) const
	{
		return GetGlobal<int32_t>(GlobalOffset(GLOBAL_PARM0, d * 3));
	}

	[[nodiscard]] inline const vec_t &ArgvFloat(const uint8_t &d) const
	{
		return GetGlobal<vec_t>(GlobalOffset(GLOBAL_PARM0, d * 3));
	}

	[[nodiscard]] inline const vec3_t &ArgvVector(const uint8_t &d) const
	{
		return GetGlobal<vec3_t>(GlobalOffset(GLOBAL_PARM0, d * 3));
	}
	
	template<typename T>
	[[nodiscard]] inline T *ArgvPointer(const uint8_t &d)
	{
		return (T *)ArgvInt32(d);
	}

	inline void ReturnFloat(const vec_t &value)
	{
		SetGlobal(GLOBAL_RETURN, value);
	}

	inline void ReturnVector(const vec3_t &value)
	{
		for (size_t i = 0; i < value.size(); i++)
			SetGlobal(GlobalOffset(GLOBAL_RETURN, i), value.at(i));
	}
	
	inline void ReturnEntity(const edict_t *value)
	{
		SetGlobal(GLOBAL_RETURN, (int32_t)value);
	}
	
	inline void ReturnInt(const int32_t &value)
	{
		SetGlobal(GLOBAL_RETURN, value);
	}

	inline void ReturnFunc(const func_t &str)
	{
		SetGlobal(GLOBAL_RETURN, str);
	}

	inline void ReturnString(const char *value)
	{
		if (!(value >= string_data && value < string_data + string_size))
			Error("attempt to return dynamic string from %s", __func__);

		ReturnString((string_t)(value - string_data));
	}

	inline void ReturnString(std::string &&str)
	{
		SetGlobalStr(GLOBAL_RETURN, std::move(str));
	}

	inline void ReturnString(const string_t &str)
	{
		SetGlobal(GLOBAL_RETURN, str);
	}

	inline bool FindString(const std::string_view &value, string_t &rstr)
	{
		rstr = STRING_EMPTY;

		if (!value.length())
			return true;

		// check built-ins
		auto builtin = string_hashes.find(value);

		if (builtin != string_hashes.end())
		{
			rstr = (string_t)((*builtin).data() - string_data);
			return true;
		}

		// check temp strings
		// TODO: hash these too
		for (auto &s : dynamic_strings.GetStrings())
		{
			const char *str = dynamic_strings.Get(s.first);

			if (str && value.compare(str) == 0)
			{
				rstr = s.first;
				return true;
			}
		}

		return false;
	}

	// Note: DOES NOT ACQUIRE IF REF COUNTED!!
	inline string_t StoreOrFind(std::string &&value)
	{
		// check built-ins
		string_t str;

		if (FindString(value, str))
			return str;

		return dynamic_strings.StoreRefCounted(std::move(value));
	}

	struct QCVMState {
		std::list<QCStack>				stack;
		QCStack							*current;
		uint8_t							argc = 0;

		QCVMState()
		{
			current = &stack.emplace_back();
		}
	} state;

#ifdef ALLOW_DEBUGGING
	struct {
		debug_state_t state;
		QCFunction *step_function;
		const QCStatement *step_statement;
		size_t step_depth;
	} debug;

	inline evaluate_result_t ValueFromPtr(const QCDefinition *def, const void *ptr)
	{
		switch (def->id & ~TYPE_GLOBAL)
		{
		default:
			return { };
		case TYPE_STRING:
			return { .type = TYPE_STRING, .strid = *(const string_t *)(ptr) };
		case TYPE_FLOAT:
			return { .type = TYPE_FLOAT, .single = *(const vec_t *)(ptr) };
		case TYPE_VECTOR:
			return { .type = TYPE_VECTOR, .vector = *(const vec3_t *)(ptr) };
		case TYPE_ENTITY:
			return { .type = TYPE_ENTITY, .entid = *(const ent_t *)(ptr) };
		case TYPE_FIELD:
			return { };
		case TYPE_FUNCTION:
			return { .type = TYPE_FUNCTION, .funcid = *(const func_t *)(ptr) };
		case TYPE_POINTER:
			return { .type = TYPE_POINTER, .ptr = (void *)(*(const int *)(ptr)) };
		case TYPE_INTEGER:
			return { .type = TYPE_INTEGER, .integer = *(const int32_t *)(ptr) };
		}
	}

	inline evaluate_result_t ValueFromGlobal(const QCDefinition *def)
	{
		return ValueFromPtr(def, GetGlobalByIndex(def->global_index));
	}

	evaluate_result_t EvaluateFromLocalOrGlobal(const std::string_view &variable)
	{
		// we don't have a . so we're just checking for a base object.
		// check locals
		if (state.current->function)
		{
			size_t i;
			global_t g;

			for (i = 0, g = state.current->function->first_arg; i < state.current->function->num_args_and_locals; i++, g++)
			{
				QCDefinition *def = definition_map_by_id[g];

				if (!def || def->name_index == STRING_EMPTY || variable != GetString(def->name_index))
					continue;

				return ValueFromGlobal(def);
			}
		}

		// no locals, so we can check all the globals
		for (auto &def : definitions)
		{
			if (def.name_index == STRING_EMPTY || variable != GetString(def.name_index))
				continue;

			return ValueFromGlobal(&def);
		}

		return { };
	}

	evaluate_result_t Evaluate(const std::string &variable)
	{
		if (variable.find_first_of('.') != std::string::npos)
		{
			// we have a . so we're either a entity, pointer or struct...
			std::string_view context = std::string_view(variable).substr(0, variable.find_first_of('.'));
			evaluate_result_t left_hand = EvaluateFromLocalOrGlobal(context);

			if (left_hand.type == TYPE_ENTITY)
			{
				std::string_view right_context = std::string_view(variable).substr(context.length() + 1);
				const ent_t &ent = left_hand.entid;

				if (ent == ENT_INVALID)
					return left_hand;

				const QCDefinition *field = nullptr;

				for (auto &f : fields)
				{
					if (f.name_index == STRING_EMPTY || strcmp(GetString(f.name_index), right_context.data()))
						continue;

					field = &f;
					break;
				}

				if (!field)
					return { };

				return ValueFromPtr(field, GetEntityFieldPointer(EntToEntity(ent), (int32_t)field->global_index));
			}
		}

		return EvaluateFromLocalOrGlobal(variable);
	}

	void BreakOnCurrentStatement();
#endif

	inline int LineNumberFor(const QCStatement *statement)
	{
		return linenumbers[statement - statements.data()];
	}

	inline void Enter(QCFunction *function)
	{
		QCStack *cur_stack = state.current;

		// save current stack space that will be overwritten by the new function
		if (function->num_args_and_locals)
		{
			cur_stack->locals.reserve(function->num_args_and_locals);

			for (size_t i = 0, arg = function->first_arg; i < function->num_args_and_locals; i++, arg++)
			{
				cur_stack->locals.push_back(std::make_tuple((global_t)arg, GetGlobal<global_t>((global_t)arg)));

				const void *ptr = GetGlobalByIndex((global_t)arg);

				if (dynamic_strings.HasRef(ptr))
					cur_stack->ref_strings.push_back(dynamic_strings.PopRef(ptr));
			}
		}

		QCStack *new_stack = &state.stack.emplace_back();

		// set up current stack
		new_stack->function = function;
		new_stack->statement = &statements[function->id - 1];

		// copy parameters
		for (size_t i = 0, arg_id = function->first_arg; i < function->num_args; i++)
			for (size_t s = 0; s < function->arg_sizes[i]; s++, arg_id++)
				CopyGlobal<global_t>(arg_id, GlobalOffset(GLOBAL_PARM0, (i * 3) + s));

#ifdef ALLOW_PROFILING
		new_stack->profile = &profile_data[function - functions.data()];
		new_stack->profile->fields[NumSelfCalls]++;
		new_stack->start = Q_time();
#endif

		state.current = new_stack;
	}

	inline void Leave()
	{
		// restore stack
#ifdef ALLOW_PROFILING
		auto current_stack = std::move(state.stack.back());
#endif
		state.stack.pop_back();
		QCStack *prev_stack = &state.stack.back();

		state.current = prev_stack;

		if (prev_stack->locals.size())
		{
			for (auto &local : prev_stack->locals)
				SetGlobal(std::get<0>(local), std::get<1>(local));

			for (auto &str : prev_stack->ref_strings)
				dynamic_strings.PushRef(str);

			prev_stack->ref_strings.clear();
			prev_stack->locals.clear();
		}
		
#ifdef ALLOW_PROFILING
		const uint64_t time_spent = Q_time() - current_stack.start;
		current_stack.profile->total += time_spent;

		// add time we spent in this function into the parent's call_into time
		if (prev_stack->profile)
			prev_stack->profile->call_into += time_spent;
#endif

		allowed_stack = 0;
		allowed_stack_size = 0;
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
				return (func_t)i;

			i++;
		}

		return FUNC_VOID;
	}
	
	inline const char *GetString(const string_t &str) const
	{
		if (str < 0)
			return dynamic_strings.Get(str);
		else if ((size_t)str >= string_size)
			Error("bad string");

		return string_data + (size_t)str;
	}

	inline size_t StringLength(const string_t &str) const
	{
		if (str < 0)
			return dynamic_strings.Length(str);
		else if ((size_t)str >= string_size)
			Error("bad string");

		return string_lengths[(size_t)str];
	}

	inline QCFunction *FindFunction(const func_t &id)
	{
		return &functions[id];
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
// EXTENSIONS !!!
		case OP_STORE_I:
		case OP_LOAD_I:
		case OP_STOREP_I:
			return TYPE_INTEGER;
		case OP_STORE_P:
		case OP_LOAD_P:
			return TYPE_POINTER;
// EXTENSIONS !!!
		default:
			Error("what dis");
		}
	}

	inline int32_t *GetEntityFieldPointer(edict_t *ent, const int32_t &field)
	{
		return (int32_t *)(ent) + field;
	}

	inline int32_t EntityFieldAddress(edict_t *ent, const int32_t &field)
	{
		return (int32_t)GetEntityFieldPointer(ent, field);
	}

	inline void *AddressToEntityField(const int32_t &address)
	{
		return (uint8_t *)address;
	}

	template<typename T>
	inline T *AddressToEntityField(const int32_t &address)
	{
		return (T *)AddressToEntityField(address);
	}

	inline ptrdiff_t AddressToField(edict_t *entity, const int32_t &address)
	{
		return AddressToEntityField<uint8_t>(address) - (uint8_t *)(entity);
	}

	inline edict_t *AddressToEntity(const int32_t &address)
	{
		return itoe(address / globals.edict_size);
	}

	inline bool PointerValid(const size_t &address, const bool allow_null = false, const size_t &len = sizeof(global_t)) const
	{
		return (allow_null && address == 0) ||
			(address >= (ptrdiff_t)globals.edicts && (address + len) < ((ptrdiff_t)globals.edicts + (globals.edict_size * globals.max_edicts))) ||
			(address >= (ptrdiff_t)global_data && (address + len) < (ptrdiff_t)(global_data + global_size)) ||
			(allowed_stack && address >= (ptrdiff_t)allowed_stack && address < ((ptrdiff_t)allowed_stack + allowed_stack_size));
	}

	std::string StackEntry(const QCStack &stack);
	std::string StackTrace();

	inline void CallBuiltin(QCFunction *function)
	{
		QCBuiltin func;

		if (!builtins.IsRegistered(function->id, func))
			Error("Bad builtin call number");

#ifdef ALLOW_PROFILING
		QCProfile *profile = &profile_data[function - functions.data()];
		profile->fields[NumSelfCalls]++;
		const uint64_t start = Q_time();
#endif

		func(*this);

#ifdef ALLOW_PROFILING
		const uint64_t time_spent = Q_time() - start;
		profile->total += time_spent;

		// add time we spent in this function into the parent's call_into time
		if (state.current->profile)
			state.current->profile->call_into += time_spent;
#endif
	}

	void Execute(QCFunction *function);
	
	void WriteState(FILE *fp);
	void ReadState(FILE *fp);

#ifdef ALLOW_DEBUGGING
	void SetBreakpoint(const int &mode, const std::string &file, const int &line);
#endif
};

std::string ParseFormat(const string_t &format, QCVM &vm, const uint8_t &start);

// Ideally we'd not be static, but I don't expect to run more
// than one VM
extern QCVM qvm;

void InitVM();
void CheckVM();
void ShutdownVM();

// Helpful macro for quickly registering a builtin
#define RegisterBuiltin(name) \
	vm.builtins.Register(#name, QC_ ## name)

extern const cvar_t *game_var;