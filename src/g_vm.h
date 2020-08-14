#pragma once

typedef struct qcvm_s qcvm_t;

//#define ALLOW_DEBUGGING
//#define ALLOW_INSTRUMENTING
#define ALLOW_PROFILING

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

#include "vm_opcodes.h"

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

enum
{
	QCVM_POINTER_NULL,
	QCVM_POINTER_GLOBAL,
	QCVM_POINTER_ENTITY,
	QCVM_POINTER_STACK
};

typedef uint32_t qcvm_pointer_type_t;

typedef struct
{
	uint32_t			offset : 30;
	qcvm_pointer_type_t	type : 2;
} qcvm_pointer_t;

void qcvm_stack_push_ref_string(qcvm_stack_t *stack, const qcvm_string_backup_t ref_string);

typedef void (*qcvm_builtin_t) (qcvm_t *vm);

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

typedef struct
{
	qcvm_t	*vm;

	// Mapped list to dynamic strings
	qcvm_ref_counted_string_t	*strings;
	size_t						strings_size, strings_allocated;

	// stores a list of free indices that were explicitly freed
	qcvm_string_t	*free_indices;
	size_t			free_indices_size, free_indices_allocated;

	// mapped list of addresses that contain(ed) strings
	qcvm_ref_storage_hash_t		*ref_storage_data, **ref_storage_hashes, *ref_storage_free;
	size_t						ref_storage_stored, ref_storage_allocated;
} qcvm_string_list_t;

// Note: ownership of the pointer is transferred to the string list here.
qcvm_string_t qcvm_string_list_store(qcvm_string_list_t *list, const char *str, const size_t len);
void qcvm_string_list_unstore(qcvm_string_list_t *list, const qcvm_string_t id);
size_t qcvm_string_list_get_length(const qcvm_string_list_t *list, const qcvm_string_t id);
const char *qcvm_string_list_get(const qcvm_string_list_t *list, const qcvm_string_t id);
void qcvm_string_list_acquire(qcvm_string_list_t *list, const qcvm_string_t id);
void qcvm_string_list_release(qcvm_string_list_t *list, const qcvm_string_t id);
// mark a memory address as containing a reference to the specified string.
// increases ref count by 1 and shoves it into the list.
void qcvm_string_list_mark_ref_copy(qcvm_string_list_t *list, const qcvm_string_t id, const void *ptr);
bool qcvm_string_list_check_ref_unset(qcvm_string_list_t *list, const void *ptr, const size_t span, const bool assume_changed/* = false*/);
void qcvm_string_list_mark_refs_copied(qcvm_string_list_t *list, const void *src, const void *dst, const size_t span);
bool qcvm_string_list_is_ref_counted(qcvm_string_list_t *list, const qcvm_string_t id);
qcvm_string_backup_t qcvm_string_list_pop_ref(qcvm_string_list_t *list, const void *ptr);
void qcvm_string_list_push_ref(qcvm_string_list_t *list, const qcvm_string_backup_t *backup);

#ifdef _DEBUG
const char *qcvm_dump_pointer(qcvm_t *vm, const qcvm_global_t *ptr);
void qcvm_string_list_dump_refs(FILE *fp, qcvm_string_list_t *list);
#endif

typedef struct
{
	qcvm_t			*vm;
	qcvm_builtin_t	*list;
	size_t			count, registered;
} qcvm_builtin_list_t;

void qcvm_builtin_list_register(qcvm_builtin_list_t *list, const char *name, qcvm_builtin_t builtin);

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

typedef struct
{
	qcvm_t					*vm;
	qcvm_field_wrapper_t	*wraps;
} qcvm_field_wrap_list_t;

void qcvm_field_wrap_list_register(qcvm_field_wrap_list_t *list, const char *field_name, const size_t field_offset, const size_t client_offset, qcvm_field_setter_t setter);
void qcvm_field_wrap_list_check_set(qcvm_field_wrap_list_t *list, const void *ptr, const size_t span);

#ifdef ALLOW_DEBUGGING
typedef struct
{
	qcvm_deftype_t		type;

	union
	{
		int	integer;
		float single;
		vec3_t vector;
		qcvm_string_t strid;
		qcvm_ent_t entid;
		qcvm_func_t funcid;
		void *ptr;
	};
} qcvm_eval_result_t;
#endif

