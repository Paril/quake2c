#define QCVM_INTERNAL
#include "shared/shared.h"
#include "game.h"
#include "g_vm.h"
#include "vm_game.h"
#include "vm_opcodes.h"
#include "vm_math.h"
#include "vm_debug.h"
#include "vm_mem.h"
#include "vm_string.h"
#include "vm_ext.h"

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

void qcvm_stack_needs_resize(qcvm_stack_t *stack)
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

static void qcvm_string_list_init(qcvm_string_list_t *list, qcvm_t *vm)
{
	list->vm = vm;
}

qcvm_string_t qcvm_string_list_store(qcvm_string_list_t *list, const char *str, const size_t len)
{
	int32_t index;

	if (list->free_indices_size)
		index = (-list->free_indices[--list->free_indices_size]) - 1;
	else
	{
		if (list->strings_size == list->strings_allocated)
		{
			list->strings_allocated += REF_STRING_RESERVE;
			qcvm_ref_counted_string_t *old_strings = list->strings;
			list->strings = (qcvm_ref_counted_string_t *)qcvm_alloc(list->vm, sizeof(qcvm_ref_counted_string_t) * list->strings_allocated);
			if (old_strings)
			{
				memcpy(list->strings, old_strings, sizeof(qcvm_ref_counted_string_t) * list->strings_size);
				qcvm_mem_free(list->vm, old_strings);
			}
			qcvm_debug(list->vm, "Increased ref string storage to %u\n", list->strings_allocated);
		}

		index = list->strings_size++;
	}

	list->strings[index] = (qcvm_ref_counted_string_t) {
		str,
		len,
		0
	};

	return (qcvm_string_t)(-(index + 1));
}

void qcvm_string_list_unstore(qcvm_string_list_t *list, const qcvm_string_t id)
{
	const int32_t index = (int32_t)(-id) - 1;

	assert(index >= 0 && index < list->strings_size);

	qcvm_ref_counted_string_t *str = &list->strings[index];

	assert(!str->ref_count);

	qcvm_mem_free(list->vm, (void *)str->str);

	*str = (qcvm_ref_counted_string_t) { NULL, 0, 0 };

	if (list->free_indices_size == list->free_indices_allocated)
	{
		list->free_indices_allocated += FREE_STRING_RESERVE;
		qcvm_string_t *old_free_indices = list->free_indices;
		list->free_indices = (qcvm_string_t *)qcvm_alloc(list->vm, sizeof(qcvm_string_t) * list->free_indices_allocated);
		if (old_free_indices)
		{
			memcpy(list->free_indices, old_free_indices, sizeof(qcvm_string_t) * list->free_indices_size);
			qcvm_mem_free(list->vm, old_free_indices);
		}
		qcvm_debug(list->vm, "Increased free string storage to %u\n", list->free_indices_allocated);
	}

	list->free_indices[list->free_indices_size++] = id;
}

size_t qcvm_string_list_get_length(const qcvm_string_list_t *list, const qcvm_string_t id)
{
	const int32_t index = (int32_t)(-id) - 1;
	assert(index >= 0 && index < list->strings_size);
	return list->strings[index].length;
}

const char *qcvm_string_list_get(const qcvm_string_list_t *list, const qcvm_string_t id)
{
	const int32_t index = (int32_t)(-id) - 1;
	assert(index >= 0 && index < list->strings_size);
	return list->strings[index].str;
}

void qcvm_string_list_acquire(qcvm_string_list_t *list, const qcvm_string_t id)
{
	START_TIMER(list->vm, StringAcquire);
	
	const int32_t index = (int32_t)(-id) - 1;

	assert(index >= 0 && index < list->strings_size);

	list->strings[index].ref_count++;

	END_TIMER(list->vm, PROFILE_TIMERS);
}

void qcvm_string_list_release(qcvm_string_list_t *list, const qcvm_string_t id)
{
	START_TIMER(list->vm, StringRelease);
	
	const int32_t index = (int32_t)(-id) - 1;

	assert(index >= 0 && index < list->strings_size);

	qcvm_ref_counted_string_t *str = &list->strings[index];

	assert(str->ref_count);
		
	str->ref_count--;

	if (!str->ref_count)
		qcvm_string_list_unstore(list, id);

	END_TIMER(list->vm, PROFILE_TIMERS);
}

static void qcvm_string_list_ref_link(qcvm_string_list_t *list, uint32_t hash, const qcvm_string_t id, const void *ptr)
{
	// see if we need to expand the hashes list
	if (!list->ref_storage_free)
	{
		const size_t old_size = list->ref_storage_allocated;
		list->ref_storage_allocated += REF_STRING_RESERVE * 2;

		qcvm_debug(list->vm, "Increased ref string pointer storage to %u\n", list->ref_storage_allocated);

		qcvm_ref_storage_hash_t	*old_ref_storage_data = list->ref_storage_data;
		list->ref_storage_data = (qcvm_ref_storage_hash_t*)qcvm_alloc(list->vm, sizeof(qcvm_ref_storage_hash_t) * list->ref_storage_allocated);

		list->ref_storage_free = NULL;

		if (old_ref_storage_data)
		{
			memcpy(list->ref_storage_data, old_ref_storage_data, sizeof(qcvm_ref_storage_hash_t) * old_size);
			qcvm_mem_free(list->vm, old_ref_storage_data);
		}

		if (list->ref_storage_hashes)
			qcvm_mem_free(list->vm, list->ref_storage_hashes);

		list->ref_storage_hashes = (qcvm_ref_storage_hash_t**)qcvm_alloc(list->vm, sizeof(qcvm_ref_storage_hash_t*) * list->ref_storage_allocated);

		// re-hash since hashs changed
		for (qcvm_ref_storage_hash_t *h = list->ref_storage_data; h < list->ref_storage_data + list->ref_storage_allocated; h++)
		{
			// this is a free pointer, so link us into the free ptr list
			if (!h->ptr)
			{
				h->hash_next = list->ref_storage_free;

				if (list->ref_storage_free)
					list->ref_storage_free->hash_prev = h;

				list->ref_storage_free = h;
				continue;
			}

			h->hash_value = Q_hash_pointer((uint32_t)h->ptr, list->ref_storage_allocated);

			// we have to set these up next pass since we can't really tell if
			// the pointer is good or not
			h->hash_next = h->hash_prev = NULL;
		}

		// re-link hash buckets
		for (qcvm_ref_storage_hash_t *h = list->ref_storage_data; h < list->ref_storage_data + list->ref_storage_allocated; h++)
		{
			if (!h->ptr)
				continue;

			h->hash_next = list->ref_storage_hashes[h->hash_value];

			if (h->hash_next)
				h->hash_next->hash_prev = h;

			list->ref_storage_hashes[h->hash_value] = h;
		}

		// re-hash, because wrap changes
		hash = Q_hash_pointer((uint32_t)ptr, list->ref_storage_allocated);
	}

	// pop a free pointer off the free list
	qcvm_ref_storage_hash_t *hashed = list->ref_storage_free;

	list->ref_storage_free = list->ref_storage_free->hash_next;

	if (list->ref_storage_free)
		list->ref_storage_free->hash_prev = NULL;

	hashed->hash_next = hashed->hash_prev = NULL;

	// hash us in
	hashed->ptr = ptr;
	hashed->id = id;
	hashed->hash_value = hash;
	qcvm_ref_storage_hash_t *old_head = list->ref_storage_hashes[hash];
	hashed->hash_next = old_head;
	if (old_head)
		old_head->hash_prev = hashed;
	list->ref_storage_hashes[hash] = hashed;

	list->ref_storage_stored++;
}

