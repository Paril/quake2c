#pragma once

typedef struct qcvm_s qcvm_t;

#define ALLOW_DEBUGGING
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

#include "vm_opcodes.h"

enum
{
	STRING_EMPTY
};

typedef int qcvm_string_t;

// NOTE: in QC, the value 0 is used for the world.
// The value "1" is used to mean a null entity, which is required
// to differentiate things like groundentity that can be the world.
enum
{
	ENT_WORLD = 0,
	ENT_INVALID = 1
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

#ifdef ALLOW_PROFILING
enum
{
	PROFILE_FUNCTIONS	= 1,
	PROFILE_FIELDS		= 2,
	PROFILE_TIMERS		= 4,
	PROFILE_OPCODES		= 8
};

typedef int32_t qcvm_profiler_flags_t;

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

	TotalTimerFields
};

typedef size_t qcvm_profiler_timer_id_t;

typedef struct
{
	size_t		count;
	uint64_t	time;
} qcvm_profile_timer_t;

typedef struct
{
	qcvm_profile_timer_t	*timer;
	uint64_t		start;
} qcvm_active_timer_t;

#define START_TIMER(vm, id) \
	qcvm_active_timer_t __timer; \
	if (vm->profile_flags & PROFILE_TIMERS) \
	{ \
		__timer = (qcvm_active_timer_t) { &vm->timers[id], Q_time() }; __timer.timer->count++; \
	}
#define END_TIMER(vm, flag) \
	{ if (vm->profile_flags & flag) \
		__timer.timer->time += Q_time() - __timer.start; }

#define RESUME_TIMER(vm, flag) \
	{ if (vm->profile_flags & flag) \
		__timer.start = Q_time(); }

#define START_OPCODE_TIMER(vm, id) \
	qcvm_active_timer_t __timer; \
	if (vm->profile_flags & PROFILE_OPCODES) \
	{ \
		__timer = (qcvm_active_timer_t) { &vm->opcode_timers[id], Q_time() }; __timer.timer->count++; \
	}

typedef struct
{
	uint64_t	ext, self;
	size_t		fields[TotalProfileFields];
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

typedef struct
{
	qcvm_global_t	global;
	int32_t			value;
} qcvm_stack_local_t;

enum { STACK_STRINGS_RESERVE = 64 };

typedef struct
{
	qcvm_t					*vm;
	qcvm_function_t			*function;
	const qcvm_statement_t	*statement;
	qcvm_stack_local_t		*locals;
	qcvm_string_backup_t	*ref_strings;
	size_t					ref_strings_size, ref_strings_allocated;

#ifdef ALLOW_PROFILING
	qcvm_profile_t	*profile;
	uint64_t	callee_start, caller_start;
#endif
} qcvm_stack_t;

typedef enum
{
	QCVM_POINTER_NULL,
	QCVM_POINTER_GLOBAL,
	QCVM_POINTER_ENTITY,
	QCVM_POINTER_STACK
} qcvm_pointer_type_t;

typedef struct
{
	qcvm_pointer_type_t	type : 4;
	uint32_t offset : 28;
} qcvm_pointer_t;

void qcvm_stack_needs_resize(qcvm_stack_t *stack);
void qcvm_stack_push_ref_string(qcvm_stack_t *stack, const qcvm_string_backup_t ref_string);

typedef void (*qcvm_builtin_t) (qcvm_t *vm);

typedef struct
{
	const char		*str;
	size_t			length;
	size_t			ref_count;
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
	size_t		free_indices_size, free_indices_allocated;

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

typedef void(*qcvm_field_setter_t)(void *out, const void *in);

typedef struct qcvm_field_wrapper_s
{
	const qcvm_definition_t		*field;
	size_t						field_offset, client_offset;
	qcvm_field_setter_t			setter;

	struct qcvm_field_wrapper_s	*next;
} qcvm_field_wrapper_t;

typedef struct
{
	qcvm_t					*vm;
	qcvm_field_wrapper_t	*wrap_head;
	size_t					field_range_min, field_range_max;
} qcvm_field_wrap_list_t;

void qcvm_field_wrap_list_register(qcvm_field_wrap_list_t *list, const char *field_name, const size_t field_offset, const size_t client_offset, qcvm_field_setter_t setter);

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
#ifdef ALLOW_PROFILING
	qcvm_profile_t			*profile_data;
	qcvm_profile_timer_t	timers[TotalTimerFields];
	qcvm_profile_timer_t	opcode_timers[OP_NUMOPS];
	qcvm_profiler_flags_t	profile_flags;
#endif
	qcvm_global_t			*global_data;
	size_t					global_size;
	char					*string_data;
	size_t					*string_lengths;
	size_t					string_size;
	qcvm_string_hash_t		**string_hashes, *string_hashes_data;
	qcvm_string_list_t		dynamic_strings;
	qcvm_builtin_list_t		builtins;
	qcvm_field_wrap_list_t	field_wraps;
	int						*linenumbers;
	const void				*allowed_stack;
	size_t					allowed_stack_size;
	qcvm_state_t			state;

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

qcvm_string_t qcvm_set_global_str(qcvm_t *vm, const qcvm_global_t global, const char *value);

// This is mostly an internal function, but basically assigns string to pointer.
qcvm_string_t qcvm_set_string_ptr(qcvm_t *vm, void *ptr, const char *value);

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
	}

edict_t *qcvm_ent_to_entity(const qcvm_ent_t ent, bool allow_invalid);

qcvm_ent_t qcvm_entity_to_ent(edict_t *ent);

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
qcvm_string_t qcvm_store_or_find_string(qcvm_t *vm, const char *value);

int qcvm_line_number_for(const qcvm_t *vm, const qcvm_statement_t *statement);

qcvm_func_t qcvm_find_function_id(const qcvm_t *vm, const char *name);

qcvm_function_t *qcvm_get_function(const qcvm_t *vm, const qcvm_func_t id);

qcvm_function_t *qcvm_find_function(const qcvm_t *vm, const char *name);
	
const char *qcvm_get_string(const qcvm_t *vm, const qcvm_string_t str);

size_t qcvm_get_string_length(const qcvm_t *vm, const qcvm_string_t str);

qcvm_pointer_t qcvm_get_entity_field_pointer(qcvm_t *vm, edict_t *ent, const int32_t field);

bool qcvm_pointer_valid(const qcvm_t *vm, const qcvm_pointer_t pointer, const bool allow_null, const size_t len);

qcvm_pointer_t qcvm_make_pointer(const qcvm_t *vm, const qcvm_pointer_type_t type, const void *pointer);

void *qcvm_resolve_pointer(const qcvm_t *vm, const qcvm_pointer_t pointer);

const char *qcvm_stack_entry(const qcvm_t *vm, const qcvm_stack_t *s);

const char *qcvm_stack_trace(const qcvm_t *vm);

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
void qcvm_enter(qcvm_t *vm, qcvm_function_t *function);
void qcvm_leave(qcvm_t *vm);
#endif