static const size_t STACK_RESERVE = 32;

typedef struct
{
	qcvm_t *vm;
	qcvm_stack_t *stack;
	size_t stack_size, stack_allocated;
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

inline qcvm_global_t qcvm_global_offset(const qcvm_global_t base, const int32_t offset)
{
	return (qcvm_global_t)((int32_t)base + offset);
}

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

#ifdef ALLOW_PROFILING
typedef struct
{
	uint64_t	count[TOTAL_MARKS];
} qcvm_sampling_t;
#endif

typedef struct
{
	void	(*free)		(qcvm_t *vm, void *handle);
	void	(*write)	(qcvm_t *vm, void *handle, FILE *fp);
	void	*(*read)	(qcvm_t *vm, FILE *fp);
} qcvm_handle_descriptor_t;

typedef int32_t qcvm_handle_id_t;

typedef struct
{
	qcvm_handle_id_t				id;
	const qcvm_handle_descriptor_t	*descriptor;
	void							*handle;
} qcvm_handle_t;

static const size_t HANDLES_RESERVE = 128;

int32_t qcvm_handle_alloc(qcvm_t *vm, void *ptr, const qcvm_handle_descriptor_t *descriptor);

void *qcvm_fetch_handle(qcvm_t *vm, const int32_t id);

void qcvm_handle_free(qcvm_t *vm, qcvm_handle_t *handle);

#define qcvm_argv_handle(type, vm, d) \
	(type *)qcvm_fetch_handle(vm, qcvm_argv_int32(vm, d))

typedef struct qcvm_s
{
	// loaded from progs.dat
	const char				*engine_name;
	char					path[MAX_QPATH];
	qcvm_definition_t		*definitions;
	size_t					definitions_size;
	qcvm_definition_t		**definition_map_by_id;
	qcvm_definition_hash_t	**definition_hashes, *definition_hashes_data;
	qcvm_definition_t		*fields;
	size_t					fields_size;
	qcvm_definition_t		**field_map_by_id;
	size_t					field_real_size;
	qcvm_definition_hash_t	**field_hashes, *field_hashes_data;
	qcvm_statement_t		*statements;
	size_t					statements_size;
	qcvm_function_t			*functions;
	size_t					functions_size;
	size_t					highest_stack;
#if defined(ALLOW_INSTRUMENTING) || defined(ALLOW_PROFILING)
	qcvm_profiler_flags_t	profile_flags;
	const char				*profile_name;
	qcvm_profiler_mark_t	profiler_mark;
#endif
#ifdef ALLOW_INSTRUMENTING
	qcvm_profile_t			*profile_data;
	qcvm_profile_timer_t	timers[TotalTimerFields][TOTAL_MARKS];
	qcvm_profile_timer_t	opcode_timers[OP_NUMOPS][TOTAL_MARKS];
	qcvm_function_t			*profiler_func;
#endif
#ifdef ALLOW_PROFILING
	qcvm_sampling_t			*sample_data;
	uint32_t				sample_id, sample_rate;
#endif
	qcvm_global_t			*global_data;
	size_t					global_size;
	char					*string_data;
	size_t					*string_lengths;
	size_t					string_size;
	qcvm_string_hash_t		**string_hashes, *string_hashes_data;
	qcvm_global_t			*string_case_sensitive;
	qcvm_string_list_t		dynamic_strings;
	qcvm_builtin_list_t		builtins;
	qcvm_field_wrap_list_t	field_wraps;
	int						*linenumbers;
	const void				*allowed_stack;
	size_t					allowed_stack_size;
	qcvm_state_t			state;
	qcvm_system_field_t		*system_fields;
	size_t					system_fields_size;
	
	// set by implementor
	void					*edicts;
	size_t					system_edict_size;
	size_t					edict_size;
	size_t					max_edicts;

	struct {
		qcvm_handle_t	*data;
		size_t			size, allocated;
		int32_t			*free;
		size_t			free_size;
	} handles;

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

	// callbacks set by implementor
	void					(*error)(const char *str);
	void					(*warning)(const char *format, ...);
	void					(*debug_print)(const char *str);
	void					*(*alloc)(const size_t size);
	void					(*free)(void *ptr);
} qcvm_t;

void qcvm_error(const qcvm_t *vm, const char *format, ...);

#ifdef _DEBUG
void qcvm_debug(const qcvm_t *vm, const char *format, ...);
#else
#define qcvm_debug(...) { }
#endif

void *qcvm_alloc(const qcvm_t *vm, size_t size);
void qcvm_mem_free(const qcvm_t *vm, void *ptr);

void qcvm_set_allowed_stack(qcvm_t *vm, const void *ptr, const size_t length);
qcvm_global_t *qcvm_get_global(qcvm_t *vm, const qcvm_global_t g);
const qcvm_global_t *qcvm_get_const_global(const qcvm_t *vm, const qcvm_global_t g);

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

qcvm_string_t qcvm_set_global_str(qcvm_t *vm, const qcvm_global_t global, const char *value, const size_t len, const bool copy);

// This is mostly an internal function, but basically assigns string to pointer.
qcvm_string_t qcvm_set_string_ptr(qcvm_t *vm, void *ptr, const char *value, const size_t len, const bool copy);

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
		qcvm_string_list_mark_refs_copied(&vm->dynamic_strings, src_ptr, dst_ptr, span); \
		qcvm_field_wrap_list_check_set(&vm->field_wraps, dst_ptr, span); \
	}