static void qcvm_string_list_ref_unlink(qcvm_string_list_t *list, qcvm_ref_storage_hash_t *hashed)
{
	// if we were the head, swap us out first
	if (list->ref_storage_hashes[hashed->hash_value] == hashed)
		list->ref_storage_hashes[hashed->hash_value] = hashed->hash_next;

	// unlink hashed
	if (hashed->hash_prev)
		hashed->hash_prev->hash_next = hashed->hash_next;

	if (hashed->hash_next)
		hashed->hash_next->hash_prev = hashed->hash_prev;

	// put into free list
	hashed->hash_next = list->ref_storage_free;
	hashed->hash_prev = NULL;
	if (list->ref_storage_free)
		list->ref_storage_free->hash_prev = hashed;
	list->ref_storage_free = hashed;

	list->ref_storage_stored--;
}

static inline qcvm_ref_storage_hash_t *qcvm_string_list_get_storage_hash(qcvm_string_list_t *list, const uint32_t hash)
{
	if (!list->ref_storage_stored)
		return NULL;

	return list->ref_storage_hashes[hash];
}

void qcvm_string_list_mark_ref_copy(qcvm_string_list_t *list, const qcvm_string_t id, const void *ptr)
{
	START_TIMER(list->vm, StringMark);

	uint32_t hash = Q_hash_pointer((uint32_t)ptr, list->ref_storage_allocated);
	qcvm_ref_storage_hash_t *hashed = qcvm_string_list_get_storage_hash(list, hash);

	for (; hashed; hashed = hashed->hash_next)
	{
		if (hashed->ptr == ptr)
		{
			// it's *possible* for a seemingly no-op to occur in some cases
			// (for instance, a call into function which copies PARM0 into locals+0, then
			// copies locals+0 back into PARM0 for calling a function). because PARM0
			// doesn't release its ref until its value changes, we treat this as a no-op.
			// if we released every time the value changes (even to the same value it already
			// had) this would effectively be the same behavior.
			if (id == hashed->id)
			{
				END_TIMER(list->vm, PROFILE_TIMERS);
				return;
			}
			
			// we're stomping over another string, so unlink us
			qcvm_string_list_ref_unlink(list, hashed);
			break;
		}
	}

	// increase ref count
	qcvm_string_list_acquire(list, id);

	// link!
	qcvm_string_list_ref_link(list, hash, id, ptr);

	END_TIMER(list->vm, PROFILE_TIMERS);
}

void qcvm_string_list_check_ref_unset(qcvm_string_list_t *list, const void *ptr, const size_t span, const bool assume_changed)
{
	START_TIMER(list->vm, StringCheckUnset);

	for (size_t i = 0; i < span; i++)
	{
		const qcvm_global_t *gptr = (const qcvm_global_t *)ptr + i;

		qcvm_ref_storage_hash_t *hashed = qcvm_string_list_get_storage_hash(list, Q_hash_pointer((uint32_t)gptr, list->ref_storage_allocated));

		for (; hashed; hashed = hashed->hash_next)
			if (hashed->ptr == ptr)
				break;

		if (!hashed)
			continue;

		const qcvm_string_t old = hashed->id;

		if (!assume_changed)
		{
			const qcvm_string_t newstr = *(const qcvm_string_t *)gptr;

			// still here, so we probably just copied to ourselves or something
			if (newstr == old)
				continue;
		}

		// not here! release and unmark
		qcvm_string_list_release(list, old);

		// unlink
		qcvm_string_list_ref_unlink(list, hashed);
	}

	END_TIMER(list->vm, PROFILE_TIMERS);
}

qcvm_string_t *qcvm_string_list_has_ref(qcvm_string_list_t *list, const void *ptr)
{
	START_TIMER(list->vm, StringHasRef);

	qcvm_ref_storage_hash_t *hashed = qcvm_string_list_get_storage_hash(list, Q_hash_pointer((uint32_t)ptr, list->ref_storage_allocated));

	for (; hashed; hashed = hashed->hash_next)
	{
		if (hashed->ptr == ptr)
		{
			qcvm_string_t *rv = &hashed->id;
			END_TIMER(list->vm, PROFILE_TIMERS);
			return rv;
		}
	}
	
	END_TIMER(list->vm, PROFILE_TIMERS);
	return NULL;
}

void qcvm_string_list_mark_refs_copied(qcvm_string_list_t *list, const void *src, const void *dst, const size_t span)
{
	// unref any strings that were in dst
	qcvm_string_list_check_ref_unset(list, dst, span, false);
	
	START_TIMER(list->vm, StringMarkRefsCopied);

	// grab list of fields that have strings
	for (size_t i = 0; i < span; i++)
	{
		const qcvm_global_t *sptr = (const qcvm_global_t *)src + i;
		qcvm_string_t *str;

		if (!(str = qcvm_string_list_has_ref(list, sptr)))
			continue;
		
		// mark them as being inside of src as well now
		const qcvm_global_t *dptr = (const qcvm_global_t *)dst + i;
		qcvm_string_list_mark_ref_copy(list, *str, dptr);
	}

	END_TIMER(list->vm, PROFILE_TIMERS);
}

void qcvm_string_list_mark_if_has_ref(qcvm_string_list_t *list, const void *src_ptr, const void *dst_ptr, const size_t span)
{
	START_TIMER(list->vm, StringMarkIfHasRef);

	for (size_t i = 0; i < span; i++)
	{
		const qcvm_global_t *src_gptr = (const qcvm_global_t *)src_ptr + i;
		const qcvm_global_t *dst_gptr = (const qcvm_global_t *)dst_ptr + i;

		qcvm_ref_storage_hash_t *hashed = qcvm_string_list_get_storage_hash(list, Q_hash_pointer((uint32_t)src_gptr, list->ref_storage_allocated));

		for (; hashed; hashed = hashed->hash_next)
		{
			if (hashed->ptr == src_gptr)
			{
				qcvm_string_list_mark_ref_copy(list, hashed->id, dst_gptr);
				break;
			}
		}
	}

	END_TIMER(list->vm, PROFILE_TIMERS);
}

