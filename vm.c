#define QCVM_INTERNAL
#include "shared/shared.h"
#include "vm.h"
#include "vm_game.h"
#include "vm_math.h"
#include "vm_debug.h"
#include "vm_mem.h"
#include "vm_string.h"
#include "vm_ext.h"
#include "vm_file.h"
#include "vm_hash.h"
#include "vm_structlist.h"
#include "vm_list.h"
#include "vm_heap.h"
#include "vm_opcodes.h"

#include <time.h>

typedef struct
{
	uint32_t	offset;
	uint32_t	size;
} qcvm_offset_t;

enum
{
	PROGS_Q1	= 6,
	PROGS_FTE	= 7,

	PROG_SECONDARYVERSION16 = ((('1'<<0)|('F'<<8)|('T'<<16)|('E'<<24))^(('P'<<0)|('R'<<8)|('O'<<16)|('G'<<24))),
	PROG_SECONDARYVERSION32 = ((('1'<<0)|('F'<<8)|('T'<<16)|('E'<<24))^(('3'<<0)|('2'<<8)|('B'<<16)|(' '<<24)))
};

typedef uint32_t progs_version_t;

typedef struct
{
	progs_version_t	version;
	uint16_t		crc;
	uint16_t		skip;

	struct {
		qcvm_offset_t	statement;
		qcvm_offset_t	definition;
		qcvm_offset_t	field;
		qcvm_offset_t	function;
		qcvm_offset_t	string;
		qcvm_offset_t	globals;
	} sections;

	uint32_t		entityfields;

	uint32_t		ofs_files;	//non list format. no comp
	uint32_t		ofs_linenums;	//numstatements big	//comp 64
	qcvm_offset_t	bodylessfuncs;
	qcvm_offset_t	types;

	uint32_t		blockscompressed;

	progs_version_t	secondary_version;
} qcvm_header_t;

static inline void qcvm_stack_needs_resize(qcvm_stack_t *stack)
{
	qcvm_string_backup_t *old_ref_strings = stack->ref_strings;
	stack->ref_strings_allocated += STACK_STRINGS_RESERVE;
	stack->ref_strings = (qcvm_string_backup_t *)(qcvm_alloc(stack->vm, sizeof(qcvm_string_backup_t) * stack->ref_strings_allocated));

	if (old_ref_strings)
	{
		memcpy(stack->ref_strings, old_ref_strings, sizeof(qcvm_string_backup_t) * stack->ref_strings_size);
		qcvm_mem_free(stack->vm, old_ref_strings);
	}

#ifdef _DEBUG
	if (stack->ref_strings_allocated != STACK_STRINGS_RESERVE)
		qcvm_debug(stack->vm, "Stack string count extended to %i\n", stack->ref_strings_allocated);
#endif
}

void qcvm_stack_push_ref_string(qcvm_stack_t *stack, const qcvm_string_backup_t ref_string)
{
	if (stack->ref_strings_size == stack->ref_strings_allocated)
		qcvm_stack_needs_resize(stack);

	stack->ref_strings[stack->ref_strings_size++] = ref_string;
}

qcvm_builtin_t qcvm_builtin_list_get(qcvm_t *vm, const qcvm_func_t func)
{
	assert(func < 0);

	const int32_t index = (-func) - 1;

	assert(index < vm->builtins.count);

	return vm->builtins.list[index];
}

void qcvm_builtin_list_register(qcvm_t *vm, const char *name, qcvm_builtin_t builtin)
{
	qcvm_builtin_list_t *list = &vm->builtins;

	for (qcvm_function_t *func = vm->functions; func < vm->functions + vm->functions_size; func++)
	{
		if (func->id || func->name_index == STRING_EMPTY)
			continue;

		if (strcmp(qcvm_get_string(vm, func->name_index), name) == 0)
		{
			if (list->registered == list->count)
				qcvm_error(vm, "Builtin list overrun");

			const int32_t index = (int32_t) list->registered;
			func->id = (qcvm_func_t)(-(index + 1));
			list->list[index] = builtin;
			list->registered++;
			return;
		}
	}

	qcvm_debug(vm, "No builtin to assign to %s\n", name);
}

void qcvm_register_system_field(qcvm_t *vm, const char *field_name, const size_t field_offset, const size_t field_span)
{
	qcvm_definition_t *def = qcvm_find_definition(vm, field_name, TYPE_FIELD);

	if (!def)
	{
		qcvm_debug(vm, "field definition not found for mapping: %s\n", field_name);
		return;
	}

	qcvm_definition_t *field = qcvm_find_field(vm, field_name);

	if (!field)
	{
		qcvm_debug(vm, "field not found for mapping: %s\n", field_name);
		return;
	}

	if (vm->system_fields_size == vm->fields_size)
		qcvm_error(vm, "system fields overrun");

	if ((field_offset + field_span) > vm->system_edict_size)
		qcvm_error(vm, "system fields overrun");

	qcvm_system_field_t *system_field = &vm->system_fields[vm->system_fields_size++];
	system_field->def = def;
	system_field->field = field;
	system_field->offset = (qcvm_global_t) field_offset;
	system_field->span = field_span;
}

const char *qcvm_parse_format(const qcvm_string_t formatid, const qcvm_t *vm, const uint8_t start)
{
	typedef enum
	{
		PT_NONE,
		PT_SPECIFIER,
		PT_SKIP
	} ParseToken;

	static char buffer[0x800];
	size_t i = 0;
	const size_t len = qcvm_get_string_length(vm, formatid);
	static char format_buffer[17];
	uint8_t param_index = start;
	const char *format = qcvm_get_string(vm, formatid);
	char *p = buffer;
	size_t len_left = sizeof(buffer) - 1;

	while (true)
	{
		const char *next = strchr(format + i, '%');

		if (!next)
		{
			const size_t write_len = (format + len) - (format + i);
			if (len_left < write_len)
				qcvm_error(vm, "qcvm_parse_format overflow");

			strncpy(p, format + i, write_len);
			p += write_len;
			len_left -= write_len;
			break;
		}

		size_t write_len = (next - format) - i;
		if (len_left < write_len)
			qcvm_error(vm, "qcvm_parse_format overflow");
		strncpy(p, format + i, write_len);
		p += write_len;
		len_left -= write_len;
		i = next - format;

		const char *specifier_start = next;
		ParseToken state = PT_NONE;

		while (state < PT_SPECIFIER)
		{
			next++;
			i++;

			// check specifier
			switch (*next)
			{
			case 'd':
			case 'i':
			case 'o':
			case 'x':
			case 'X':
			case 'f':
			case 'F':
			case 'e':
			case 'E':
			case 'g':
			case 'G':
			case 'a':
			case 'A':
			case 'c':
			case 's':
			case 'p':
				state = PT_SPECIFIER;
				continue;
			case '%':
				if (len_left < 1)
					qcvm_error(vm, "qcvm_parse_format overflow");
		
				*p = '%';
				p++;
				len_left--;
				state = PT_SKIP;
				continue;
			}
		}

		if (state == PT_SPECIFIER)
		{
			Q_strlcpy(format_buffer, specifier_start, minsz(sizeof(format_buffer), (size_t)((next - specifier_start) + 1 + 1)));

			const char *formatted;

			switch (*next)
			{
			case 'd':
			case 'i':
			case 'o':
			case 'x':
			case 'X':
			case 'c':
			case 'p':
				formatted = qcvm_temp_format(vm, format_buffer, qcvm_argv_int32(vm, param_index++));
				break;
			case 'f':
			case 'F':
			case 'e':
			case 'E':
			case 'g':
			case 'G':
			case 'a':
			case 'A':
				formatted = qcvm_temp_format(vm, format_buffer, qcvm_argv_float(vm, param_index++));
				break;
			case 's':
				formatted = qcvm_temp_format(vm, format_buffer, qcvm_argv_string(vm, param_index++));
				break;
			default:
				qcvm_error(vm, "invalid specifier");
			}
			
			write_len = strlen(formatted);
			if (len_left < write_len)
				qcvm_error(vm, "qcvm_parse_format overflow");
			strncpy(p, formatted, write_len);
			p += write_len;
			len_left -= write_len;
		}

		i++;
	}

	*p = 0;
	return buffer;
}