inline bool qcvm_strings_case_sensitive(const qcvm_t *vm)
{
	return *vm->string_case_sensitive;
}

edict_t *qcvm_ent_to_entity(const qcvm_t *vm, const qcvm_ent_t ent, bool allow_invalid);

qcvm_ent_t qcvm_entity_to_ent(const qcvm_t *vm, const edict_t *ent);

edict_t *qcvm_argv_entity(const qcvm_t *vm, const uint8_t d);

qcvm_string_t qcvm_argv_string_id(const qcvm_t *vm, const uint8_t d);

const char *qcvm_argv_string(const qcvm_t *vm, const uint8_t d);

int32_t qcvm_argv_int32(const qcvm_t *vm, const uint8_t d);

vec_t qcvm_argv_float(const qcvm_t *vm, const uint8_t d);

vec3_t qcvm_argv_vector(const qcvm_t *vm, const uint8_t d);

qcvm_pointer_t qcvm_argv_pointer(const qcvm_t *vm, const uint8_t d);

void qcvm_return_float(qcvm_t *vm, const vec_t value);

void qcvm_return_vector(qcvm_t *vm, const vec3_t value);
	
void qcvm_return_entity(qcvm_t *vm, const edict_t *value);
	
void qcvm_return_int32(qcvm_t *vm, const int32_t value);

void qcvm_return_func(qcvm_t *vm, const qcvm_func_t func);

void qcvm_return_string_id(qcvm_t *vm, const qcvm_string_t str);

void qcvm_return_string(qcvm_t *vm, const char *str);

void qcvm_return_pointer(qcvm_t *vm, const qcvm_pointer_t ptr);

bool qcvm_find_string(qcvm_t *vm, const char *value, qcvm_string_t *rstr);

// Note: DOES NOT ACQUIRE IF REF COUNTED!!
// Note: currently *copies* value if it's acquired
qcvm_string_t qcvm_store_or_find_string(qcvm_t *vm, const char *value, const size_t len, const bool copy);

qcvm_string_t *qcvm_string_list_has_ref(qcvm_string_list_t *list, const void *ptr, qcvm_ref_storage_hash_t **hashed_ptr);

qcvm_definition_t *qcvm_find_definition(qcvm_t *vm, const char *name, const qcvm_deftype_t type);

qcvm_definition_t *qcvm_find_field(qcvm_t *vm, const char *name);

int qcvm_line_number_for(const qcvm_t *vm, const qcvm_statement_t *statement);

qcvm_func_t qcvm_find_function_id(const qcvm_t *vm, const char *name);

qcvm_function_t *qcvm_get_function(const qcvm_t *vm, const qcvm_func_t id);