bool qcvm_string_list_is_ref_counted(qcvm_string_list_t *list, const qcvm_string_t id)
{
	const int32_t index = (int32_t)(-id) - 1;
	return index < list->strings_size && list->strings[index].str;
}

qcvm_string_backup_t qcvm_string_list_pop_ref(qcvm_string_list_t *list, const void *ptr)
{
	START_TIMER(list->vm, StringPopRef);

	qcvm_ref_storage_hash_t *hashed = qcvm_string_list_get_storage_hash(list, Q_hash_pointer((uint32_t)ptr, list->ref_storage_allocated));

	for (; hashed; hashed = hashed->hash_next)
		if (hashed->ptr == ptr)
			break;

	assert(hashed);

	const qcvm_string_t id = hashed->id;

	const qcvm_string_backup_t popped_ref = (qcvm_string_backup_t) { ptr, id };

	qcvm_string_list_ref_unlink(list, hashed);

	END_TIMER(list->vm, PROFILE_TIMERS);

	return popped_ref;
}

void qcvm_string_list_push_ref(qcvm_string_list_t *list, const qcvm_string_backup_t *backup)
{
	START_TIMER(list->vm, StringPushRef);

	const uint32_t hash = Q_hash_pointer((uint32_t)backup->ptr, list->ref_storage_allocated);
	qcvm_ref_storage_hash_t *hashed = qcvm_string_list_get_storage_hash(list, hash);

	for (; hashed; hashed = hashed->hash_next)
	{
		if (hashed->ptr == backup->ptr)
		{
			// somebody stole our ptr >:(
			if (backup->id == hashed->id)
			{
				// ..oh maybe it was us. no-op!
				END_TIMER(list->vm, PROFILE_TIMERS);
				return;
			}

			qcvm_string_list_release(list, hashed->id);
			qcvm_string_list_ref_unlink(list, hashed);
			break;
		}
	}

	const int32_t index = (int32_t)(-backup->id) - 1;

	// simple restore
	if ((index >= 0 && index < list->strings_size) && list->strings[index].str)
	{
		qcvm_string_list_ref_link(list, hash, backup->id, backup->ptr);
		END_TIMER(list->vm, PROFILE_TIMERS);
		return;
	}

	qcvm_error(list->vm, "unable to push string backup");
}

static void qcvm_string_list_write_state(qcvm_string_list_t *list, FILE *fp)
{
	for (qcvm_ref_counted_string_t *s = list->strings; s < list->strings + list->strings_size; s++)
	{
		if (!s->str)
			continue;

		fwrite(&s->length, sizeof(s->length), 1, fp);
		fwrite(s->str, sizeof(char), s->length, fp);
	}

	const size_t len = 0;
	fwrite(&len, sizeof(len), 1, fp);
}

static void qcvm_string_list_read_state(qcvm_string_list_t *list, FILE *fp)
{
	while (true)
	{
		size_t len;

		fread(&len, sizeof(len), 1, fp);

		if (!len)
			break;

		char *s = qcvm_temp_buffer(list->vm, len);
		fread(s, sizeof(char), len, fp);
		s[len] = 0;

		// does not acquire, since entity/game state does that itself
		qcvm_store_or_find_string(list->vm, s);
	}
}

static void qcvm_builtin_list_init(qcvm_builtin_list_t *list, qcvm_t *vm)
{
	list->vm = vm;
}

qcvm_builtin_t qcvm_builtin_list_get(qcvm_builtin_list_t *list, const qcvm_func_t func)
{
	assert(func < 0);

	const int32_t index = (-func) - 1;

	assert(index < list->count);

	return list->list[index];
}

void qcvm_builtin_list_register(qcvm_builtin_list_t *list, const char *name, qcvm_builtin_t builtin)
{
	for (qcvm_function_t *func = list->vm->functions; func < list->vm->functions + list->vm->functions_size; func++)
	{
		if (func->id || func->name_index == STRING_EMPTY)
			continue;

		if (strcmp(qcvm_get_string(list->vm, func->name_index), name) == 0)
		{
			if (list->registered == list->count)
				qcvm_error(list->vm, "Builtin list overrun");

			const int32_t index = list->registered;
			func->id = (qcvm_func_t)(-(index + 1));
			list->list[index] = builtin;
			list->registered++;
			return;
		}
	}

	qcvm_debug(list->vm, "No builtin to assign to %s\n", name);
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

static void qcvm_field_wrap_list_init(qcvm_field_wrap_list_t *list, qcvm_t *vm)
{
	list->vm = vm;
}

void qcvm_field_wrap_list_register(qcvm_field_wrap_list_t *list, const char *field_name, const size_t field_offset, const size_t client_offset, qcvm_field_setter_t setter)
{
	for (qcvm_definition_t *f = list->vm->fields; f < list->vm->fields + list->vm->fields_size; f++)
	{
		if (f->name_index == STRING_EMPTY)
			continue;
		else if (strcmp(qcvm_get_string(list->vm, f->name_index), field_name))
			continue;

		assert((f->global_index + field_offset) >= 0 && (f->global_index + field_offset) < list->vm->fields_size);

		qcvm_field_wrapper_t *wrapper = (qcvm_field_wrapper_t *)qcvm_alloc(list->vm, sizeof(qcvm_field_wrapper_t));
		*wrapper = (qcvm_field_wrapper_t) {
			f,
			f->global_index + field_offset,
			client_offset,
			setter,
			list->wrap_head
		};
		list->wrap_head = wrapper;
		list->field_range_min = (list->field_range_min == 0) ? wrapper->field_offset : minsz(list->field_range_min, wrapper->field_offset);
		list->field_range_max = (list->field_range_max == 0) ? (wrapper->field_offset + 1) : maxsz(list->field_range_max, wrapper->field_offset + 1);
		return;
	}

	gi.dprintf("QCVM WARNING: can't find field %s in progs", field_name);
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
		new_stack->locals = (qcvm_stack_local_t *)qcvm_alloc(state->vm, sizeof(qcvm_stack_local_t) * state->vm->highest_stack);
		qcvm_stack_needs_resize(new_stack);
	}

	if (state->stack_allocated != STACK_RESERVE)
		qcvm_debug(state->vm, "Stack size increased to %i\n", state->stack_allocated);
}

qcvm_stack_t *qcvm_state_stack_push(qcvm_state_t *state)
{
	state->current++;

	if (state->current == state->stack_allocated)
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
	memset(vm, 0, sizeof(*vm));
	qcvm_string_list_init(&vm->dynamic_strings, vm);
	qcvm_builtin_list_init(&vm->builtins, vm);
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
#ifdef ALLOW_DEBUGGING
	qcvm_break_on_current_statement((qcvm_t *)vm);
	__debugbreak();
#endif

	gi.error("%s", buffer);
}