static void qcvm_field_wrap_list_init(qcvm_t *vm)
{
	vm->field_wraps = (qcvm_field_wrapper_t *)qcvm_alloc(vm, sizeof(qcvm_field_wrapper_t) * vm->field_real_size);
}

void qcvm_field_wrap_list_register(qcvm_t *vm, const char *field_name, const size_t field_offset, const size_t struct_offset, qcvm_field_setter_t setter)
{
	for (qcvm_definition_t *f = vm->fields; f < vm->fields + vm->fields_size; f++)
	{
		if (f->name_index == STRING_EMPTY)
			continue;
		else if (strcmp(qcvm_get_string(vm, f->name_index), field_name))
			continue;

		assert((f->global_index + field_offset) < vm->field_real_size);

		qcvm_field_wrapper_t *wrapper = &vm->field_wraps[f->global_index + field_offset];
		*wrapper = (qcvm_field_wrapper_t) {
			f,
			f->global_index + field_offset,
			strncmp(field_name, "client.", 7) == 0,
			struct_offset,
			setter
		};
		return;
	}

	vm->warning("QCVM WARNING: can't find field %s in progs\n", field_name);
}

void qcvm_field_wrap_list_check_set(qcvm_t *vm, const void *ptr, const size_t span)
{
	// FIXME: this shouldn't be required ideally...
	if (!vm)
		return;

	START_TIMER(vm, WrapApply);

	// no entities involved in this wrap check (or no entities to check yet)
	if (ptr < vm->edicts || ptr >= (void *)((uint8_t *)vm->edicts + (vm->edict_size * vm->max_edicts)))
	{
		END_TIMER(vm, PROFILE_TIMERS);
		return;
	}

	// check where we're starting
	void *start = (void *)((uint8_t *)ptr - (uint8_t *)vm->edicts);
	edict_t *ent = (edict_t *)qcvm_itoe(vm, (int32_t)((ptrdiff_t)start / vm->edict_size));
	size_t offset = (const uint32_t *)ptr - (uint32_t *)ent;
	const int32_t *sptr = (const int32_t *)ptr;

	for (size_t i = 0; i < span; i++, sptr++, offset++)
	{
		// we're wrapping over to a new entity
		if (offset >= vm->field_real_size)
		{
			ent++;
			offset = 0;
		}

		const qcvm_field_wrapper_t *wrap = &vm->field_wraps[offset];

		if (!wrap->field)
			continue;

		if (wrap->field_offset != offset)
			continue;

		// client wraps may attempt to write to them on non-clients
		// during memcpy, etc
		if (wrap->is_client && !ent->client)
			continue;

		void *dst = (void *)(((wrap->is_client) ? (uint8_t *)ent->client : (uint8_t *)ent) + wrap->struct_offset);
			
		if (wrap->setter)
			wrap->setter(dst, sptr);
		else
			*(int32_t *)dst = *sptr;
	}

	END_TIMER(vm, PROFILE_TIMERS);
}

static void qcvm_state_init(qcvm_state_t *state, qcvm_t *vm)
{
	state->vm = vm;
	state->current = -1;
}

static void qcvm_state_free(qcvm_state_t *state)
{
	for (int32_t i = 0; i < state->stack_allocated; i++)
	{
		qcvm_mem_free(state->vm, state->stack[i].locals);
		qcvm_mem_free(state->vm, state->stack[i].ref_strings);
	}

	qcvm_mem_free(state->vm, state->stack);
}

void qcvm_state_needs_resize(qcvm_state_t *state)
{
	qcvm_stack_t *old_stack = state->stack;
	state->stack_allocated += STACK_RESERVE;
	state->stack = (qcvm_stack_t *)qcvm_alloc(state->vm, sizeof(qcvm_stack_t) * state->stack_allocated);

	if (old_stack)
	{
		memcpy(state->stack, old_stack, sizeof(qcvm_stack_t) * state->current);
		qcvm_mem_free(state->vm, old_stack);
	}

	for (int32_t i = state->current; i < state->stack_allocated; i++)
	{
		qcvm_stack_t *new_stack = state->stack + i;
		new_stack->vm = state->vm;
		new_stack->locals = (qcvm_global_t *)qcvm_alloc(state->vm, sizeof(qcvm_global_t) * state->vm->highest_stack);
		qcvm_stack_needs_resize(new_stack);
	}

	if (state->stack_allocated != STACK_RESERVE)
		qcvm_debug(state->vm, "Stack size increased to %i\n", state->stack_allocated);
}

qcvm_stack_t *qcvm_state_stack_push(qcvm_state_t *state)
{
	state->current++;

	if ((size_t)state->current == state->stack_allocated)
		qcvm_state_needs_resize(state);

	return &state->stack[state->current];
}

void qcvm_state_stack_pop(qcvm_state_t *state)
{
	if (state->current <= -1)
		qcvm_error(state->vm, "stack underflow");
	state->current--;
}

static void qcvm_init(qcvm_t *vm)
{
	qcvm_state_init(&vm->state, vm);
	Q_srand((uint32_t)time(NULL));
}

static void qcvm_free(qcvm_t *vm)
{
	qcvm_mem_free(vm, vm->string_hashes);
	qcvm_mem_free(vm, vm->string_hashes_data);
	qcvm_state_free(&vm->state);
}

void qcvm_error(const qcvm_t *vm, const char *format, ...)
{
	va_list         argptr;
	static char		buffer[512];

	va_start(argptr, format);
	vsnprintf(buffer, sizeof(buffer), format, argptr);
	va_end(argptr);

	// this breaks const-correctness, but since this is for debugging I think it's
	// fine.
#if ALLOW_DEBUGGING
	qcvm_break_on_current_statement((qcvm_t *)vm);
#ifdef WINDOWS
	__debugbreak();
#endif
#endif

	vm->error(buffer);
}

#ifdef _DEBUG
void qcvm_debug(const qcvm_t *vm, const char *format, ...)
{
	va_list         argptr;
	static char		buffer[512];

	va_start(argptr, format);
	vsnprintf(buffer, sizeof(buffer), format, argptr);
	va_end(argptr);

	vm->debug_print(buffer);
}



#ifdef _DEBUG
static const char *qcvm_function_for_local(qcvm_t *vm, const qcvm_global_t local)
{
	qcvm_global_t closest = -1;
	qcvm_function_t *closest_func = NULL;

	for (qcvm_function_t *func = vm->functions; func < vm->functions + vm->functions_size; func++)
	{
		if (local < func->first_arg)
			continue;
		else if (local == func->first_arg)
		{
			closest_func = func;
			break;
		}
		else if ((local - func->first_arg) < closest)
		{
			closest = (local - func->first_arg);
			closest_func = func;
		}
	}

	if (closest_func && closest_func->name_index)
		return qcvm_get_string(vm, closest_func->name_index);

	return NULL;
}

