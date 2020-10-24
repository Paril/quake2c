#pragma once

// fixing a bug with num_locals
#define LOCALS_FIX 2

typedef struct qcvm_s qcvm_t;

// whether or not to use address-of-label opcode jumps.
// if this is disabled, a simple switch(code) is used.
#define USE_GNU_OPCODE_JUMPING
// whether the FTEQCC debugger is supported. shouldn't really
// affect performance.
#define ALLOW_DEBUGGING
// whether intense instrumentation is enabled or not.
// enabling this may have adverse consequences on performance!
//#define ALLOW_INSTRUMENTING
// whether simple sampling is enabled or not. this is light and
// will barely affect performance.
//#define ALLOW_PROFILING

#ifdef ALLOW_DEBUGGING
typedef enum
{
	DEBUG_NONE,
	DEBUG_STEP_INTO,
	DEBUG_STEP_OUT,
	DEBUG_STEP_OVER,

	DEBUG_BROKE
} qcvm_debug_state_t;

typedef void *qcvm_mutex_t;
typedef void *qcvm_thread_t;
typedef void (*qcvm_thread_func_t) (void);
#endif

#include <assert.h>

enum
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
typedef uint32_t qcvm_global_t;

enum
{
	STRING_EMPTY
};

typedef int qcvm_string_t;

// NOTE: in QC, the value 0 is used for the world.
// The value of -1 is used to mean a null entity, which is required
// to differentiate things like groundentity that can be the world.
enum
{
	ENT_WORLD = 0,
	ENT_INVALID = -1
};

typedef int qcvm_ent_t;

enum
{
	FUNC_VOID
};

typedef int qcvm_func_t;

enum
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

	TYPE_GLOBAL		= 1 << 15
};

typedef uint32_t qcvm_deftype_t;

inline size_t qcvm_type_span(const qcvm_deftype_t type)
{
	return (type & ~TYPE_GLOBAL) == TYPE_VECTOR ? 3 : 1;
}

inline size_t qcvm_type_size(const qcvm_deftype_t type)
{
	return qcvm_type_span(type) * sizeof(qcvm_global_t);
}

typedef uint32_t qcvm_opcode_t;

typedef struct
{
	qcvm_global_t	a, b, c;
} qcvm_operands_t;

typedef struct
{
	qcvm_opcode_t	opcode;
	qcvm_operands_t	args;
} qcvm_statement_t;

typedef struct
{
	qcvm_deftype_t	id;
	qcvm_global_t	global_index;
	qcvm_string_t	name_index;
} qcvm_definition_t;

typedef struct
{
	qcvm_func_t		id;
	qcvm_global_t	first_arg;
	uint32_t		num_args_and_locals;
	uint32_t		profile;
	qcvm_string_t	name_index;
	qcvm_string_t	file_index;
	uint32_t		num_args;
	uint8_t			arg_sizes[8];
} qcvm_function_t;

#if defined(ALLOW_INSTRUMENTING) || defined(ALLOW_PROFILING)
enum
{
	PROFILE_FUNCTIONS	= 1,
	PROFILE_FIELDS		= 2,
	PROFILE_TIMERS		= 4,
	PROFILE_OPCODES		= 8,
	PROFILE_SAMPLES		= 16,

	// allows profile to continue between multiple
	// games
	PROFILE_CONTINUOUS	= 32
};

typedef int32_t qcvm_profiler_flags_t;

enum
{
	MARK_INIT,
	MARK_SHUTDOWN,
	MARK_SPAWNENTITIES,
	MARK_WRITEGAME,
	MARK_READGAME,
	MARK_WRITELEVEL,
	MARK_READLEVEL,
	MARK_CLIENTCONNECT,
	MARK_CLIENTBEGIN,
	MARK_CLIENTUSERINFOCHANGED,
	MARK_CLIENTDISCONNECT,
	MARK_CLIENTCOMMAND,
	MARK_CLIENTTHINK,
	MARK_RUNFRAME,
	MARK_SERVERCOMMAND,
	MARK_CUSTOM,

	TOTAL_MARKS
};

typedef size_t qcvm_profiler_mark_t;

#ifdef ALLOW_PROFILING
typedef struct
{
	uint64_t	count[TOTAL_MARKS];
} qcvm_sampling_t;
#endif

#endif

#ifdef ALLOW_INSTRUMENTING
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

typedef size_t qcvm_profiler_field_t;

enum
{
	StringAcquire,
	StringRelease,
	StringMark,
	StringCheckUnset,
	StringHasRef,
	StringMarkIfHasRef,
	StringMarkRefsCopied,
	StringPopRef,
	StringPushRef,
	StringFind,

	WrapApply,

	TotalTimerFields
};

typedef size_t qcvm_profiler_timer_id_t;

typedef struct
{
	size_t		count[TOTAL_MARKS];
	uint64_t	time[TOTAL_MARKS];
} qcvm_profile_timer_t;

typedef struct
{
	qcvm_profile_timer_t	*timer;
	uint64_t				start;
} qcvm_active_timer_t;

#define START_TIMER(vm, id) \
	qcvm_active_timer_t __timer; \
	if (vm->profile_flags & PROFILE_TIMERS) \
	{ \
		__timer = (qcvm_active_timer_t) { &vm->timers[id][vm->profiler_mark], Q_time() }; __timer.timer->count[vm->profiler_mark]++; \
	}
#define END_TIMER(vm, flag) \
	{ if (vm->profile_flags & flag) \
		__timer.timer->time[vm->profiler_mark] += Q_time() - __timer.start; }

#define RESUME_TIMER(vm, flag) \
	{ if (vm->profile_flags & flag) \
		__timer.start = Q_time(); }