#ifdef _DEBUG
void qcvm_debug(const qcvm_t *vm, const char *format, ...)
{
	va_list         argptr;
	static char		buffer[512];

	va_start(argptr, format);
	vsnprintf(buffer, sizeof(buffer), format, argptr);
	va_end(argptr);

	gi.dprintf("QCVM DEBUG: %s", buffer);
}
#endif

void *qcvm_alloc(const qcvm_t *vm, size_t size)
{
	return gi.TagMalloc(size, TAG_GAME);
}

void qcvm_mem_free(const qcvm_t *vm, void *ptr)
{
	gi.TagFree(ptr);
}

void qcvm_set_allowed_stack(qcvm_t *vm, const void *ptr, const size_t length)
{
	if (length % 4 != 0)
		qcvm_error(vm, "QCVM 'Allowed Stack' must be a multiple of 4.");

	vm->allowed_stack = ptr;
	vm->allowed_stack_size = length / sizeof(qcvm_global_t);
}

qcvm_global_t *qcvm_get_global(qcvm_t *vm, const qcvm_global_t g)
{
#ifdef ALLOW_PROFILING
	if ((vm->profile_flags & PROFILE_FIELDS) && vm->state.current >= 0 && vm->state.stack[vm->state.current].profile)
		vm->state.stack[vm->state.current].profile->fields[NumGlobalsFetched]++;
#endif

	return vm->global_data + g;
}

const qcvm_global_t *qcvm_get_const_global(const qcvm_t *vm, const qcvm_global_t g)
{
#ifdef ALLOW_PROFILING
	if ((vm->profile_flags & PROFILE_FIELDS) && vm->state.current >= 0 && vm->state.stack[vm->state.current].profile)
		vm->state.stack[vm->state.current].profile->fields[NumGlobalsFetched]++;
#endif

	return vm->global_data + g;
}

void *qcvm_get_global_ptr(qcvm_t *vm, const qcvm_global_t global, const size_t value_size)
{
	const qcvm_pointer_t address = *(const qcvm_pointer_t*)qcvm_get_const_global(vm, global);

	assert((value_size % 4) == 0);

	if (!qcvm_pointer_valid(vm, address, false, value_size / sizeof(qcvm_global_t)))
		qcvm_error(vm, "bad address");

	return (void *)(qcvm_resolve_pointer(vm, address));
}

void qcvm_set_global(qcvm_t *vm, const qcvm_global_t global, const void *value, const size_t value_size)
{
#ifdef ALLOW_PROFILING
	if ((vm->profile_flags & PROFILE_FIELDS) && vm->state.current >= 0 && vm->state.stack[vm->state.current].profile)
		vm->state.stack[vm->state.current].profile->fields[NumGlobalsSet]++;
#endif

	if (global == GLOBAL_NULL)
		qcvm_error(vm, "attempt to overwrite 0");

	assert((value_size % 4) == 0);

	void *dst = qcvm_get_global(vm, global);
	memcpy(dst, value, value_size);
	qcvm_string_list_check_ref_unset(&vm->dynamic_strings, dst, value_size / sizeof(qcvm_global_t), false);
}

qcvm_string_t qcvm_set_global_str(qcvm_t *vm, const qcvm_global_t global, const char *value)
{
	qcvm_string_t str = qcvm_store_or_find_string(vm, value);
	qcvm_set_global_typed_value(qcvm_string_t, vm, global, str);

	if (qcvm_string_list_is_ref_counted(&vm->dynamic_strings, str))
		qcvm_string_list_mark_ref_copy(&vm->dynamic_strings, str, qcvm_get_global(vm, global));

	return str;
}

qcvm_string_t qcvm_set_string_ptr(qcvm_t *vm, void *ptr, const char *value)
{
	qcvm_string_t str = qcvm_store_or_find_string(vm, value);
	*(qcvm_string_t *)ptr = str;
	qcvm_string_list_check_ref_unset(&vm->dynamic_strings, ptr, sizeof(qcvm_string_t) / sizeof(qcvm_global_t), false);

	if (qcvm_string_list_is_ref_counted(&vm->dynamic_strings, str))
		qcvm_string_list_mark_ref_copy(&vm->dynamic_strings, str, ptr);

	return str;
}

// safe way of copying globals between other globals
void qcvm_copy_globals(qcvm_t *vm, const qcvm_global_t dst, const qcvm_global_t src, const size_t size)
{
	const size_t count = size / sizeof(qcvm_global_t);

	const void *src_ptr = qcvm_get_global(vm, src);
	void *dst_ptr = qcvm_get_global(vm, dst);

	memcpy(dst_ptr, src_ptr, size);
	qcvm_string_list_check_ref_unset(&vm->dynamic_strings, dst_ptr, count, false);

	// if there were any ref strings in src, make sure they are
	// reffed in dst too
	qcvm_string_list_mark_if_has_ref(&vm->dynamic_strings, src_ptr, dst_ptr, count);
}

const char *qcvm_stack_trace(const qcvm_t *vm)
{
	const char *str = "> ";

	for (qcvm_stack_t *s = &vm->state.stack[vm->state.current]; s >= vm->state.stack && s->function; s--)
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

		str = qcvm_temp_format(vm, "%s%s (%s:%i @ stmt %u)\n", str, func, file, qcvm_line_number_for(vm, s->statement), s->statement - vm->statements);
	}

	return str;
}

edict_t *qcvm_ent_to_entity(const qcvm_ent_t ent, bool allow_invalid)
{
	if (ent == ENT_INVALID)
	{
		if (!allow_invalid)
			return NULL;
		else
			return itoe(MAX_EDICTS);
	}
	else if (ent == ENT_WORLD)
		return itoe(0);

	return itoe(((uint8_t *)(ent) - (uint8_t *)(globals.edicts)) / globals.edict_size);
}

qcvm_ent_t qcvm_entity_to_ent(edict_t *ent)
{
	if (ent == NULL)
		return ENT_INVALID;
	else if (ent->s.number == 0)
		return ENT_WORLD;

	return (qcvm_ent_t)(ptrdiff_t)ent;
}

edict_t *qcvm_argv_entity(const qcvm_t *vm, const uint8_t d)
{
	return qcvm_ent_to_entity(*qcvm_get_const_global_typed(qcvm_ent_t, vm, qcvm_global_offset(GLOBAL_PARM0, d * 3)), false);
}

qcvm_string_t qcvm_argv_string_id(const qcvm_t *vm, const uint8_t d)
{
	return *qcvm_get_const_global_typed(qcvm_string_t, vm, qcvm_global_offset(GLOBAL_PARM0, d * 3));
}