const char *qcvm_dump_pointer(qcvm_t *vm, const qcvm_global_t *ptr)
{
	if (ptr >= vm->global_data && ptr < (vm->global_data + vm->global_size))
	{
		const qcvm_global_t offset = (qcvm_global_t)(ptr - vm->global_data);
		qcvm_global_t closest = -1;
		qcvm_definition_t *closest_def = NULL;

		for (qcvm_definition_t *def = vm->definitions; def < vm->definitions + vm->definitions_size; def++)
		{
			if (def->global_index == offset)
			{
				closest = def->global_index;
				closest_def = def;
				break;
			}
			else if (def->global_index < offset)
			{
				if ((offset - def->global_index) < closest)
				{
					closest = offset - def->global_index;
					closest_def = def;
				}
			}
		}

		if (closest == -1 || !closest_def || !closest_def->name_index)
			return qcvm_temp_format(vm, "global %u", offset);

		const char *func = qcvm_function_for_local(vm, offset);

		if (func)
			return qcvm_temp_format(vm, "global %s::%s + %u[%u]", func, qcvm_get_string(vm, closest_def->name_index), closest, offset);

		return qcvm_temp_format(vm, "global %s + %u[%u]", qcvm_get_string(vm, closest_def->name_index), closest, offset);
	}
	else if (ptr >= (qcvm_global_t *)vm->edicts && ptr < (qcvm_global_t *)vm->edicts + (vm->edict_size * vm->max_edicts))
	{
		const edict_t *ent = qcvm_ent_to_entity(vm, (qcvm_ent_t)ptr, false);
		const qcvm_global_t field_offset = (qcvm_global_t)(ptr - (qcvm_global_t *)ent);

		if (field_offset < vm->field_real_size)
		{
			const qcvm_definition_t *def = vm->field_map_by_id[field_offset];
			size_t offset;

			for (offset = 0; !def; def = vm->field_map_by_id[field_offset - (++offset)]) ;

			if (def)
			{
				if (offset)
					return qcvm_temp_format(vm, "field %u::%s + %u[%u]", ent->s.number, qcvm_get_string(vm, def->name_index), offset, field_offset);
				
				return qcvm_temp_format(vm, "field %u::%s[%u]", ent->s.number, qcvm_get_string(vm, def->name_index), field_offset);
			}
		}

		return qcvm_temp_format(vm, "field %u::%u ???", ent->s.number, field_offset);
	}

	return "???";
}
#endif
#endif

qcvm_global_t *qcvm_get_global(qcvm_t *vm, const qcvm_global_t g)
{
#if ALLOW_INSTRUMENTING
	if ((vm->profiling.flags & PROFILE_FIELDS) && vm->state.current >= 0 && vm->state.stack[vm->state.current].profile)
		vm->state.stack[vm->state.current].profile->fields[NumGlobalsFetched][vm->profiling.mark]++;
#endif

	return vm->global_data + g;
}

const qcvm_global_t *qcvm_get_const_global(const qcvm_t *vm, const qcvm_global_t g)
{
#if ALLOW_INSTRUMENTING
	if ((vm->profiling.flags & PROFILE_FIELDS) && vm->state.current >= 0 && vm->state.stack[vm->state.current].profile)
		vm->state.stack[vm->state.current].profile->fields[NumGlobalsFetched][vm->profiling.mark]++;
#endif

	return vm->global_data + g;
}

void *qcvm_get_global_ptr(qcvm_t *vm, const qcvm_global_t global, const size_t value_size)
{
	const qcvm_pointer_t pointer = *(const qcvm_pointer_t*)qcvm_get_const_global(vm, global);

	assert((value_size % 4) == 0);

	void *address;

	if (!qcvm_resolve_pointer(vm, pointer, false, value_size, &address))
		qcvm_error(vm, "bad address");

	return address;
}

void qcvm_set_global(qcvm_t *vm, const qcvm_global_t global, const void *value, const size_t value_size)
{
#if ALLOW_INSTRUMENTING
	if ((vm->profiling.flags & PROFILE_FIELDS) && vm->state.current >= 0 && vm->state.stack[vm->state.current].profile)
		vm->state.stack[vm->state.current].profile->fields[NumGlobalsSet][vm->profiling.mark]++;
#endif

	if (global == GLOBAL_NULL)
		qcvm_error(vm, "attempt to overwrite 0");

	assert((value_size % 4) == 0);

	void *dst = qcvm_get_global(vm, global);
	memcpy(dst, value, value_size);
	qcvm_string_list_check_ref_unset(vm, dst, value_size / sizeof(qcvm_global_t), false);
	qcvm_field_wrap_list_check_set(vm, dst, value_size / sizeof(qcvm_global_t));
}

// safe way of copying globals between other globals
void qcvm_copy_globals(qcvm_t *vm, const qcvm_global_t dst, const qcvm_global_t src, const size_t size)
{
	const size_t span = size / sizeof(qcvm_global_t);

	const void *src_ptr = qcvm_get_global(vm, src);
	void *dst_ptr = qcvm_get_global(vm, dst);

	memcpy(dst_ptr, src_ptr, size);

	qcvm_string_list_mark_refs_copied(vm, src_ptr, dst_ptr, span);
	qcvm_field_wrap_list_check_set(vm, dst_ptr, span);
}

const char *qcvm_stack_entry(const qcvm_t *vm, const qcvm_stack_t *s, const bool compact)
{
	if (!vm->linenumbers)
		return "dunno:dunno";

	if (!s->function)
		return "C code";

	const char *func = qcvm_get_string(vm, s->function->name_index);

	if (!*func)
		func = "dunno";

	const char *file = qcvm_get_string(vm, s->function->file_index);

	if (!*file)
		file = "dunno.qc";

	if (compact)
		return qcvm_temp_format(vm, "%s:%i", func, qcvm_line_number_for(vm, s->statement));

	return qcvm_temp_format(vm, "%s (%s:%i @ stmt %u)", func, file, qcvm_line_number_for(vm, s->statement), s->statement - vm->statements);
}

const char *qcvm_stack_trace(const qcvm_t *vm, const bool compact)
{
	const char *str = compact ? "" : "> ";

	for (qcvm_stack_t *s = vm->state.stack; s <= &vm->state.stack[vm->state.current] && s->function; s++)
	{
		if (compact)
			str = qcvm_temp_format(vm, "%s->%s", str, qcvm_stack_entry(vm, s, compact));
		else
			str = qcvm_temp_format(vm, "%s%s\n", str, qcvm_stack_entry(vm, s, compact));
	}

	return str;
}

qcvm_definition_t *qcvm_find_definition(qcvm_t *vm, const char *name, const qcvm_deftype_t type)
{
	qcvm_definition_hash_t *hashed = vm->definition_hashes[Q_hash_string(name, vm->definitions_size)];

	for (; hashed; hashed = hashed->hash_next)
		if ((hashed->def->id & ~TYPE_GLOBAL) == type && !strcmp(qcvm_get_string(vm, hashed->def->name_index), name))
			break;

	if (hashed)
		return hashed->def;

	return NULL;
}

qcvm_definition_t *qcvm_find_field(qcvm_t *vm, const char *name)
{
	qcvm_definition_hash_t *hashed = vm->field_hashes[Q_hash_string(name, vm->fields_size)];

	for (; hashed; hashed = hashed->hash_next)
		if (!strcmp(qcvm_get_string(vm, hashed->def->name_index), name))
			break;

	if (hashed)
		return hashed->def;

	return NULL;
}

#if ALLOW_DEBUGGING
static qcvm_evaluated_t qcvm_value_from_ptr(const qcvm_definition_t *def, const void *ptr)
{
	switch (def->id & ~TYPE_GLOBAL)
	{
	default:
		return (qcvm_evaluated_t) { def->global_index, .variant = { .type = TYPE_VOID } };
	case TYPE_STRING:
		return (qcvm_evaluated_t) { def->global_index, .variant = { .type = TYPE_STRING, .value.str = *(const qcvm_string_t *)(ptr) } };
	case TYPE_FLOAT:
		return (qcvm_evaluated_t) { def->global_index, .variant = { .type = TYPE_FLOAT, .value.flt = *(const vec_t *)(ptr) } };
	case TYPE_VECTOR:
		return (qcvm_evaluated_t) { def->global_index, .variant = { .type = TYPE_VECTOR, .value.vec = *(const vec3_t *)(ptr) } };
	case TYPE_ENTITY:
		return (qcvm_evaluated_t) { def->global_index, .variant = { .type = TYPE_ENTITY, .value.ent = *(const qcvm_ent_t *)(ptr) } };
	case TYPE_FIELD:
		return (qcvm_evaluated_t) { def->global_index, .variant = { .type = TYPE_FIELD, .value.fld = *(const int32_t *)(ptr) } };
	case TYPE_FUNCTION:
		return (qcvm_evaluated_t) { def->global_index, .variant = { .type = TYPE_FUNCTION, .value.fnc = *(const qcvm_func_t *)(ptr) } };
	case TYPE_POINTER:
		return (qcvm_evaluated_t) { def->global_index, .variant = { .type = TYPE_POINTER, .value.ptr = *(const qcvm_pointer_t *)(ptr) } };
	case TYPE_INTEGER:
		return (qcvm_evaluated_t) { def->global_index, .variant = { .type = TYPE_INTEGER, .value.itg = *(const int32_t *)(ptr) } };
	}
}