qcvm_function_t *qcvm_find_function(const qcvm_t *vm, const char *name);
	
const char *qcvm_get_string(const qcvm_t *vm, const qcvm_string_t str);

size_t qcvm_get_string_length(const qcvm_t *vm, const qcvm_string_t str);

void *qcvm_itoe(const qcvm_t *vm, const int32_t n);

qcvm_pointer_t qcvm_get_entity_field_pointer(qcvm_t *vm, edict_t *ent, const int32_t field);

bool qcvm_pointer_valid(const qcvm_t *vm, const qcvm_pointer_t pointer, const bool allow_null, const size_t len);

qcvm_pointer_t qcvm_make_pointer(const qcvm_t *vm, const qcvm_pointer_type_t type, const void *pointer);

qcvm_pointer_t qcvm_offset_pointer(const qcvm_t *vm, const qcvm_pointer_t pointer, const size_t offset);

void *qcvm_resolve_pointer(const qcvm_t *vm, const qcvm_pointer_t pointer);

const char *qcvm_stack_entry(const qcvm_t *vm, const qcvm_stack_t *s, const bool compact);

const char *qcvm_stack_trace(const qcvm_t *vm, const bool compact);

void qcvm_call_builtin(qcvm_t *vm, qcvm_function_t *function);

void qcvm_execute(qcvm_t *vm, qcvm_function_t *function);
	
void qcvm_write_state(qcvm_t *vm, FILE *fp);

void qcvm_read_state(qcvm_t *vm, FILE *fp);

const char *qcvm_parse_format(const qcvm_string_t formatid, const qcvm_t *vm, const uint8_t start);

void qcvm_load(qcvm_t *vm, const char *engine_name, const char *filename);

void qcvm_check(qcvm_t *vm);

void qcvm_shutdown(qcvm_t *vm);

// Helpful macro for quickly registering a builtin
#define qcvm_register_builtin(name) \
	qcvm_builtin_list_register(&vm->builtins, #name, QC_ ## name)

void qcvm_init_all_builtins(qcvm_t *vm);

#ifdef QCVM_INTERNAL
__attribute__((always_inline)) inline void qcvm_enter(qcvm_t *vm, qcvm_function_t *function)
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
		memcpy(cur_stack->locals, qcvm_get_global(vm, function->first_arg), sizeof(qcvm_global_t) * function->num_args_and_locals);
		
		for (qcvm_global_t i = 0, arg = function->first_arg; i < function->num_args_and_locals; i++, arg++)
		{
			const void *ptr = qcvm_get_global(vm, (qcvm_global_t)arg);

			if (qcvm_string_list_has_ref(&vm->dynamic_strings, ptr, NULL))
				qcvm_stack_push_ref_string(cur_stack, qcvm_string_list_pop_ref(&vm->dynamic_strings, ptr));
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
	for (qcvm_global_t i = 0, arg_id = function->first_arg; i < function->num_args; i++)
		for (qcvm_global_t s = 0; s < function->arg_sizes[i]; s++, arg_id++)
			qcvm_copy_globals_typed(qcvm_global_t, vm, arg_id, qcvm_global_offset(GLOBAL_PARM0, (i * 3) + s));

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

__attribute__((always_inline)) inline void qcvm_leave(qcvm_t *vm)
{
	// restore stack
	qcvm_stack_t *current_stack = &vm->state.stack[vm->state.current];
	qcvm_state_stack_pop(&vm->state);
	qcvm_stack_t *prev_stack = (vm->state.current == -1) ? NULL : &vm->state.stack[vm->state.current];

	if (prev_stack && current_stack->function->num_args_and_locals)
	{
		memcpy(qcvm_get_global(vm, current_stack->function->first_arg), prev_stack->locals, sizeof(qcvm_global_t) * current_stack->function->num_args_and_locals);

		for (const qcvm_string_backup_t *str = prev_stack->ref_strings; str < prev_stack->ref_strings + prev_stack->ref_strings_size; str++)
			qcvm_string_list_push_ref(&vm->dynamic_strings, str);

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

	vm->allowed_stack = 0;
	vm->allowed_stack_size = 0;

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