const char *qcvm_argv_string(const qcvm_t *vm, const uint8_t d)
{
	return qcvm_get_string(vm, qcvm_argv_string_id(vm, d));
}

int32_t qcvm_argv_int32(const qcvm_t *vm, const uint8_t d)
{
	return *qcvm_get_const_global_typed(int32_t, vm, qcvm_global_offset(GLOBAL_PARM0, d * 3));
}

vec_t qcvm_argv_float(const qcvm_t *vm, const uint8_t d)
{
	return *qcvm_get_const_global_typed(vec_t, vm, qcvm_global_offset(GLOBAL_PARM0, d * 3));
}

vec3_t qcvm_argv_vector(const qcvm_t *vm, const uint8_t d)
{
	return *qcvm_get_const_global_typed(vec3_t, vm, qcvm_global_offset(GLOBAL_PARM0, d * 3));
}

qcvm_pointer_t qcvm_argv_pointer(const qcvm_t *vm, const uint8_t d)
{
	return *qcvm_get_const_global_typed(qcvm_pointer_t, vm, qcvm_global_offset(GLOBAL_PARM0, d * 3));
}

void qcvm_return_float(qcvm_t *vm, const vec_t value)
{
	qcvm_set_global_typed_value(vec_t, vm, GLOBAL_RETURN, value);
}

void qcvm_return_vector(qcvm_t *vm, const vec3_t value)
{
	qcvm_set_global_typed_value(vec3_t, vm, GLOBAL_RETURN, value);
}
	
void qcvm_return_entity(qcvm_t *vm, const edict_t *value)
{
	const int32_t as_int = (int32_t)value;
	qcvm_set_global_typed_value(int32_t, vm, GLOBAL_RETURN, as_int);
}
	
void qcvm_return_int32(qcvm_t *vm, const int32_t value)
{
	qcvm_set_global_typed_value(int32_t, vm, GLOBAL_RETURN, value);
}

void qcvm_return_func(qcvm_t *vm, const qcvm_func_t func)
{
	qcvm_set_global_typed_value(qcvm_func_t, vm, GLOBAL_RETURN, func);
}

void qcvm_return_string_id(qcvm_t *vm, const qcvm_string_t str)
{
	qcvm_set_global_typed_value(qcvm_func_t, vm, GLOBAL_RETURN, str);
}

void qcvm_return_string(qcvm_t *vm, const char *str)
{
	if (!(str >= vm->string_data && str < vm->string_data + vm->string_size))
	{
		qcvm_set_global_str(vm, GLOBAL_RETURN, str); // dynamic
		return;
	}

	const qcvm_string_t s = (str == NULL || *str == 0) ? STRING_EMPTY : (qcvm_string_t)(str - vm->string_data);
	qcvm_set_global_typed_value(qcvm_string_t, vm, GLOBAL_RETURN, s);
}

void qcvm_return_pointer(qcvm_t *vm, const qcvm_pointer_t ptr)
{
#ifdef _DEBUG
	if (!qcvm_pointer_valid(vm, ptr, false, 1))
		qcvm_debug(vm, "Invalid pointer returned; writes to this will fail");
#endif

	qcvm_set_global_typed_value(qcvm_pointer_t, vm, GLOBAL_RETURN, ptr);
}

bool qcvm_find_string(qcvm_t *vm, const char *value, qcvm_string_t *rstr)
{
	START_TIMER(vm, StringFind);

	*rstr = STRING_EMPTY;

	if (!value || !*value)
	{
		END_TIMER(vm, PROFILE_TIMERS);
		return true;
	}

	// check built-ins
	const uint32_t hash = Q_hash_string(value, vm->string_size);

	for (qcvm_string_hash_t *hashed = vm->string_hashes[hash]; hashed; hashed = hashed->hash_next)
	{
		if (!strcmp(hashed->str, value))
		{
			*rstr = (qcvm_string_t)(hashed->str - vm->string_data);
			END_TIMER(vm, PROFILE_TIMERS);
			return true;
		}
	}

	// check temp strings
	// TODO: hash these too
	for (qcvm_ref_counted_string_t *s = vm->dynamic_strings.strings; s < vm->dynamic_strings.strings + vm->dynamic_strings.strings_size; s++)
	{
		if (!s->str)
			continue;

		const char *str = s->str;

		if (str && strcmp(value, str) == 0)
		{
			*rstr = -((s - vm->dynamic_strings.strings) + 1);
			END_TIMER(vm, PROFILE_TIMERS);
			return true;
		}
	}
	
	END_TIMER(vm, PROFILE_TIMERS);
	return false;
}

// Note: DOES NOT ACQUIRE IF REF COUNTED!!
// Note: currently *copies* value if it's acquired
qcvm_string_t qcvm_store_or_find_string(qcvm_t *vm, const char *value)
{
	// check built-ins
	qcvm_string_t str;

	if (qcvm_find_string(vm, value, &str))
		return str;

	const size_t len = strlen(value);
	char *copy = (char *)qcvm_alloc(vm, sizeof(char) * len + 1);
	memcpy(copy, value, sizeof(char) * strlen(value));
	copy[len] = 0;

	return qcvm_string_list_store(&vm->dynamic_strings, copy, len);
}

#ifdef ALLOW_DEBUGGING
static qcvm_eval_result_t qcvm_value_from_ptr(const qcvm_definition_t *def, const void *ptr)
{
	switch (def->id & ~TYPE_GLOBAL)
	{
	default:
		return (qcvm_eval_result_t) { .type = TYPE_VOID };
	case TYPE_STRING:
		return (qcvm_eval_result_t) { .type = TYPE_STRING, .strid = *(const qcvm_string_t *)(ptr) };
	case TYPE_FLOAT:
		return (qcvm_eval_result_t) { .type = TYPE_FLOAT, .single = *(const vec_t *)(ptr) };
	case TYPE_VECTOR:
		return (qcvm_eval_result_t) { .type = TYPE_VECTOR, .vector = *(const vec3_t *)(ptr) };
	case TYPE_ENTITY:
		return (qcvm_eval_result_t) { .type = TYPE_ENTITY, .entid = *(const qcvm_ent_t *)(ptr) };
	case TYPE_FIELD:
		return (qcvm_eval_result_t) { .type = TYPE_VOID };
	case TYPE_FUNCTION:
		return (qcvm_eval_result_t) { .type = TYPE_FUNCTION, .funcid = *(const qcvm_func_t *)(ptr) };
	case TYPE_POINTER:
		return (qcvm_eval_result_t) { .type = TYPE_POINTER, .ptr = (void *)(*(const int *)(ptr)) };
	case TYPE_INTEGER:
		return (qcvm_eval_result_t) { .type = TYPE_INTEGER, .integer = *(const int32_t *)(ptr) };
	}
}