static qcvm_evaluated_t qcvm_value_from_global(qcvm_t *vm, const qcvm_definition_t *def)
{
	return qcvm_value_from_ptr(def, qcvm_get_global(vm, def->global_index));
}

static qcvm_evaluated_t qcvm_evaluate_from_local_or_global(qcvm_t *vm, const char *variable)
{
	// we don't have a . so we're just checking for a base object.
	// check locals
	if (vm->state.current >= 0 && vm->state.stack[vm->state.current].function)
	{
		qcvm_stack_t *current = &vm->state.stack[vm->state.current];
		size_t i;
		qcvm_global_t g;

		for (i = 0, g = current->function->first_arg; i < current->function->num_args_and_locals; i++, g++)
		{
			qcvm_definition_t *def = vm->definition_map_by_id[g];

			if (!def || def->name_index == STRING_EMPTY || strcmp(variable, qcvm_get_string(vm, def->name_index)))
				continue;

			return qcvm_value_from_global(vm, def);
		}
	}

	// no locals, so we can check all the globals
	for (qcvm_definition_t *def = vm->definitions; def < vm->definitions + vm->definitions_size; def++)
	{
		if (def->name_index == STRING_EMPTY || strcmp(variable, qcvm_get_string(vm, def->name_index)))
			continue;

		return qcvm_value_from_global(vm, def);
	}

	return (qcvm_evaluated_t) { 0, .variant = { .type = TYPE_VOID } };
}

qcvm_evaluated_t qcvm_evaluate(qcvm_t *vm, const char *variable)
{
	const char *dot = strchr(variable, '.');
	if (dot)
	{
		char evaluate_left[MAX_INFO_STRING];

		strncpy(evaluate_left, variable, dot - variable);
		evaluate_left[dot - variable] = 0;

		// we have a . so we're either a entity, pointer or struct...
		qcvm_evaluated_t left_hand = qcvm_evaluate_from_local_or_global(vm, evaluate_left);

		if (left_hand.variant.type == TYPE_ENTITY)
		{
			const char *right_context = dot + 1;
			const qcvm_ent_t ent = left_hand.variant.value.ent;

			if (ent == ENT_INVALID)
				return left_hand;

			const qcvm_definition_t *field = NULL;
				
			for (qcvm_definition_t *f = vm->fields; f < vm->fields + vm->fields_size; f++)
			{
				if (f->name_index == STRING_EMPTY || strcmp(qcvm_get_string(vm, f->name_index), right_context))
					continue;

				field = f;
				break;
			}

			if (!field)
				return (qcvm_evaluated_t) { 0, .variant = { .type = TYPE_VOID } };

			void *ptr;

			if (!qcvm_resolve_pointer(vm, qcvm_get_entity_field_pointer(vm, qcvm_ent_to_entity(vm, ent, false), (int32_t)field->global_index), true, qcvm_type_size(field->id), &ptr))
				qcvm_error(vm, "bad pointer");

			return qcvm_value_from_ptr(field, ptr);
		}
	}

	return qcvm_evaluate_from_local_or_global(vm, variable);
}

void qcvm_set_breakpoint(qcvm_t *vm, const bool is_set, const char *file, const int line)
{
	qcvm_string_t id;

	if (!qcvm_find_string(vm, file, &id))
	{
		qcvm_debug(vm, "Can't toggle breakpoint: can't find file %s in table\n", file);
		return;
	}

	for (qcvm_function_t *function = vm->functions; function < vm->functions + vm->functions_size; function++)
	{
		if (function->id <= 0 || function->file_index != id)
			continue;

		for (qcvm_statement_t *statement = &vm->statements[function->id]; statement->opcode != OP_DONE; statement++)
		{
			if (qcvm_line_number_for(vm, statement) != line)
				continue;

			// got it
			if (is_set)
				statement->opcode |= OP_BREAKPOINT;
			else
				statement->opcode &= ~OP_BREAKPOINT;
				
			qcvm_debug(vm, "Breakpoint set @ %s:%i\n", file, line);
			return;
		}
	}

	qcvm_debug(vm, "Can't toggle breakpoint: can't find %s:%i\n", file, line);
}

void qcvm_break_on_current_statement(qcvm_t *vm)
{
	if (!vm->debug.attached)
		return;

	qcvm_stack_t *current = &vm->state.stack[vm->state.current];
	qcvm_send_debugger_command(vm, qcvm_temp_format(vm, "qcstep \"%s\":%i\n", qcvm_get_string(vm, current->function->file_index), qcvm_line_number_for(vm, current->statement)));
	vm->debug.state = DEBUG_BROKE;
	vm->debug.step_function = current->function;
	vm->debug.step_statement = current->statement;
	vm->debug.step_depth = vm->state.current;
	qcvm_wait_for_debugger_commands(vm);
}
#endif

int qcvm_line_number_for(const qcvm_t *vm, const qcvm_statement_t *statement)
{
	if (vm->linenumbers)
		return vm->linenumbers[statement - vm->statements];

	return 0;
}

const char *qcvm_function_for(const qcvm_t *vm, const qcvm_statement_t *statement)
{
	while (statement >= vm->statements)
	{
		for (size_t i = 0; i < vm->functions_size; i++)
		{
			const qcvm_function_t *func = vm->functions + i;

			if (func->id < 0)
				continue;

			const qcvm_statement_t *start = vm->statements + (func->id - 1);

			if (statement == start)
				return qcvm_get_string(vm, func->name_index);
		}

		--statement;
	}

	return "???";
}

qcvm_func_t qcvm_find_function_id(const qcvm_t *vm, const char *name)
{
	size_t i = 0;

	for (qcvm_function_t *func = vm->functions; func < vm->functions + vm->functions_size; func++, i++)
		if (!strcmp(qcvm_get_string(vm, func->name_index), name))
			return (qcvm_func_t)i;

	return FUNC_VOID;
}

qcvm_function_t *qcvm_get_function(const qcvm_t *vm, const qcvm_func_t id)
{
	return &vm->functions[id];
}

qcvm_function_t *qcvm_find_function(const qcvm_t *vm, const char *name)
{
	qcvm_func_t id = qcvm_find_function_id(vm, name);

	if (!id)
		return NULL;

	return qcvm_get_function(vm, id);
}

int32_t qcvm_handle_alloc(qcvm_t *vm, void *ptr, const qcvm_handle_descriptor_t *descriptor)
{
	qcvm_handle_t *handle;

	if (vm->handles.free_size)
		handle = &vm->handles.data[vm->handles.free[--vm->handles.free_size]];
	else
	{
		if (vm->handles.size == vm->handles.allocated)
		{
			vm->handles.allocated += HANDLES_RESERVE;
			qcvm_handle_t *old_handles = vm->handles.data;
			vm->handles.data = (qcvm_handle_t *)qcvm_alloc(vm, sizeof(qcvm_handle_t) * vm->handles.allocated);
			if (old_handles)
			{
				memcpy(vm->handles.data, old_handles, sizeof(qcvm_handle_t) * vm->handles.size);
				qcvm_mem_free(vm, old_handles);
			}

			if (vm->handles.free)
				qcvm_mem_free(vm, vm->handles.free);

			vm->handles.free = (int32_t *)qcvm_alloc(vm, sizeof(int32_t) * vm->handles.allocated);

			qcvm_debug(vm, "Increased handle storage to %u\n", vm->handles.allocated);
		}

		handle = &vm->handles.data[(int32_t)vm->handles.size++];
	}

	handle->id = (int32_t)(handle - vm->handles.data) + 1;
	handle->descriptor = descriptor;
	handle->handle = ptr;

	return handle->id;
}