#define START_OPCODE_TIMER(vm, id) \
	qcvm_active_timer_t __timer; \
	if (vm->profile_flags & PROFILE_OPCODES) \
	{ \
		__timer = (qcvm_active_timer_t) { &vm->opcode_timers[id][vm->profiler_mark], Q_time() }; __timer.timer->count[vm->profiler_mark]++; \
	}

typedef struct
{
	uint64_t	ext[TOTAL_MARKS], self[TOTAL_MARKS];
	size_t		fields[TotalProfileFields][TOTAL_MARKS];
} qcvm_profile_t;
#else
#define START_TIMER(...)
#define END_TIMER(...)
#define RESUME_TIMER(...)
#define START_OPCODE_TIMER(...)
#endif

// POINTER STUFF
enum
{
	// null pointer; offset means nothing
	QCVM_POINTER_NULL,
	// pointer into global; offset is a byte offset into global data
	QCVM_POINTER_GLOBAL,
	// pointer into entity; offset is a byte offset into entity data
	QCVM_POINTER_ENTITY,
	// pointer into handle; first 10 bits are handle index (max handles of 2048)
	// and the other 20 bits are the byte offset (max offset of 2 MB)
	QCVM_POINTER_HANDLE
};

typedef uint32_t qcvm_pointer_type_t;

typedef struct
{
	uint32_t			offset : 20;
	uint32_t			index : 10;
	qcvm_pointer_type_t	type : 2;
} qcvm_handle_pointer_t;

typedef struct
{
	uint32_t			offset : 30;
	qcvm_pointer_type_t	type : 2;
} qcvm_raw_pointer_t;

typedef union
{
	qcvm_raw_pointer_t		raw;
	qcvm_handle_pointer_t	handle;
} qcvm_pointer_t;

// Variant
typedef struct
{
	qcvm_deftype_t	type;
	union
	{
		qcvm_string_t	str;
		vec_t			flt;
		vec3_t			vec;
		qcvm_ent_t		ent;
		int32_t			fld;
		qcvm_func_t		fnc;
		qcvm_pointer_t	ptr;
		int32_t			itg; // in-te-ger.. I guess
	} value;
} qcvm_variant_t;

// Strings & string lists
typedef struct
{
	const void		*ptr;
	qcvm_string_t	id;
} qcvm_string_backup_t;

enum { STACK_STRINGS_RESERVE = 64 };

typedef struct
{
	qcvm_t					*vm;
	qcvm_function_t			*function;
	const qcvm_statement_t	*statement;
	qcvm_global_t			*locals;
	qcvm_string_backup_t	*ref_strings;
	size_t					ref_strings_size, ref_strings_allocated;

#ifdef ALLOW_INSTRUMENTING
	qcvm_profile_t	*profile;
	uint64_t		callee_start, caller_start;
#endif
} qcvm_stack_t;

typedef struct
{
	const char	*str;
	size_t		length;
	size_t		ref_count;
} qcvm_ref_counted_string_t;

static const size_t REF_STRING_RESERVE = 256;
static const size_t FREE_STRING_RESERVE = 64;

typedef struct qcvm_ref_storage_hash_s
{
	const void		*ptr;
	qcvm_string_t	id;
#ifdef _DEBUG
	qcvm_stack_t	stack;
#endif

	uint32_t						hash_value;
	struct qcvm_ref_storage_hash_s	*hash_next, *hash_prev;
} qcvm_ref_storage_hash_t;

// QC does not inherently support dynamic strings, but since they're stored in an integer
// QC VMs have gone along with the standard of "positive string is offset in static string data,
// negative string is a dynamic string". The term "dynamic" is interchangeable with "temp", which
// is what Quake 1 calls them. Our implementation of temp strings prefers to make programming
// with strings *easier* at the expense of operation time. There is no string "allocation" or
// "freeing"; you just work with strings in QC as if they were magical and you never have to
// think about it. On the engine side, we ref count strings, storing pointers to where they
// exist in memory and handling ref counting that way. It's more complex on our end, but
// less of a hassle and prevents other weird issues (we don't have to think about save/load
// for instance; we just pop in the strings and away we go). Strings in my QCVM are
// **immutable** in every circumstance. Other QCVMs allow string mutation.
typedef struct
{
	// Mapped list to dynamic strings
	qcvm_ref_counted_string_t	*strings;
	size_t						strings_size, strings_allocated;

	// stores a list of free indices that were explicitly freed
	qcvm_string_t	*free_indices;
	size_t			free_indices_size, free_indices_allocated;

	// mapped list of addresses that contain(ed) strings
	qcvm_ref_storage_hash_t	*ref_storage_data, **ref_storage_hashes, *ref_storage_free;
	size_t					ref_storage_stored, ref_storage_allocated;
} qcvm_string_list_t;

const char *qcvm_string_list_get(const qcvm_t *vm, const qcvm_string_t id);
void qcvm_string_list_acquire(qcvm_t *vm, const qcvm_string_t id);
void qcvm_string_list_release(qcvm_t *vm, const qcvm_string_t id);
// mark a memory address as containing a reference to the specified string.
// increases ref count by 1 and shoves it into the list.
void qcvm_string_list_mark_ref_copy(qcvm_t *vm, const qcvm_string_t id, const void *ptr);
bool qcvm_string_list_check_ref_unset(qcvm_t *vm, const void *ptr, const size_t span, const bool assume_changed);
void qcvm_string_list_mark_refs_copied(qcvm_t *vm, const void *src, const void *dst, const size_t span);
bool qcvm_string_list_is_ref_counted(qcvm_t *vm, const qcvm_string_t id);
bool qcvm_find_string(qcvm_t *vm, const char *value, qcvm_string_t *rstr);
// An easy method of easily storing a string and getting an ID, whether dynamic or not.
// Note that this does not *acquire* the string if it's dynamic! You are still responsible
// for either using mark_ref_copy or manually using acquire/release to ref count it.
qcvm_string_t qcvm_store_or_find_string(qcvm_t *vm, const char *value, const size_t len, const bool copy);
const char *qcvm_get_string(const qcvm_t *vm, const qcvm_string_t str);
size_t qcvm_get_string_length(const qcvm_t *vm, const qcvm_string_t str);