static qcvm_eval_result_t qcvm_value_from_global(qcvm_t *vm, const qcvm_definition_t *def)
{
	return qcvm_value_from_ptr(def, qcvm_get_global(vm, def->global_index));
}

static qcvm_eval_result_t qcvm_evaluate_from_local_or_global(qcvm_t *vm, const char *variable)
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

	return (qcvm_eval_result_t) { .type = TYPE_VOID };
}

qcvm_eval_result_t qcvm_evaluate(qcvm_t *vm, const char *variable)
{
	const char *dot = strchr(variable, '.');
	if (dot)
	{
		char evaluate_left[MAX_INFO_STRING];

		strncpy(evaluate_left, variable, dot - variable);
		evaluate_left[dot - variable] = 0;

		// we have a . so we're either a entity, pointer or struct...
		qcvm_eval_result_t left_hand = qcvm_evaluate_from_local_or_global(vm, evaluate_left);

		if (left_hand.type == TYPE_ENTITY)
		{
			const char *right_context = dot + 1;
			const qcvm_ent_t ent = left_hand.entid;

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
				return (qcvm_eval_result_t) { .type = TYPE_VOID };

			return qcvm_value_from_ptr(field, qcvm_resolve_pointer(vm, qcvm_get_entity_field_pointer(vm, qcvm_ent_to_entity(ent, false), (int32_t)field->global_index)));
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
	vm->debug.step_depth = vm->state.stack_size;
	qcvm_wait_for_debugger_commands(vm);
}
#endif

int qcvm_line_number_for(const qcvm_t *vm, const qcvm_statement_t *statement)
{
	if (vm->linenumbers)
		return vm->linenumbers[statement - vm->statements];

	return 0;
}

void qcvm_enter(qcvm_t *vm, qcvm_function_t *function)
{
	qcvm_stack_t *cur_stack = (vm->state.current >= 0) ? &vm->state.stack[vm->state.current] : NULL;

	// save current stack space that will be overwritten by the new function
	if (cur_stack && function->num_args_and_locals)
	{
		for (size_t i = 0, arg = function->first_arg; i < function->num_args_and_locals; i++, arg++)
		{
			cur_stack->locals[i] = (qcvm_stack_local_t) { (qcvm_global_t)arg, *qcvm_get_global_typed(int32_t, vm, (qcvm_global_t)arg) };

			const void *ptr = qcvm_get_global(vm, (qcvm_global_t)arg);

			if (qcvm_string_list_has_ref(&vm->dynamic_strings, ptr))
				qcvm_stack_push_ref_string(cur_stack, qcvm_string_list_pop_ref(&vm->dynamic_strings, ptr));
		}

#ifdef ALLOW_PROFILING
		// entering a function call;
		// add time we spent up till now into self
		if (vm->profile_flags & PROFILE_FUNCTIONS)
		{
			cur_stack->profile->self += Q_time() - cur_stack->caller_start;
			cur_stack->callee_start = Q_time();
		}
#endif
	}

	qcvm_stack_t *new_stack = qcvm_state_stack_push(&vm->state);

	// set up current stack
	new_stack->function = function;
	new_stack->statement = &vm->statements[function->id - 1];

	// copy parameters
	for (size_t i = 0, arg_id = function->first_arg; i < function->num_args; i++)
		for (size_t s = 0; s < function->arg_sizes[i]; s++, arg_id++)
			qcvm_copy_globals_typed(qcvm_global_t, vm, arg_id, qcvm_global_offset(GLOBAL_PARM0, (i * 3) + s));

#ifdef ALLOW_PROFILING
	if (vm->profile_flags & (PROFILE_FUNCTIONS | PROFILE_FIELDS))
	{
		new_stack->profile = &vm->profile_data[function - vm->functions];

		if (vm->profile_flags & PROFILE_FIELDS)
			new_stack->profile->fields[NumSelfCalls]++;
		if (vm->profile_flags & PROFILE_FUNCTIONS)
			new_stack->caller_start = Q_time();
	}
#endif
}

void qcvm_leave(qcvm_t *vm)
{
	// restore stack
	qcvm_stack_t *current_stack = &vm->state.stack[vm->state.current];
	qcvm_state_stack_pop(&vm->state);
	qcvm_stack_t *prev_stack = (vm->state.current == -1) ? NULL : &vm->state.stack[vm->state.current];

	if (prev_stack && current_stack->function->num_args_and_locals)
	{
		for (const qcvm_stack_local_t *local = prev_stack->locals; local < prev_stack->locals + current_stack->function->num_args_and_locals; local++)
			qcvm_set_global_typed_value(qcvm_global_t, vm, local->global, local->value);

		for (const qcvm_string_backup_t *str = prev_stack->ref_strings; str < prev_stack->ref_strings + prev_stack->ref_strings_size; str++)
			qcvm_string_list_push_ref(&vm->dynamic_strings, str);

		prev_stack->ref_strings_size = 0;

#ifdef ALLOW_PROFILING
		if (vm->profile_flags & PROFILE_FUNCTIONS)
		{
			// we're coming back into prev_stack, so set up its caller_start
			prev_stack->caller_start = Q_time();
			// and add up the time we spent in the previous stack
			prev_stack->profile->ext += Q_time() - prev_stack->callee_start;
		}
#endif
	}
		
#ifdef ALLOW_PROFILING
	if (vm->profile_flags & PROFILE_FUNCTIONS)
		current_stack->profile->self += Q_time() - current_stack->caller_start;
#endif

	vm->allowed_stack = 0;
	vm->allowed_stack_size = 0;
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
	
const char *qcvm_get_string(const qcvm_t *vm, const qcvm_string_t str)
{
	if (str < 0)
		return qcvm_string_list_get(&vm->dynamic_strings, str);
	else if ((size_t)str >= vm->string_size)
		qcvm_error(vm, "bad string");

	return vm->string_data + (size_t)str;
}

size_t qcvm_get_string_length(const qcvm_t *vm, const qcvm_string_t str)
{
	if (str < 0)
		return qcvm_string_list_get_length(&vm->dynamic_strings, str);
	else if ((size_t)str >= vm->string_size)
		qcvm_error(vm, "bad string");

	return vm->string_lengths[(size_t)str];
}

qcvm_pointer_t qcvm_get_entity_field_pointer(qcvm_t *vm, edict_t *ent, const int32_t field)
{
	const qcvm_pointer_t ptr = qcvm_make_pointer(vm, QCVM_POINTER_ENTITY, (int32_t *)ent + field);

#ifdef _DEBUG
	if (!qcvm_pointer_valid(vm, ptr, false, 1))
		qcvm_error(vm, "Returning invalid entity field pointer");
#endif

	return ptr;
}

bool qcvm_pointer_valid(const qcvm_t *vm, const qcvm_pointer_t pointer, const bool allow_null, const size_t len)
{
	switch (pointer.type)
	{
	case QCVM_POINTER_NULL:
		return allow_null && !len && !pointer.offset;
	case QCVM_POINTER_GLOBAL:
		return (pointer.offset + len) <= vm->global_size;
	case QCVM_POINTER_ENTITY:
		return (pointer.offset + len) <= globals.edict_size * globals.max_edicts;
	case QCVM_POINTER_STACK:
		return vm->allowed_stack && (pointer.offset + len) <= vm->allowed_stack_size;
	}
}

void *qcvm_resolve_pointer(const qcvm_t *vm, const qcvm_pointer_t address)
{
	switch (address.type)
	{
	case QCVM_POINTER_NULL:
		return NULL;
	case QCVM_POINTER_GLOBAL:
		return vm->global_data + address.offset;
	case QCVM_POINTER_ENTITY:
		return ((int32_t *)globals.edicts) + address.offset;
	case QCVM_POINTER_STACK:
		return ((int32_t *)vm->allowed_stack) + address.offset;
	}
}

qcvm_pointer_t qcvm_make_pointer(const qcvm_t *vm, const qcvm_pointer_type_t type, const void *pointer)
{
	switch (type)
	{
	case QCVM_POINTER_NULL:
		return (qcvm_pointer_t) { type, 0 };
	case QCVM_POINTER_GLOBAL:
		return (qcvm_pointer_t) { type, (qcvm_global_t *)pointer - vm->global_data };
	case QCVM_POINTER_ENTITY:
		return (qcvm_pointer_t) { type, (int32_t *)pointer - (int32_t *)globals.edicts };
	case QCVM_POINTER_STACK:
		return (qcvm_pointer_t) { type, (int32_t *)pointer - (int32_t *)vm->allowed_stack };
	}
}

void qcvm_call_builtin(qcvm_t *vm, qcvm_function_t *function)
{
	qcvm_builtin_t func;

	if (!(func = qcvm_builtin_list_get(&vm->builtins, function->id)))
		qcvm_error(vm, "Bad builtin call number");

#ifdef ALLOW_PROFILING
	qcvm_profile_t *profile = &vm->profile_data[function - vm->functions];

	if (vm->profile_flags & PROFILE_FIELDS)
		profile->fields[NumSelfCalls]++;
	
	uint64_t start = 0;
	qcvm_stack_t *prev_stack = NULL;
	
	if (vm->profile_flags & PROFILE_FUNCTIONS)
	{
		start = Q_time();
		prev_stack = (vm->state.current >= 0 && vm->state.stack[vm->state.current].profile) ? &vm->state.stack[vm->state.current] : NULL;

		// moving into builtin; add up what we have so far into prev stack
		if (prev_stack)
		{
			prev_stack->profile->self += Q_time() - prev_stack->caller_start;
			prev_stack->callee_start = Q_time();
		}
	}
#endif

	func(vm);

#ifdef ALLOW_PROFILING
	if (vm->profile_flags & PROFILE_FUNCTIONS)
	{
		// builtins don't have external call time, just internal self time
		const uint64_t time_spent = Q_time() - start;
		profile->self += time_spent;

		// add time we spent in this function into the parent's call_into time
		if (prev_stack)
			prev_stack->profile->ext += Q_time() - prev_stack->callee_start;
	}
#endif
}

void qcvm_execute(qcvm_t *vm, qcvm_function_t *function)
{
	if (function->id < 0)
	{
		qcvm_call_builtin(vm, function);
		return;
	}

	int32_t enter_depth = 1;

	qcvm_enter(vm, function);

	while (1)
	{
		// get next statement
		qcvm_stack_t *current = &vm->state.stack[vm->state.current];
		const qcvm_statement_t *statement = ++current->statement;
		START_OPCODE_TIMER(vm, (statement->opcode & ~OP_BREAKPOINT));

#ifdef ALLOW_PROFILING
		if (vm->profile_flags & PROFILE_FIELDS)
			current->profile->fields[NumInstructions]++;
#endif

#ifdef ALLOW_DEBUGGING
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
					if (vm->debug.step_depth > vm->state.stack_size)
						qcvm_break_on_current_statement(vm);
				}
				// step over: either step out, or the next step that is in the same function + stack depth + not on same line
				else if (vm->debug.state == DEBUG_STEP_OVER)
				{
					if (vm->debug.step_depth > vm->state.stack_size ||
						(vm->debug.step_depth == vm->state.stack_size && vm->debug.step_function == current->function && qcvm_line_number_for(vm, vm->debug.step_statement) != qcvm_line_number_for(vm, current->statement)))
						qcvm_break_on_current_statement(vm);
				}
			}
		}

		const qcvm_opcode_t code = statement->opcode & ~OP_BREAKPOINT;
#else
		const qcvm_opcode_t code = statement->opcode;
#endif

		const qcvm_opcode_func_t func = qcvm_code_funcs[code];

		func(vm, statement->args, &enter_depth);

		END_TIMER(vm, PROFILE_OPCODES);

		if (!enter_depth)
			return;		// all done
	}
}