qcvm_handle_t *qcvm_fetch_handle(const qcvm_t *vm, const int32_t id)
{
	if (id <= 0 || id > vm->handles.size)
		qcvm_error(vm, "invalid handle ID");

	qcvm_handle_t *handle = &vm->handles.data[id - 1];

	if (!handle->handle)
		qcvm_error(vm, "invalid handle ID");

	return handle;
}

void qcvm_handle_free(qcvm_t *vm, qcvm_handle_t *handle)
{
	if (!handle || !handle->handle || handle < vm->handles.data || handle >= (vm->handles.data + vm->handles.size))
		qcvm_error(vm, "invalid handle");

	if (handle->descriptor && handle->descriptor->free)
		handle->descriptor->free(vm, handle->handle);

	vm->handles.free[vm->handles.free_size++] = handle->id - 1;
	handle->descriptor = NULL;
	handle->handle = NULL;
}

qcvm_pointer_t qcvm_get_entity_field_pointer(qcvm_t *vm, edict_t *ent, const int32_t field)
{
	const qcvm_pointer_t ptr = qcvm_make_pointer(vm, QCVM_POINTER_ENTITY, (int32_t *)ent + field);

#ifdef _DEBUG
	if (!qcvm_resolve_pointer(vm, ptr, false, sizeof(qcvm_global_t), NULL))
		qcvm_error(vm, "Returning invalid entity field pointer");
#endif

	return ptr;
}

void qcvm_execute(qcvm_t *vm, qcvm_function_t *function)
{
	if (!function || function->id == 0)
		qcvm_error(vm, "bad function");

	if (function->id < 0)
	{
		qcvm_call_builtin(vm, function);
		return;
	}

	int32_t enter_depth = 1;
	const qcvm_statement_t *statement;

	qcvm_enter(vm, function);

	while (1)
	{
		// get next statement
		qcvm_stack_t *current = &vm->state.stack[vm->state.current];
		statement = ++current->statement;

#if ALLOW_INSTRUMENTING
		if (vm->profiling.flags & PROFILE_FIELDS)
			current->profile->fields[NumInstructions][vm->profiling.mark]++;
#endif

#if ALLOW_DEBUGGING
		if (vm->debug.attached)
		{
			if (statement->opcode & OP_BREAKPOINT)
				qcvm_break_on_current_statement(vm);
			else
			{
				// figure out if we need to break here.
				// step into is easiest: next QC execution that is not on the same function+line combo
				if (vm->debug.state == DEBUG_STEP_INTO)
				{
					if (vm->debug.step_function != current->function || qcvm_line_number_for(vm, vm->debug.step_statement) != qcvm_line_number_for(vm, current->statement))
						qcvm_break_on_current_statement(vm);
				}
				// I lied, step out is the easiest
				else if (vm->debug.state == DEBUG_STEP_OUT)
				{
					if (vm->debug.step_depth > vm->state.current)
						qcvm_break_on_current_statement(vm);
				}
				// step over: either step out, or the next step that is in the same function + stack depth + not on same line
				else if (vm->debug.state == DEBUG_STEP_OVER)
				{
					if (vm->debug.step_depth > vm->state.current ||
						(vm->debug.step_depth == vm->state.current && vm->debug.step_function == current->function && qcvm_line_number_for(vm, vm->debug.step_statement) != qcvm_line_number_for(vm, current->statement)))
						qcvm_break_on_current_statement(vm);
				}
			}
		}

		const qcvm_opcode_t code = statement->opcode & ~OP_BREAKPOINT;
#else
		const qcvm_opcode_t code = statement->opcode;
#endif
		JUMPCODE_LIST;

		START_OPCODE_TIMER(vm, code);

		EXECUTE_JUMPCODE;

		END_TIMER(vm, PROFILE_OPCODES);

#if ALLOW_PROFILING
		if (vm->profiling.flags & PROFILE_SAMPLES)
		{
			if (!--vm->profiling.sampling.id)
			{
				vm->profiling.sampling.data[statement - vm->statements].count[vm->profiling.mark]++;
				vm->profiling.sampling.id = vm->profiling.sampling.rate;
			}
		}
#endif

		if (!enter_depth)
			return;		// all done
	}

	/*vm->debug_print(qcvm_temp_format(vm, "Infinite loop broken @ %s\n", qcvm_stack_trace(vm, true)));
	
	while (vm->state.current != -1)
		qcvm_leave(vm);*/

JUMPCODE_ASM
}

static const uint32_t QCVM_VERSION	= 1;

void qcvm_write_state(qcvm_t *vm, FILE *fp)
{
	fwrite(&QCVM_VERSION, sizeof(QCVM_VERSION), 1, fp);

	// write dynamic strings
	qcvm_string_list_write_state(vm, fp);
}

void qcvm_read_state(qcvm_t *vm, FILE *fp)
{
	uint32_t ver;

	fread(&ver, sizeof(ver), 1, fp);

	if (ver != QCVM_VERSION)
		qcvm_error(vm, "bad VM version");

	// read dynamic strings
	qcvm_string_list_read_state(vm, fp);
}

static void VMLoadStatements(qcvm_t *vm, FILE *fp, qcvm_statement_t *dst, const qcvm_header_t *header)
{
	// simple, rustic
	if (header->version == PROGS_FTE && header->secondary_version == PROG_SECONDARYVERSION32)
	{
		fread(dst, sizeof(qcvm_statement_t), header->sections.statement.size, fp);
		return;
	}

	typedef struct
	{
		uint16_t	opcode;
		uint16_t	args[3];
	} QCStatement16;

	QCStatement16 *statements = (QCStatement16 *)qcvm_alloc(vm, sizeof(QCStatement16) * header->sections.statement.size);
	fread(statements, sizeof(QCStatement16), header->sections.statement.size, fp);

	for (size_t i = 0; i < header->sections.statement.size; i++, dst++)
	{
		QCStatement16 *src = statements + i;

		dst->opcode = (qcvm_opcode_t)src->opcode;
		dst->args = (qcvm_operands_t) {
			(qcvm_global_t)src->args[0],
			(qcvm_global_t)src->args[1],
			(qcvm_global_t)src->args[2]
		};
	}

	qcvm_mem_free(vm, statements);
}

static void VMLoadDefinitions(qcvm_t *vm, FILE *fp, qcvm_definition_t *dst, const qcvm_header_t *header, const size_t size)
{
	// simple, rustic
	if (header->version == PROGS_FTE && header->secondary_version == PROG_SECONDARYVERSION32)
	{
		fread(dst, sizeof(qcvm_definition_t), size, fp);
		return;
	}

	typedef struct
	{
		uint16_t	id;
		uint16_t	global_index;
		qcvm_string_t	name_index;
	} QCDefinition16;

	QCDefinition16 *defs = (QCDefinition16 *)qcvm_alloc(vm, sizeof(QCDefinition16) * size);
	fread(defs, sizeof(QCDefinition16), size, fp);

	for (size_t i = 0; i < size; i++, dst++)
	{
		QCDefinition16 *src = defs + i;

		dst->id = (qcvm_deftype_t)src->id;
		dst->global_index = (qcvm_global_t)src->global_index;
		dst->name_index = src->name_index;
	}

	qcvm_mem_free(vm, defs);
}