#ifdef QCVM_INTERNAL
// Note: ownership of the pointer is transferred to the string list here.
qcvm_string_t qcvm_string_list_store(qcvm_t *vm, const char *str, const size_t len);
void qcvm_string_list_unstore(qcvm_t *vm, const qcvm_string_t id);
qcvm_string_t *qcvm_string_list_has_ref(qcvm_t *vm, const void *ptr, qcvm_ref_storage_hash_t **hashed_ptr);
qcvm_string_backup_t qcvm_string_list_pop_ref(qcvm_t *vm, const void *ptr);
void qcvm_string_list_push_ref(qcvm_t *vm, const qcvm_string_backup_t *backup);
void qcvm_string_list_read_state(qcvm_t *vm, FILE *fp);
void qcvm_string_list_write_state(qcvm_t *vm, FILE *fp);
#endif

#ifdef _DEBUG
void qcvm_string_list_dump_refs(FILE *fp, qcvm_t *vm);
#endif

typedef void (*qcvm_builtin_t) (qcvm_t *vm);

typedef struct
{
	qcvm_builtin_t	*list;
	size_t			count, registered;
} qcvm_builtin_list_t;

void qcvm_builtin_list_register(qcvm_t *vm, const char *name, qcvm_builtin_t builtin);
qcvm_builtin_t qcvm_builtin_list_get(qcvm_t *vm, const qcvm_func_t func);