static const uint32_t QCVM_VERSION	= 1;

void qcvm_write_state(qcvm_t *vm, FILE *fp)
{
	fwrite(&QCVM_VERSION, sizeof(QCVM_VERSION), 1, fp);

	// write dynamic strings
	qcvm_string_list_write_state(&vm->dynamic_strings, fp);
}

void qcvm_read_state(qcvm_t *vm, FILE *fp)
{
	uint32_t ver;

	fread(&ver, sizeof(ver), 1, fp);

	if (ver != QCVM_VERSION)
		qcvm_error(vm, "bad VM version");

	// read dynamic strings
	qcvm_string_list_read_state(&vm->dynamic_strings, fp);
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
	gi.dprintf ("==== %s ====\n", __func__);

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

	vm->definition_map_by_id = (qcvm_definition_t **)qcvm_alloc(vm, sizeof(qcvm_definition_t) * vm->definitions_size);
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

	fseek(fp, header.sections.field.offset, SEEK_SET);
	VMLoadDefinitions(vm, fp, vm->fields, &header, vm->fields_size);

	vm->field_map_by_id = (qcvm_definition_t **)qcvm_alloc(vm, sizeof(qcvm_definition_t) * vm->fields_size);
	vm->field_hashes = (qcvm_definition_hash_t **)qcvm_alloc(vm, sizeof(qcvm_definition_hash_t *) * vm->fields_size);
	vm->field_hashes_data = (qcvm_definition_hash_t *)qcvm_alloc(vm, sizeof(qcvm_definition_hash_t) * vm->fields_size);

	for (qcvm_definition_t *field = vm->fields; field < vm->fields + vm->fields_size; field++)
	{
		vm->field_map_by_id[field->global_index] = field;

		qcvm_definition_hash_t *hashed = &vm->field_hashes_data[field - vm->fields];
		hashed->def = field;
		hashed->hash_value = Q_hash_string(qcvm_get_string(vm, field->name_index), vm->fields_size);
		hashed->hash_next = vm->field_hashes[hashed->hash_value];
		vm->field_hashes[hashed->hash_value] = hashed;
	}

	qcvm_field_wrap_list_init(&vm->field_wraps, vm);

	vm->functions_size = header.sections.function.size;
	vm->functions = (qcvm_function_t *)qcvm_alloc(vm, sizeof(qcvm_function_t) * vm->functions_size);

#ifdef ALLOW_PROFILING
	vm->profile_data = (qcvm_profile_t *)qcvm_alloc(vm, sizeof(qcvm_profile_t) * vm->functions_size);
#endif

	fseek(fp, header.sections.function.offset, SEEK_SET);
	fread(vm->functions, sizeof(qcvm_function_t), vm->functions_size, fp);
	
	vm->global_size = header.sections.globals.size;
	vm->global_data = (qcvm_global_t *)qcvm_alloc(vm, sizeof(qcvm_global_t) * vm->global_size);

	fseek(fp, header.sections.globals.offset, SEEK_SET);
	fread(vm->global_data, sizeof(qcvm_global_t), vm->global_size, fp);

	for (qcvm_function_t *func = vm->functions; func < vm->functions + vm->functions_size; func++)
	{
		if (func->id < 0)
			gi.dprintf("QCVM WARNING: Code contains old-school negative-indexed builtin. Use #0 for all builtins!");
		else if (func->id == 0 && func->name_index)
			vm->builtins.count++;

		vm->highest_stack = maxsz(vm->highest_stack, func->num_args_and_locals);
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

		if (lno_header.magic == lnotype && lno_header.ver == version && lno_header.numglobaldefs == header.sections.definition.size &&
			lno_header.numglobals == header.sections.globals.size && lno_header.numfielddefs == header.sections.field.size &&
			lno_header.numstatements == header.sections.statement.size)
		{
			vm->linenumbers = (int *)qcvm_alloc(vm, sizeof(*vm->linenumbers) * header.sections.statement.size);
			fread(vm->linenumbers, sizeof(int), header.sections.statement.size, fp);
			gi.dprintf("progs.lno line numbers loaded\n");
		}
		else
			gi.dprintf("Unsupported/outdated progs.lno file\n");

		fclose(fp);
	}
}