void qcvm_load(qcvm_t *vm, const char *engine_name, const char *filename)
{
	qcvm_init(vm);

	vm->engine_name = engine_name;

	const char *last_slash = strrchr(filename, '\\');

	if (!last_slash)
		last_slash = strrchr(filename, '/');

	if (!last_slash)
		vm->path[0] = 0;
	else
		strncpy(vm->path, filename, last_slash - filename + 1);

	FILE *fp = fopen(filename, "rb");

	if (!fp)
		qcvm_error(vm, "no progs.dat");

	qcvm_header_t header;

	fread(&header, sizeof(header), 1, fp);

	if (header.version != PROGS_Q1 && header.version != PROGS_FTE)
		qcvm_error(vm, "bad version (only version 6 & 7 progs are supported)");

	vm->global_size = header.sections.globals.size;

	vm->string_size = header.sections.string.size;
	vm->string_data = (char *)qcvm_alloc(vm, sizeof(char) * vm->string_size);
	vm->string_lengths = (size_t *)qcvm_alloc(vm, sizeof(size_t) * vm->string_size);

	fseek(fp, header.sections.string.offset, SEEK_SET);
	fread(vm->string_data, sizeof(char), header.sections.string.size, fp);
	
	vm->string_hashes = (qcvm_string_hash_t **)qcvm_alloc(vm, sizeof(qcvm_string_hash_t *) * vm->string_size);
	vm->string_hashes_data = (qcvm_string_hash_t *)qcvm_alloc(vm, sizeof(qcvm_string_hash_t) * vm->string_size);

#ifdef _DEBUG
	size_t added_hash_values = 0, unique_hash_values = 0, max_hash_depth = 0;
#endif
	
	// create immutable string map, for fast hash action
	for (size_t i = 0; i < vm->string_size; i++)
	{
		const char *s = vm->string_data + i;

		if (!*s)
			continue;

		const size_t s_len = strlen(s);

		for (size_t x = 0; x < s_len; x++)
		{
			size_t len = s_len - x;
			vm->string_lengths[i + x] = len;
			qcvm_string_hash_t *hashed = &vm->string_hashes_data[i + x];
			hashed->str = s + x;
			hashed->hash_value = Q_hash_string(s + x, vm->string_size);

			// if we already have this string hashed, don't hash us again
			for (qcvm_string_hash_t *existing_hashed = vm->string_hashes[hashed->hash_value]; ; existing_hashed = existing_hashed->hash_next)
			{
				if (!existing_hashed)
				{
#ifdef _DEBUG
					if (!vm->string_hashes[hashed->hash_value])
						unique_hash_values++;
#endif

					hashed->hash_next = vm->string_hashes[hashed->hash_value];
					vm->string_hashes[hashed->hash_value] = hashed;
#ifdef _DEBUG
					added_hash_values++;
#endif
					break;
				}
				else if (!strcmp(existing_hashed->str, hashed->str))
					break;
			}

#ifdef _DEBUG
			size_t depth = 1;

			for (qcvm_string_hash_t *existing_hashed = vm->string_hashes[hashed->hash_value]; existing_hashed; existing_hashed = existing_hashed->hash_next, depth++) ;

			max_hash_depth = maxsz(max_hash_depth, depth);
#endif
		}

		i += s_len;
	}

	qcvm_debug(vm, "String hash table: %u added, %u unique, %u max depth, %u strings total\n", added_hash_values, unique_hash_values, max_hash_depth, vm->string_size);

	vm->statements_size = header.sections.statement.size;
	vm->statements = (qcvm_statement_t *)qcvm_alloc(vm, sizeof(qcvm_statement_t) * vm->statements_size);

	fseek(fp, header.sections.statement.offset, SEEK_SET);
	VMLoadStatements(vm, fp, vm->statements, &header);

	for (qcvm_statement_t *s = vm->statements; s < vm->statements + vm->statements_size; s++)
		if (s->opcode >= OP_NUMOPS || !qcvm_code_funcs[s->opcode])
			qcvm_error(vm, "opcode invalid or not implemented: %i\n", s->opcode);
	
	vm->definitions_size = header.sections.definition.size;
	vm->definitions = (qcvm_definition_t *)qcvm_alloc(vm, sizeof(qcvm_definition_t) * vm->definitions_size);

	fseek(fp, header.sections.definition.offset, SEEK_SET);
	VMLoadDefinitions(vm, fp, vm->definitions, &header, vm->definitions_size);

	vm->definition_map_by_id = (qcvm_definition_t **)qcvm_alloc(vm, sizeof(qcvm_definition_t) * vm->global_size);
	vm->definition_hashes = (qcvm_definition_hash_t **)qcvm_alloc(vm, sizeof(qcvm_definition_hash_t *) * vm->definitions_size);
	vm->definition_hashes_data = (qcvm_definition_hash_t *)qcvm_alloc(vm, sizeof(qcvm_definition_hash_t) * vm->definitions_size);
	
	for (qcvm_definition_t *definition = vm->definitions; definition < vm->definitions + vm->definitions_size; definition++)
	{
		if (definition->name_index != STRING_EMPTY)
		{
			qcvm_definition_hash_t *hashed = &vm->definition_hashes_data[definition - vm->definitions];
			hashed->def = definition;
			hashed->hash_value = Q_hash_string(qcvm_get_string(vm, definition->name_index), vm->definitions_size);
			hashed->hash_next = vm->definition_hashes[hashed->hash_value];
			vm->definition_hashes[hashed->hash_value] = hashed;
		}

		vm->definition_map_by_id[definition->global_index] = definition;
	}

	vm->fields_size = header.sections.field.size;
	vm->fields = (qcvm_definition_t *)qcvm_alloc(vm, sizeof(qcvm_definition_t) * vm->fields_size);
	vm->system_fields = (qcvm_system_field_t *)qcvm_alloc(vm, sizeof(qcvm_system_field_t) * vm->fields_size);

	fseek(fp, header.sections.field.offset, SEEK_SET);
	VMLoadDefinitions(vm, fp, vm->fields, &header, vm->fields_size);

	vm->field_hashes = (qcvm_definition_hash_t **)qcvm_alloc(vm, sizeof(qcvm_definition_hash_t *) * vm->fields_size);
	vm->field_hashes_data = (qcvm_definition_hash_t *)qcvm_alloc(vm, sizeof(qcvm_definition_hash_t) * vm->fields_size);

	for (qcvm_definition_t *field = vm->fields; field < vm->fields + vm->fields_size; field++)
	{
		qcvm_definition_hash_t *hashed = &vm->field_hashes_data[field - vm->fields];
		hashed->def = field;
		hashed->hash_value = Q_hash_string(qcvm_get_string(vm, field->name_index), vm->fields_size);
		hashed->hash_next = vm->field_hashes[hashed->hash_value];
		vm->field_hashes[hashed->hash_value] = hashed;
	}

	vm->functions_size = header.sections.function.size;
	vm->functions = (qcvm_function_t *)qcvm_alloc(vm, sizeof(qcvm_function_t) * vm->functions_size);

	fseek(fp, header.sections.function.offset, SEEK_SET);
	fread(vm->functions, sizeof(qcvm_function_t), vm->functions_size, fp);
	
	vm->global_data = (qcvm_global_t *)qcvm_alloc(vm, sizeof(qcvm_global_t) * vm->global_size);

	fseek(fp, header.sections.globals.offset, SEEK_SET);
	fread(vm->global_data, sizeof(qcvm_global_t), vm->global_size, fp);

	for (qcvm_function_t *func = vm->functions; func < vm->functions + vm->functions_size; func++)
	{
		if (func->id < 0)
		{
			vm->warning("QCVM WARNING: Code contains old-school negative-indexed builtin \"%s\". Use #0 for all builtins!\n", qcvm_get_string(vm, func->name_index));
			func->id = 0;
		}
		
		if (func->id == 0 && func->name_index)
			vm->builtins.count++;

		vm->highest_stack = maxsz(vm->highest_stack, func->num_args_and_locals + LOCALS_FIX);

		if (func->num_args_and_locals > 128)
			vm->warning("QCVM WARNING: func \"%s\" has a pretty big stack (%i locals)\n", qcvm_get_string(vm, func->name_index), func->num_args_and_locals);
	}

	vm->builtins.list = (qcvm_builtin_t *)qcvm_alloc(vm, sizeof(qcvm_builtin_t *) * vm->builtins.count);

	qcvm_debug(vm, "QCVM Stack Locals Size: %i bytes\n", vm->highest_stack * 4);

	fclose(fp);

	// Check for debugging info
	fp = fopen(qcvm_temp_format(vm, "%sprogs.lno", vm->path), "rb");

	if (fp)
	{
		const int lnotype = 1179602508;
		const int version = 1;

		struct {
			int magic, ver, numglobaldefs, numglobals, numfielddefs, numstatements;
		} lno_header;
		
		fread(&lno_header, sizeof(lno_header), 1, fp);

		if (lno_header.magic == lnotype && lno_header.ver == version && (size_t)lno_header.numglobaldefs == header.sections.definition.size &&
			(size_t)lno_header.numglobals == header.sections.globals.size && (size_t)lno_header.numfielddefs == header.sections.field.size &&
			(size_t)lno_header.numstatements == header.sections.statement.size)
		{
			vm->linenumbers = (int *)qcvm_alloc(vm, sizeof(*vm->linenumbers) * header.sections.statement.size);
			fread(vm->linenumbers, sizeof(int), header.sections.statement.size, fp);
		}

		fclose(fp);
	}

#if ALLOW_INSTRUMENTING
	vm->profiling.instrumentation.data = (qcvm_profile_t *)qcvm_alloc(vm, sizeof(qcvm_profile_t) * vm->functions_size);
#endif

#if ALLOW_PROFILING
	vm->profiling.sampling.data = (qcvm_sampling_t *)qcvm_alloc(vm, sizeof(qcvm_sampling_t) * vm->statements_size);
	vm->profiling.sampling.function_data = (qcvm_sampling_t *)qcvm_alloc(vm, sizeof(qcvm_sampling_t) * vm->functions_size);
#endif

#if ALLOW_INSTRUMENTING || ALLOW_PROFILING
	if (vm->profiling.flags & PROFILE_CONTINUOUS)
	{
		fp = fopen(qcvm_temp_format(vm, "%s%s.perf", vm->path, vm->profiling.filename), "rb");

		if (fp)
		{
#if ALLOW_INSTRUMENTING
			fread(vm->profiling.instrumentation.data, sizeof(qcvm_profile_t), vm->functions_size, fp);
			fread(vm->profiling.instrumentation.opcode_timers, sizeof(qcvm_profile_timer_t), OP_NUMOPS, fp);
			fread(vm->profiling.instrumentation.timers, sizeof(qcvm_profile_timer_t), OP_NUMOPS, fp);
#endif
#if ALLOW_PROFILING
			fread(vm->profiling.sampling.data, sizeof(qcvm_sampling_t), vm->statements_size, fp);
			fread(vm->profiling.sampling.function_data, sizeof(qcvm_sampling_t), vm->functions_size, fp);
#endif

			fclose(fp);

			vm->debug_print(qcvm_temp_format(vm, "QCVM: continuing profile of %s\n", vm->profiling.filename));
		}
	}
#endif

	const qcvm_definition_t *def = qcvm_find_definition(vm, "strcasesensitive", TYPE_INTEGER);

	if (!def)
		qcvm_error(vm, "can't find required definition \"strcasesensitive\"");
	
	vm->string_case_sensitive = vm->global_data + def->global_index;
}