// Helpful macro for quickly registering a builtin
#define qcvm_register_builtin(name) \
	qcvm_builtin_list_register(vm, #name, QC_ ## name)

// Initializes all of the core builtins.
void qcvm_init_all_builtins(qcvm_t *vm);

typedef struct
{
	qcvm_definition_t	*def, *field;
	qcvm_global_t		offset;
	size_t				span;
} qcvm_system_field_t;

void qcvm_register_system_field(qcvm_t *vm, const char *field_name, const size_t field_offset, const size_t field_span);

typedef void(*qcvm_field_setter_t)(void *out, const void *in);

typedef struct qcvm_field_wrapper_s
{
	const qcvm_definition_t	*field;
	size_t					field_offset;
	bool					is_client;
	size_t					struct_offset;
	qcvm_field_setter_t		setter;
} qcvm_field_wrapper_t;

void qcvm_field_wrap_list_register(qcvm_t *vm, const char *field_name, const size_t field_offset, const size_t client_offset, qcvm_field_setter_t setter);
void qcvm_field_wrap_list_check_set(qcvm_t *vm, const void *ptr, const size_t span);

static const size_t STACK_RESERVE = 32;

typedef struct
{
	qcvm_t *vm;
	qcvm_stack_t *stack;
	size_t stack_allocated;
	uint8_t	argc;
	int32_t current;

#ifdef ALLOW_INSTRUMENTING
	qcvm_profiler_mark_t	profile_mark_backup;
	size_t					profile_mark_depth;
#endif
} qcvm_state_t;

void qcvm_state_needs_resize(qcvm_state_t *state);
qcvm_stack_t *qcvm_state_stack_push(qcvm_state_t *state);
void qcvm_state_stack_pop(qcvm_state_t *state);
void qcvm_stack_push_ref_string(qcvm_stack_t *stack, const qcvm_string_backup_t ref_string);

typedef struct qcvm_string_hash_s
{
	const char		*str;

	uint32_t					hash_value;
	struct qcvm_string_hash_s	*hash_next;
} qcvm_string_hash_t;

typedef struct qcvm_definition_hash_s
{
	qcvm_definition_t	*def;

	uint32_t						hash_value;
	struct qcvm_definition_hash_s	*hash_next;
} qcvm_definition_hash_t;

// HANDLES
// Quake2C uses handles for various things, like files and advanced containers
// like hashset. The following few functions deal with handles. They're a simple
// integer-indexed list, with 0 acting as "null", for fast access to handles.
// On the engine side, you set up a descriptor which stores the cleanup/free function
// as well as read/write functions which control how these are persisted to save games
// if used as globals/fields (the engine won't care about *where* they're stored, but will
// persist these if need be). Then, you simply call qcvm_handle_alloc with the pointer and
// descriptor, and you're off to the races.
// On the QC side, just be sure to call handle_free when you're done with it!
typedef struct
{
	void	(*free)				(qcvm_t *vm, void *handle);
	void	(*write)			(qcvm_t *vm, void *handle, FILE *fp);
	void	*(*read)			(qcvm_t *vm, FILE *fp);
	bool	(*resolve_pointer)	(const qcvm_t *vm, void *handle, const size_t offset, const size_t len, void **address);
} qcvm_handle_descriptor_t;

typedef int32_t qcvm_handle_id_t;

typedef struct
{
	qcvm_handle_id_t				id;
	const qcvm_handle_descriptor_t	*descriptor;
	void							*handle;
} qcvm_handle_t;

typedef struct
{
	qcvm_handle_t	*data;
	size_t			size, allocated;
	int32_t			*free;
	size_t			free_size;
} qcvm_handle_list_t;

static const size_t HANDLES_RESERVE = 128;

int32_t qcvm_handle_alloc(qcvm_t *vm, void *ptr, const qcvm_handle_descriptor_t *descriptor);
qcvm_handle_t *qcvm_fetch_handle(const qcvm_t *vm, const int32_t id);

#define qcvm_fetch_handle_typed(type, vm, id) \
	((type *)(qcvm_fetch_handle(vm, (id))->handle))

void qcvm_handle_free(qcvm_t *vm, qcvm_handle_t *handle);

qcvm_global_t *qcvm_get_global(qcvm_t *vm, const qcvm_global_t g);
const qcvm_global_t *qcvm_get_const_global(const qcvm_t *vm, const qcvm_global_t g);

inline qcvm_global_t qcvm_global_offset(const qcvm_global_t base, const int32_t offset)
{
	return (qcvm_global_t)(base + offset);
}

#define qcvm_get_global_typed(type, vm, global) \
	(type *)(qcvm_get_global(vm, global))

#define qcvm_get_const_global_typed(type, vm, global) \
	(const type *)qcvm_get_const_global(vm, global)

void *qcvm_get_global_ptr(qcvm_t *vm, const qcvm_global_t global, const size_t value_size);

#define qcvm_get_global_ptr_typed(type, vm, global) \
	(type *)(qcvm_get_global_ptr(vm, global, sizeof(type)))

void qcvm_set_global(qcvm_t *vm, const qcvm_global_t global, const void *value, const size_t value_size);

// NOTE: do *not* use this to pass pointers! this is for value types only
#define qcvm_set_global_typed_value(type, vm, global, value) \
	qcvm_set_global(vm, global, &(value), sizeof(type))

// NOTE: do *not* use this to pass values! this is for pointers only
#define qcvm_set_global_typed_ptr(type, vm, global, value_ptr) \
	qcvm_set_global(vm, global, value_ptr, sizeof(type))

// VM
qcvm_definition_t *qcvm_find_definition(qcvm_t *vm, const char *name, const qcvm_deftype_t type);

qcvm_definition_t *qcvm_find_field(qcvm_t *vm, const char *name);

int qcvm_line_number_for(const qcvm_t *vm, const qcvm_statement_t *statement);

qcvm_func_t qcvm_find_function_id(const qcvm_t *vm, const char *name);

qcvm_function_t *qcvm_get_function(const qcvm_t *vm, const qcvm_func_t id);

qcvm_function_t *qcvm_find_function(const qcvm_t *vm, const char *name);

qcvm_pointer_t qcvm_get_entity_field_pointer(qcvm_t *vm, edict_t *ent, const int32_t field);

const char *qcvm_stack_entry(const qcvm_t *vm, const qcvm_stack_t *s, const bool compact);

const char *qcvm_stack_trace(const qcvm_t *vm, const bool compact);

void qcvm_execute(qcvm_t *vm, qcvm_function_t *function);
	
void qcvm_write_state(qcvm_t *vm, FILE *fp);

void qcvm_read_state(qcvm_t *vm, FILE *fp);

const char *qcvm_parse_format(const qcvm_string_t formatid, const qcvm_t *vm, const uint8_t start);

void qcvm_load(qcvm_t *vm, const char *engine_name, const char *filename);

void qcvm_check(qcvm_t *vm);

void qcvm_shutdown(qcvm_t *vm);

// VM struct
typedef struct qcvm_s
{
	// most of these are just data loaded from the progs.
	// the rest is hashing and ordering for quick lookups.
	// definitions contain all of the globals and such. the definition's global index
	// points to where it is in the global_data
	qcvm_definition_t		*definitions;
	size_t					definitions_size;
	qcvm_definition_t		**definition_map_by_id;
	qcvm_definition_hash_t	**definition_hashes, *definition_hashes_data;
	// fields are entity fields; after all remapping and such is done (see below)
	// the highest potential field is the final size of a single entity structure, which
	// is stored in field_real_size and used later on. It's a bit confusing, but a
	// field's global_index is the offset in the entity where the field lives, and its "id"
	// is the type of the field (TYPE_VECTOR is the only real special one, because it then takes
	// 3 spaces; they are also supposed to generate _x, _y and _z fields, although
	// as of writing fteqcc only emits them for top-level vectors.. that's fun). In `definitions`, all of
	// the fields also exist as TYPE_FIELD, whose global_index points to a *global* whose value
	// is the same as the value of the *field*'s global_index (the entity index offset).
	qcvm_definition_t		*fields;
	size_t					fields_size;
	qcvm_definition_t		**field_map_by_id;
	size_t					field_real_size;
	qcvm_definition_hash_t	**field_hashes, *field_hashes_data;
	// this is the actual binary opcode data, straight list of binary opcodes.
	qcvm_statement_t		*statements;
	size_t					statements_size;
	// special .lno file which maps statements to line numbers
	int		*linenumbers;
	// functions are.. uh.. functions.
	// builtin function (in Quake2C) will always have an id of 0 and are set up
	// at run time. In QC, builtins can be set with negative values here which are
	// meant to map to specific functions, but, this is kinda silly since you
	// have the name anyways.
	qcvm_function_t		*functions;
	size_t				functions_size;
	// the size of the highest stack function.
	// this is used for allocating stack space for locals
	// that will be clobbered by other functions.
	size_t	highest_stack;
	// globals. these are a bit of a misnomer, but this is basically
	// the "heap" space that the QCVM has. This space is also used for
	// function locals (usually at the end of the table), as well as
	// function parameters and return values.
	qcvm_global_t	*global_data;
	size_t			global_size;
	// strings. just one gigantic block of string memory that should never
	// be modified. I also cache string lengths at every location (since a
	// compact string table may refer to locations that aren't the beginning
	// of a string) as well as hashed string data, for quick lookups.
	char				*string_data;
	size_t				*string_lengths;
	size_t				string_size;
	qcvm_string_hash_t	**string_hashes, *string_hashes_data;
	// see qcvm_string_list_t for more info about dynamic strings
	qcvm_string_list_t	dynamic_strings;
	// pointer to global of "strcasesensitive" in QC. this isn't a QC thing, but rather
	// my attempt to workaround stricmp being a hot spot. Function calls are expensive, so
	// rather than use functions, you can turn string case sensitiveness on/off for string
	// operators (and functions like strcmp). Three ops (set true, do operation, set false)
	// is quicker than a function call, generally.
	qcvm_global_t	*string_case_sensitive;
	// similar to strings, builtin functions have IDs of 0 and below. In "regular" QC,
	// they used specific negative values, but in modern QCVMs they all have the ID of 0
	// and are resolved by name instead.
	qcvm_builtin_list_t		builtins;
	// this is a special method of having writes to entity fields map to entity/client
	// fields in Q2, since we have special requirements like ptrs that can't exactly
	// be resolved by a simple mapping.
	qcvm_field_wrapper_t	*field_wraps;
	// fields in QC use a zero-indexed system which is basically a direct map into an entity's data.
	// think of an entity as int*, sized by the highest possible field value, and when a field is read/write
	// it's just (int *)(edicts + size)[index] as the start position. In Q1 this is all fields used by QC, and
	// "negative" indexes are system fields (ones used only by the engine). Rather than mimic this, in Quake2C
	// system fields (ones that need to be bit-accurate and are QC types, basically) are registered through this
	// and the indexes are set up, then every subsequent field is re-mapped from sizeof(edict_t) onwards. This allows
	// edict_t to be engine-agnostic and work on any engine, even if it has a modified edict_t. This + field_wraps
	// allows you to map them fully; for instance, edict_t::owner is a system field, but can't be mapped here, so it gets
	// remapped to a non-system field and then field_wraps are used to trap writes to that value and mirror the proper
	// entity pointer over to it.
	qcvm_system_field_t		*system_fields;
	size_t					system_fields_size;

	// state of the VM
	qcvm_state_t	state;
	
	// set by implementor
	// engine name
	const char	*engine_name;
	// path to progs.dat, used for relativeness for placing
	// profiles and other files
	char	path[MAX_QPATH];
	// mirrored data from globals, here so we don't "depend" on Q2's API directly
	void	*edicts;
	size_t	system_edict_size;
	size_t	edict_size;
	size_t	max_edicts;
	// callbacks for various routines
	__attribute__((noreturn)) void	(*error)(const char *str);
	void	(*warning)(const char *format, ...);
	void	(*debug_print)(const char *str);
	void	*(*alloc)(const size_t size);
	void	(*free)(void *ptr);

	// handles!
	// see qcvm_handle_list_t
	qcvm_handle_list_t handles;

	// debugging data
#ifdef ALLOW_DEBUGGING
	struct
	{
		bool attached;
		qcvm_debug_state_t state;
		qcvm_function_t *step_function;
		const qcvm_statement_t *step_statement;
		size_t step_depth;

		qcvm_mutex_t (*create_mutex)(void);
		void (*free_mutex)(qcvm_mutex_t);
		void (*lock_mutex)(qcvm_mutex_t);
		void (*unlock_mutex)(qcvm_mutex_t);
		qcvm_thread_t (*create_thread)(qcvm_thread_func_t);
		void (*thread_sleep)(const uint32_t);
	} debug;
#endif

	// instrumentation/profiling stuff
#if defined(ALLOW_INSTRUMENTING) || defined(ALLOW_PROFILING)
	struct
	{
		qcvm_profiler_flags_t	flags;
		const char				*filename;
		qcvm_profiler_mark_t	mark;

#ifdef ALLOW_INSTRUMENTING
		struct
		{
			qcvm_profile_t			*data;
			qcvm_profile_timer_t	timers[TotalTimerFields][TOTAL_MARKS];
			qcvm_profile_timer_t	opcode_timers[OP_NUMOPS][TOTAL_MARKS];
			qcvm_function_t			*func;
		} instrumentation;
#endif
#ifdef ALLOW_PROFILING
		struct
		{
			qcvm_sampling_t	*data;
			uint32_t		id, rate;
		} sampling;
#endif
	} profiling;
#endif
} qcvm_t;

__attribute__((noreturn)) void qcvm_error(const qcvm_t *vm, const char *format, ...);

// Entity stuff
inline void *qcvm_itoe(const qcvm_t *vm, const int32_t n)
{
	if (n == ENT_INVALID)
		return NULL;

	return (uint8_t *)vm->edicts + (n * vm->edict_size);
}

inline edict_t *qcvm_ent_to_entity(const qcvm_t *vm, const qcvm_ent_t ent, bool allow_invalid)
{
	if (ent == ENT_INVALID)
	{
		if (!allow_invalid)
			return NULL;
		else
			return (edict_t *)qcvm_itoe(vm, -1);
	}
	else if (ent == ENT_WORLD)
		return (edict_t *)qcvm_itoe(vm, 0);

	assert(ent >= -1 && ent < MAX_EDICTS);

	return (edict_t *)qcvm_itoe(vm, ent);
}

inline qcvm_ent_t qcvm_entity_to_ent(const qcvm_t *vm, const edict_t *ent)
{
	if (ent == NULL)
		return ENT_INVALID;

	assert(ent->s.number == ((uint8_t *)ent - (uint8_t *)vm->edicts) / vm->edict_size);

	return (qcvm_ent_t)ent->s.number;
}

inline bool qcvm_handle_resolve_pointer(const qcvm_t *vm, const qcvm_handle_pointer_t pointer, const size_t len, void **address)
{
	qcvm_handle_t *handle = qcvm_fetch_handle(vm, pointer.index);

	if (!handle->descriptor->resolve_pointer)
		qcvm_error(vm, "handle has no pointer routine");

	return handle->descriptor->resolve_pointer(vm, handle->handle, pointer.offset, len, address);
}

// Pointers
inline bool qcvm_resolve_pointer(const qcvm_t *vm, const qcvm_pointer_t pointer, const bool allow_null, const size_t len, void **address)
{
	switch (pointer.raw.type)
	{
	case QCVM_POINTER_NULL:
	default:
		if (allow_null && !len && !pointer.raw.offset)
		{
			if (address)
				*address = NULL;
			return true;
		}
		return false;
	case QCVM_POINTER_GLOBAL:
		if ((pointer.raw.offset + len) <= vm->global_size * sizeof(qcvm_global_t))
		{
			if (address)
				*address = (uint8_t *)vm->global_data + pointer.raw.offset;
			return true;
		}
		return false;
	case QCVM_POINTER_ENTITY:
		if ((pointer.raw.offset + len) <= vm->edict_size * vm->max_edicts * sizeof(qcvm_global_t))
		{
			if (address)
				*address = (uint8_t *)vm->edicts + pointer.raw.offset;
			return true;
		}
		return false;
	case QCVM_POINTER_HANDLE:
		return qcvm_handle_resolve_pointer(vm, pointer.handle, len, address);
	}
}

// this is a convenience function, but cannot make QCVM_POINTER_HANDLE pointers
inline qcvm_pointer_t qcvm_make_pointer(const qcvm_t *vm, const qcvm_pointer_type_t type, const void *pointer)
{
	switch (type)
	{
	case QCVM_POINTER_NULL:
		return (qcvm_pointer_t) { .raw = { .offset = 0, .type = type } };
	case QCVM_POINTER_GLOBAL:
		return (qcvm_pointer_t) { .raw = { .offset = (uint32_t)((const uint8_t *)pointer - (const uint8_t *)vm->global_data), .type = type } };
	case QCVM_POINTER_ENTITY:
		return (qcvm_pointer_t) { .raw = { .offset = (uint32_t)((const uint8_t *)pointer - (const uint8_t *)vm->edicts), .type = type } };
	default:
		qcvm_error(vm, "bad use of qcvm_make_pointer");
	}
}

inline qcvm_pointer_t qcvm_offset_pointer(const qcvm_t *vm, const qcvm_pointer_t pointer, const size_t offset)
{
	if (pointer.raw.type == QCVM_POINTER_HANDLE)
		return (qcvm_pointer_t) { .handle = { .offset = (uint32_t)(pointer.handle.offset + offset), .index = pointer.handle.index, .type = pointer.handle.type } };

	return (qcvm_pointer_t) { .raw = { .offset = (uint32_t)(pointer.raw.offset + offset), .type = pointer.raw.type } };
}

#ifdef _DEBUG
void qcvm_debug(const qcvm_t *vm, const char *format, ...);
const char *qcvm_dump_pointer(qcvm_t *vm, const qcvm_global_t *ptr);
#else
#define qcvm_debug(...) { }
#endif

inline void *qcvm_alloc(const qcvm_t *vm, size_t size)
{
	return vm->alloc(size);
}

inline void qcvm_mem_free(const qcvm_t *vm, void *ptr)
{
	vm->free(ptr);
}

// Store (either by copying or taking ownership)/find and assign the string in "value" (of "len" length) in "ptr".
// Returns the string ID.
qcvm_string_t qcvm_set_string_ptr(qcvm_t *vm, void *ptr, const char *value, const size_t len, const bool copy);

// Safe/simple method of setting a global to a string value.
// Returns the string ID.
inline qcvm_string_t qcvm_set_global_str(qcvm_t *vm, const qcvm_global_t global, const char *value, const size_t len, const bool copy)
{
	return qcvm_set_string_ptr(vm, qcvm_get_global(vm, global), value, len, copy);
}

// Return and arguments
inline edict_t *qcvm_argv_entity(const qcvm_t *vm, const uint8_t d)
{
	return qcvm_ent_to_entity(vm, *qcvm_get_const_global_typed(qcvm_ent_t, vm, qcvm_global_offset(GLOBAL_PARM0, d * 3)), false);
}

inline qcvm_string_t qcvm_argv_string_id(const qcvm_t *vm, const uint8_t d)
{
	return *qcvm_get_const_global_typed(qcvm_string_t, vm, qcvm_global_offset(GLOBAL_PARM0, d * 3));
}

inline const char *qcvm_argv_string(const qcvm_t *vm, const uint8_t d)
{
	return qcvm_get_string(vm, qcvm_argv_string_id(vm, d));
}

inline int32_t qcvm_argv_int32(const qcvm_t *vm, const uint8_t d)
{
	return *qcvm_get_const_global_typed(int32_t, vm, qcvm_global_offset(GLOBAL_PARM0, d * 3));
}

inline vec_t qcvm_argv_float(const qcvm_t *vm, const uint8_t d)
{
	return *qcvm_get_const_global_typed(vec_t, vm, qcvm_global_offset(GLOBAL_PARM0, d * 3));
}

inline vec3_t qcvm_argv_vector(const qcvm_t *vm, const uint8_t d)
{
	return *qcvm_get_const_global_typed(vec3_t, vm, qcvm_global_offset(GLOBAL_PARM0, d * 3));
}

inline qcvm_variant_t qcvm_argv_variant(const qcvm_t *vm, const uint8_t d)
{
	return *qcvm_get_const_global_typed(qcvm_variant_t, vm, qcvm_global_offset(GLOBAL_PARM0, d * 3));
}

inline qcvm_pointer_t qcvm_argv_pointer(const qcvm_t *vm, const uint8_t d)
{
	return *qcvm_get_const_global_typed(qcvm_pointer_t, vm, qcvm_global_offset(GLOBAL_PARM0, d * 3));
}

#define qcvm_argv_handle(type, vm, d) \
	((type *)(qcvm_fetch_handle(vm, qcvm_argv_int32(vm, (d)))->handle))

inline void qcvm_return_float(qcvm_t *vm, const vec_t value)
{
	qcvm_set_global_typed_value(vec_t, vm, GLOBAL_RETURN, value);
}

inline void qcvm_return_vector(qcvm_t *vm, const vec3_t value)
{
	qcvm_set_global_typed_value(vec3_t, vm, GLOBAL_RETURN, value);
}

inline void qcvm_return_variant(qcvm_t *vm, const qcvm_variant_t value)
{
	qcvm_set_global_typed_value(qcvm_variant_t, vm, GLOBAL_RETURN, value);
}
	
inline void qcvm_return_entity(qcvm_t *vm, const edict_t *value)
{
	const qcvm_ent_t val = qcvm_entity_to_ent(vm, value);
	qcvm_set_global_typed_value(int32_t, vm, GLOBAL_RETURN, val);
}
	
inline void qcvm_return_int32(qcvm_t *vm, const int32_t value)
{
	qcvm_set_global_typed_value(int32_t, vm, GLOBAL_RETURN, value);
}

inline void qcvm_return_func(qcvm_t *vm, const qcvm_func_t func)
{
	qcvm_set_global_typed_value(qcvm_func_t, vm, GLOBAL_RETURN, func);
}

inline void qcvm_return_string_id(qcvm_t *vm, const qcvm_string_t str)
{
	qcvm_set_global_typed_value(qcvm_func_t, vm, GLOBAL_RETURN, str);
}

inline void qcvm_return_string(qcvm_t *vm, const char *str)
{
	if (!(str >= vm->string_data && str < vm->string_data + vm->string_size))
	{
		qcvm_set_global_str(vm, GLOBAL_RETURN, str, strlen(str), true); // dynamic
		return;
	}

	const qcvm_string_t s = (str == NULL || *str == 0) ? STRING_EMPTY : (qcvm_string_t)(str - vm->string_data);
	qcvm_set_global_typed_value(qcvm_string_t, vm, GLOBAL_RETURN, s);
}

inline void qcvm_return_pointer(qcvm_t *vm, const qcvm_pointer_t ptr)
{
#ifdef _DEBUG
	if (!qcvm_resolve_pointer(vm, ptr, false, sizeof(qcvm_global_t), NULL))
		qcvm_debug(vm, "Invalid pointer returned; writes to this will fail");
#endif

	qcvm_set_global_typed_value(qcvm_pointer_t, vm, GLOBAL_RETURN, ptr);
}

inline void qcvm_return_handle(qcvm_t *vm, void *ptr, const qcvm_handle_descriptor_t *descriptor)
{
	qcvm_return_int32(vm, qcvm_handle_alloc(vm, ptr, descriptor));
}

// safe way of copying globals *of the same types* between globals
void qcvm_copy_globals(qcvm_t *vm, const qcvm_global_t dst, const qcvm_global_t src, const size_t size);

#define qcvm_copy_globals_typed(type, vm, dst, src) \
	qcvm_copy_globals(vm, dst, src, sizeof(type))

// safe way of copying globals *of different types* between globals
#define qcvm_copy_globals_safe(TDst, TSrc, vm, dst, src) \
	{ \
		assert(sizeof(TDst) == sizeof(TSrc)); \
\
		const size_t span = sizeof(TDst) / sizeof(qcvm_global_t); \
\
		const TSrc *src_ptr = qcvm_get_global_typed(TSrc, vm, src); \
		TDst *dst_ptr = qcvm_get_global_typed(TDst, vm, dst); \
\
		*(dst_ptr) = *(src_ptr); \
\
		qcvm_string_list_mark_refs_copied(vm, src_ptr, dst_ptr, span); \
		qcvm_field_wrap_list_check_set(vm, dst_ptr, span); \
	}

inline bool qcvm_strings_case_sensitive(const qcvm_t *vm)
{
	return *vm->string_case_sensitive;
}

#ifdef QCVM_INTERNAL
#ifndef _DEBUG
__attribute__((always_inline))
#endif
inline void qcvm_call_builtin(qcvm_t *vm, qcvm_function_t *function)
{
	qcvm_builtin_t func;

	if (!(func = qcvm_builtin_list_get(vm, function->id)))
		qcvm_error(vm, "Bad builtin call number");

#ifdef ALLOW_INSTRUMENTING
	qcvm_profile_t *profile = &vm->profile_data[function - vm->functions];

	if (vm->profile_flags & PROFILE_FIELDS)
		profile->fields[NumSelfCalls][vm->profiler_mark]++;
	
	uint64_t start = 0;
	qcvm_stack_t *prev_stack = NULL;
	
	if (vm->profile_flags & PROFILE_FUNCTIONS)
	{
		start = Q_time();
		prev_stack = (vm->state.current >= 0 && vm->state.stack[vm->state.current].profile) ? &vm->state.stack[vm->state.current] : NULL;

		// moving into builtin; add up what we have so far into prev stack
		if (prev_stack)
		{
			prev_stack->profile->self[vm->profiler_mark] += Q_time() - prev_stack->caller_start;
			prev_stack->callee_start = Q_time();
		}
	}
#endif

	func(vm);

#ifdef ALLOW_INSTRUMENTING
	if (vm->profile_flags & PROFILE_FUNCTIONS)
	{
		// builtins don't have external call time, just internal self time
		const uint64_t time_spent = Q_time() - start;
		profile->self[vm->profiler_mark] += time_spent;

		// add time we spent in this function into the parent's call_into time
		if (prev_stack)
			prev_stack->profile->ext[vm->profiler_mark] += Q_time() - prev_stack->callee_start;
	}
#endif
}

#ifndef _DEBUG
__attribute__((always_inline))
#endif
inline void qcvm_enter(qcvm_t *vm, qcvm_function_t *function)
{
#ifdef ALLOW_INSTRUMENTING
	if (vm->profiler_func && function == vm->profiler_func && !vm->state.profile_mark_depth)
	{
		vm->state.profile_mark_backup = vm->profiler_mark;
		vm->state.profile_mark_depth++;
		vm->profiler_mark = MARK_CUSTOM;
	}
#endif

	qcvm_stack_t *cur_stack = (vm->state.current >= 0) ? &vm->state.stack[vm->state.current] : NULL;

	// save current stack space that will be overwritten by the new function
	if (cur_stack && function->num_args_and_locals)
	{
		memcpy(cur_stack->locals, qcvm_get_global(vm, function->first_arg), sizeof(qcvm_global_t) * (function->num_args_and_locals + LOCALS_FIX));
		
		for (qcvm_global_t i = 0, arg = function->first_arg; i < (function->num_args_and_locals + LOCALS_FIX); i++, arg++)
		{
			const void *ptr = qcvm_get_global(vm, (qcvm_global_t)arg);

			if (qcvm_string_list_has_ref(vm, ptr, NULL))
				qcvm_stack_push_ref_string(cur_stack, qcvm_string_list_pop_ref(vm, ptr));
		}

#ifdef ALLOW_INSTRUMENTING
		// entering a function call;
		// add time we spent up till now into self
		if (vm->profile_flags & PROFILE_FUNCTIONS)
		{
			cur_stack->profile->self[vm->profiler_mark] += Q_time() - cur_stack->caller_start;
			cur_stack->callee_start = Q_time();
		}
#endif
	}

	qcvm_stack_t *new_stack = qcvm_state_stack_push(&vm->state);

	// set up current stack
	new_stack->function = function;
	new_stack->statement = &vm->statements[function->id - 1];

	// copy parameters
	for (qcvm_global_t i = 0, arg_id = function->first_arg; i < function->num_args; arg_id += function->arg_sizes[i], i++)
		qcvm_copy_globals(vm, arg_id, qcvm_global_offset(GLOBAL_PARM0, i * 3), sizeof(qcvm_global_t) * function->arg_sizes[i]);

#ifdef ALLOW_INSTRUMENTING
	if (vm->profile_flags & (PROFILE_FUNCTIONS | PROFILE_FIELDS))
	{
		new_stack->profile = &vm->profile_data[function - vm->functions];

		if (vm->profile_flags & PROFILE_FIELDS)
			new_stack->profile->fields[NumSelfCalls][vm->profiler_mark]++;
		if (vm->profile_flags & PROFILE_FUNCTIONS)
			new_stack->caller_start = Q_time();
	}
#endif
}

#ifndef _DEBUG
__attribute__((always_inline))
#endif
inline void qcvm_leave(qcvm_t *vm)
{
	// restore stack
	qcvm_stack_t *current_stack = &vm->state.stack[vm->state.current];
	qcvm_state_stack_pop(&vm->state);
	qcvm_stack_t *prev_stack = (vm->state.current == -1) ? NULL : &vm->state.stack[vm->state.current];

	if (prev_stack && current_stack->function->num_args_and_locals)
	{
		memcpy(qcvm_get_global(vm, current_stack->function->first_arg), prev_stack->locals, sizeof(qcvm_global_t) * (current_stack->function->num_args_and_locals + LOCALS_FIX));

		for (const qcvm_string_backup_t *str = prev_stack->ref_strings; str < prev_stack->ref_strings + prev_stack->ref_strings_size; str++)
			qcvm_string_list_push_ref(vm, str);

		prev_stack->ref_strings_size = 0;

#ifdef ALLOW_INSTRUMENTING
		if (vm->profile_flags & PROFILE_FUNCTIONS)
		{
			// we're coming back into prev_stack, so set up its caller_start
			prev_stack->caller_start = Q_time();
			// and add up the time we spent in the previous stack
			prev_stack->profile->ext[vm->profiler_mark] += Q_time() - prev_stack->callee_start;
		}
#endif
	}
		
#ifdef ALLOW_INSTRUMENTING
	if (vm->profile_flags & PROFILE_FUNCTIONS)
		current_stack->profile->self[vm->profiler_mark] += Q_time() - current_stack->caller_start;
#endif

#ifdef ALLOW_INSTRUMENTING
	if (vm->profiler_func && vm->state.profile_mark_depth)
	{
		vm->state.profile_mark_depth--;

		if (!vm->state.profile_mark_depth)
			vm->profiler_mark = vm->state.profile_mark_backup;
	}
#endif
}
#endif