void qcvm_check(const qcvm_t *vm)
{
	for (qcvm_function_t *func = vm->functions; func < vm->functions + vm->functions_size; func++)
		if (func->id == 0 && func->name_index != STRING_EMPTY)
			gi.dprintf("Missing builtin function: %s\n", qcvm_get_string(vm, func->name_index));
}

#ifdef ALLOW_PROFILING
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
	"String Find"
};
#endif

void qcvm_shutdown(qcvm_t *vm)
{
#ifdef ALLOW_PROFILING
	if (vm->profile_flags & (PROFILE_FUNCTIONS | PROFILE_FIELDS))
	{
		FILE *fp = fopen(qcvm_temp_format(vm, "%sprofile.csv", vm->path), "wb");
		double all_total = 0;
		
		for (size_t i = 0; i < vm->functions_size; i++)
		{
			const qcvm_profile_t *profile = vm->profile_data + i;

			if (!profile->fields[NumSelfCalls] && !profile->self && !profile->ext)
				continue;

			const double total = profile->self / 1000000.0;
			all_total += total;
		}

		fprintf(fp, "ID,Name,Total (ms),Self(ms),Funcs(ms),Total (%%),Self (%%)");
	
		for (size_t i = 0; i < TotalProfileFields; i++)
			fprintf(fp, ",%s", profile_type_names[i]);
	
		fprintf(fp, "\n");

		for (size_t i = 0; i < vm->functions_size; i++)
		{
			const qcvm_profile_t *profile = vm->profile_data + i;

			if (!profile->fields[NumSelfCalls] && !profile->self && !profile->ext)
				continue;

			const qcvm_function_t *ff = vm->functions + i;
			const char *name = qcvm_get_string(vm, ff->name_index);
		
			const double self = profile->self / 1000000.0;
			const double ext = profile->ext / 1000000.0;
			const double total = self + ext;

			fprintf(fp, "%i,%s,%f,%f,%f,%f,%f", i, name, total, self, ext, (total / all_total) * 100, (self / all_total) * 100);
		
			for (qcvm_profiler_field_t f = 0; f < TotalProfileFields; f++)
				fprintf(fp, ",%i", profile->fields[f]);

			fprintf(fp, "\n");
		}

		fclose(fp);
	}
	
	if (vm->profile_flags & PROFILE_TIMERS)
	{
		FILE *fp = fopen(qcvm_temp_format(vm, "%stimers.csv", vm->path), "wb");
		double all_total = 0;

		for (size_t i = 0; i < TotalTimerFields; i++)
		{
			const qcvm_profile_timer_t *timer = vm->timers + i;
			const double total = timer->time / 1000000.0;
			all_total += total;
		}

		fprintf(fp, "Name,Count,Total (ms),Avg (ns),%%\n");

		for (size_t i = 0; i < TotalTimerFields; i++)
		{
			const qcvm_profile_timer_t *timer = vm->timers + i;
			const double total = timer->time / 1000000.0;

			fprintf(fp, "%s,%i,%f,%f,%f\n", timer_type_names[i], timer->count, total, (total / timer->count) * 1000, (total / all_total) * 100);
		}

		fclose(fp);
	}
	
	if (vm->profile_flags & PROFILE_OPCODES)
	{
		FILE *fp = fopen(qcvm_temp_format(vm, "%sopcodes.csv", vm->path), "wb");
		double all_total = 0;

		for (size_t i = 0; i < OP_NUMOPS; i++)
		{
			const qcvm_profile_timer_t *timer = vm->opcode_timers + i;

			if (!timer->count)
				continue;

			const double total = timer->time / 1000000.0;
			all_total += total;
		}

		fprintf(fp, "ID,Count,Total (ms),Avg (ns),%%\n");

		for (size_t i = 0; i < OP_NUMOPS; i++)
		{
			const qcvm_profile_timer_t *timer = vm->opcode_timers + i;

			if (!timer->count)
				continue;

			const double total = timer->time / 1000000.0;

			fprintf(fp, "%i,%i,%f,%f,%f\n", i, timer->count, total, (total / timer->count) * 1000, (total / all_total) * 100);
		}

		fclose(fp);
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
}