static inline void qcvm_setup_fields(qcvm_t *vm)
{
	qcvm_global_t field_offset = (qcvm_global_t)vm->system_edict_size;

	for (qcvm_definition_t *field = vm->fields + 1; field < vm->fields + vm->fields_size; field++)
	{
		if (!field->name_index)
			continue;

		// if we're a vector _x/_y/_z field, we were already set up
		const char *name = qcvm_get_string(vm, field->name_index);
		const size_t name_len = qcvm_get_string_length(vm, field->name_index);

		if (name[name_len - 2] == '_' && (name[name_len - 1] == 'x' || name[name_len - 1] == 'y' || name[name_len - 1] == 'z'))
		{
			char *parent_name = qcvm_temp_format(vm, "%s", name);
			parent_name[name_len - 2] = 0;

			if (qcvm_find_field(vm, parent_name))
				continue;
		}

		// check if it's a system field
		qcvm_system_field_t *sysfield = vm->system_fields;

		for (; sysfield < vm->system_fields + vm->system_fields_size; sysfield++)
			if (sysfield->field == field)
				break;

		qcvm_global_t real_offset;
		qcvm_definition_t *fielddef;
		
		// system fields have an offset determined by the field.
		if (sysfield < vm->system_fields + vm->system_fields_size)
		{
			real_offset = sysfield->offset;
			fielddef = sysfield->def;
		}
		else
		{
			// not a sysfield, so we have to be positioned after them
			real_offset = field_offset;
			field_offset += qcvm_type_span(field->id);
			fielddef = qcvm_find_definition(vm, name, TYPE_FIELD);

			if (!fielddef)
				qcvm_error(vm, "field %s has no def", name);
		}

		// set field data
		field->global_index = real_offset;

		// set def data
		qcvm_set_global_typed_value(qcvm_global_t, vm, fielddef->global_index, real_offset);

		// vectors have 3 fields
		if (field->id == TYPE_VECTOR)
		{
			for (qcvm_global_t i = 1, offset = real_offset + i; i < 3; i++, offset++)
				qcvm_set_global_typed_value(qcvm_global_t, vm, fielddef->global_index + i, offset);

			// find _x/_y/_z fields and re-assign their values as well
			static const char *vector_field_suffixes[] = { "_x", "_y", "_z" };

			for (qcvm_global_t i = 0; i < 3; i++)
			{
				const char *field_name = qcvm_temp_format(vm, "%s%s", name, vector_field_suffixes[i]);
				qcvm_definition_t *def = qcvm_find_field(vm, field_name);

				if (def)
					def->global_index = real_offset + i;
			}
		}
	}
}

static inline void qcvm_init_field_map(qcvm_t *vm)
{
	for (qcvm_definition_t *field = vm->fields + 1; field < vm->fields + vm->fields_size; field++)
		vm->field_real_size = maxsz(vm->field_real_size, field->global_index + qcvm_type_span(field->id));

	vm->field_map_by_id = (qcvm_definition_t **)qcvm_alloc(vm, sizeof(qcvm_definition_t *) * vm->field_real_size);

	for (qcvm_definition_t *f = vm->fields + 1; f < vm->fields + vm->fields_size; f++)
	{
		const char *field_name = qcvm_get_string(vm, f->name_index);
		qcvm_definition_t *def = qcvm_find_definition(vm, field_name, TYPE_FIELD);

		if (!def)
			qcvm_error(vm, "Bad field %s, missing def", field_name);

		assert(f->global_index == vm->global_data[def->global_index]);

		vm->field_map_by_id[f->global_index] = f;
	}
}

static inline void qcvm_check_builtins(qcvm_t *vm)
{
	for (qcvm_function_t *func = vm->functions; func < vm->functions + vm->functions_size; func++)
		if (func->id == 0 && func->name_index != STRING_EMPTY)
			vm->warning("Missing builtin function: %s\n", qcvm_get_string(vm, func->name_index));

	// Set up intrinsics
	for (qcvm_statement_t *s = vm->statements; s < vm->statements + vm->statements_size; s++)
	{
		if (s->opcode == OP_CALL1H)
		{
			qcvm_func_t f = *(vm->global_data + s->args.a);

			if (f == qcvm_find_function_id(vm, "sqrt"))
				s->opcode = OP_INTRIN_SQRT;
			if (f == qcvm_find_function_id(vm, "sin"))
				s->opcode = OP_INTRIN_SIN;
			if (f == qcvm_find_function_id(vm, "cos"))
				s->opcode = OP_INTRIN_COS;
		}
	}
}

void qcvm_check(qcvm_t *vm)
{
	qcvm_setup_fields(vm);
	
	qcvm_init_field_map(vm);
	
	qcvm_field_wrap_list_init(vm);

	qcvm_check_builtins(vm);
}

#if ALLOW_INSTRUMENTING
static const char *profile_type_names[TotalProfileFields] =
{
	"# Calls",
	"# Instructions",
	"# Unconditional Jumps",
	"# Conditional Jumps",
	"# Globals Fetched",
	"# Globals Set",
	"# Func Calls"
};

static const char *timer_type_names[TotalTimerFields] =
{
	"String Acquire",
	"String Release",
	"String Mark",
	"String Check Unset",
	"String HasRef",
	"String MarkIfHasRef",
	"String StringMarkRefsCopied",
	"String PopRef",
	"String PushRef",
	"String Find",

	"Wrap Apply"
};
#endif

#if ALLOW_INSTRUMENTING || ALLOW_PROFILING
static const char *mark_names[TOTAL_MARKS] =
{
	"init",
	"shutdown",
	"spawnentities",
	"writegame",
	"readgame",
	"writelevel",
	"readlevel",
	"clientconnect",
	"clientbegin",
	"clientuserinfochanged",
	"clientdisconnect",
	"clientcommand",
	"clientthink",
	"runframe",
	"servercommand",
	"qc_profile_func"
};
#endif

void qcvm_shutdown(qcvm_t *vm)
{
#if ALLOW_INSTRUMENTING || ALLOW_PROFILING
	if (vm->profiling.flags & PROFILE_CONTINUOUS)
	{
		FILE *fp = fopen(qcvm_temp_format(vm, "%s%s.perf", vm->path, vm->profiling.filename), "wb");

#if ALLOW_INSTRUMENTING
		fwrite(vm->profiling.instrumentation.data, sizeof(qcvm_profile_t), vm->functions_size, fp);
		fwrite(vm->profiling.instrumentation.opcode_timers, sizeof(vm->profiling.instrumentation.opcode_timers), 1, fp);
		fwrite(vm->profiling.instrumentation.timers, sizeof(vm->profiling.instrumentation.timers), 1, fp);
#endif
#if ALLOW_PROFILING
		fwrite(vm->profiling.sampling.data, sizeof(qcvm_sampling_t), vm->statements_size, fp);
		fwrite(vm->profiling.sampling.function_data, sizeof(qcvm_sampling_t), vm->functions_size, fp);
#endif

		fclose(fp);
	}
#endif

#if ALLOW_PROFILING
	if (vm->profiling.flags & PROFILE_SAMPLES)
	{
		for (size_t m = 0; m < TOTAL_MARKS; m++)
		{
			const char *mark = mark_names[m];
			FILE *fp = fopen(qcvm_temp_format(vm, "%sprf_%s_samples.csv", vm->path, mark), "wb");

			fprintf(fp, "ID,Path,Count\n");

			for (size_t i = 0; i < vm->statements_size; i++)
			{
				const qcvm_sampling_t *sample = vm->profiling.sampling.data + i;

				if (!sample->count[m])
					continue;

				fprintf(fp, "%" PRIuPTR ",%s:%i,%llu\n", i, qcvm_function_for(vm, vm->statements + i), qcvm_line_number_for(vm, vm->statements + i), sample->count[m]);
			}

			fclose(fp);

			fp = fopen(qcvm_temp_format(vm, "%sprf_%s_samples_func.csv", vm->path, mark), "wb");

			fprintf(fp, "ID,Name,Count\n");

			for (size_t i = 0; i < vm->functions_size; i++)
			{
				const qcvm_sampling_t *sample = vm->profiling.sampling.function_data + i;

				if (!sample->count[m])
					continue;

				fprintf(fp, "%" PRIuPTR ",%s,%llu\n", i, qcvm_get_string(vm, qcvm_get_function(vm, i)->name_index), sample->count[m]);
			}

			fclose(fp);
		}
	}
#endif
	
#if ALLOW_INSTRUMENTING
	for (size_t m = 0; m < TOTAL_MARKS; m++)
	{
		const char *mark = mark_names[m];

		if (vm->profiling.flags & (PROFILE_FUNCTIONS | PROFILE_FIELDS))
		{
			FILE *fp = fopen(qcvm_temp_format(vm, "%sprf_%s_profile.csv", vm->path, mark), "wb");
			double all_total = 0;
		
			for (size_t i = 0; i < vm->functions_size; i++)
			{
				const qcvm_profile_t *profile = vm->profiling.instrumentation.data + i;

				if (!profile->fields[NumSelfCalls][m] && !profile->self[m] && !profile->ext[m])
					continue;

				all_total += profile->self[m];
			}

			fprintf(fp, "ID,Name,Total (ms),Self(ms),Funcs(ms),Total (%%),Self (%%)");
	
			for (size_t i = 0; i < TotalProfileFields; i++)
				fprintf(fp, ",%s", profile_type_names[i]);
	
			fprintf(fp, "\n");

			for (size_t i = 0; i < vm->functions_size; i++)
			{
				const qcvm_profile_t *profile = vm->profiling.instrumentation.data + i;

				if (!profile->fields[NumSelfCalls][m] && !profile->self[m] && !profile->ext[m])
					continue;

				const qcvm_function_t *ff = vm->functions + i;
				const char *name = qcvm_get_string(vm, ff->name_index);
		
				const float self = profile->self[m];
				const float ext = profile->ext[m];
				const double total = self + ext;

				fprintf(fp, "%" PRIuPTR ",%s,%f,%f,%f,%f,%f", i, name, total, self, ext, (total / all_total) * 100, (self / all_total) * 100);
		
				for (qcvm_profiler_field_t f = 0; f < TotalProfileFields; f++)
					fprintf(fp, ",%" PRIuPTR "", profile->fields[f][m]);

				fprintf(fp, "\n");
			}

			fclose(fp);
		}
	
		if (vm->profiling.flags & PROFILE_TIMERS)
		{
			FILE *fp = fopen(qcvm_temp_format(vm, "%sprf_%s_timers.csv", vm->path, mark), "wb");
			double all_total = 0;

			for (size_t i = 0; i < TotalTimerFields; i++)
			{
				const qcvm_profile_timer_t *timer = &vm->profiling.instrumentation.timers[i][m];
				all_total += timer->time[m];
			}

			fprintf(fp, "Name,Count,Total (ms),Avg (ns),%%\n");

			for (size_t i = 0; i < TotalTimerFields; i++)
			{
				const qcvm_profile_timer_t *timer = &vm->profiling.instrumentation.timers[i][m];
				const float total = timer->time[m];

				fprintf(fp, "%s,%" PRIuPTR ",%f,%f,%f\n", timer_type_names[i], timer->count[m], total, (total / timer->count[m]) * 1000, (total / all_total) * 100);
			}

			fclose(fp);
		}
	
		if (vm->profiling.flags & PROFILE_OPCODES)
		{
			FILE *fp = fopen(qcvm_temp_format(vm, "%sprf_%s_opcodes.csv", vm->path, mark), "wb");
			double all_total = 0;

			for (size_t i = 0; i < OP_NUMOPS; i++)
			{
				const qcvm_profile_timer_t *timer = &vm->profiling.instrumentation.opcode_timers[i][m];

				if (!timer->count[m])
					continue;

				all_total += timer->time[m];
			}

			fprintf(fp, "ID,Count,Total (ms),Avg (ns),%%\n");

			for (size_t i = 0; i < OP_NUMOPS; i++)
			{
				const qcvm_profile_timer_t *timer = &vm->profiling.instrumentation.opcode_timers[i][m];

				if (!timer->count[m])
					continue;

				const float total = timer->time[m];

				fprintf(fp, "%" PRIuPTR ",%" PRIuPTR ",%f,%f,%f\n", i, timer->count[m], total, (total / timer->count[m]) * 1000, (total / all_total) * 100);
			}

			fclose(fp);
		}
	}
#endif

	qcvm_free(vm);
}

void qcvm_init_all_builtins(qcvm_t *vm)
{
	qcvm_init_game_builtins(vm);
	qcvm_init_ext_builtins(vm);
	qcvm_init_string_builtins(vm);
	qcvm_init_mem_builtins(vm);
	qcvm_init_debug_builtins(vm);
	qcvm_init_math_builtins(vm);
	qcvm_init_file_builtins(vm);
	qcvm_init_hash_builtins(vm);
	qcvm_init_structlist_builtins(vm);
	qcvm_init_list_builtins(vm);
	qcvm_init_heap_builtins(vm);
}
