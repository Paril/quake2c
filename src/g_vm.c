#define QCVM_INTERNAL
#include "shared/shared.h"
#include "g_vm.h"
#include "vm_game.h"
#include "vm_opcodes.h"
#include "vm_math.h"
#include "vm_debug.h"
#include "vm_mem.h"
#include "vm_string.h"
#include "vm_ext.h"
#include "vm_file.h"

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
			qcvm_debug(list->vm, "Increased ref string storage to %u due to \"%s\"\n", list->strings_allocated, str);
		}

		index = (int32_t)list->strings_size++;
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
	assert(list->strings[index].str);
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
		list->ref_storage_allocated = Q_next_pow2(list->ref_storage_allocated + (REF_STRING_RESERVE * 2));

		qcvm_debug(list->vm, "Increased ref string pointer storage to %u due to \"%s\"\n", list->ref_storage_allocated, qcvm_get_string(list->vm, id));

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
#ifdef _DEBUG
	if (list->vm->state.current >= 0)
		hashed->stack = list->vm->state.stack[list->vm->state.current];
#endif
	hashed->hash_value = hash;
	qcvm_ref_storage_hash_t *old_head = list->ref_storage_hashes[hash];
	hashed->hash_next = old_head;
	if (old_head)
		old_head->hash_prev = hashed;
	list->ref_storage_hashes[hash] = hashed;

	list->ref_storage_stored++;
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

void qcvm_string_list_dump_refs(FILE *fp, qcvm_string_list_t *list)
{
	fprintf(fp, "strings: %" PRIuPTR " / %" PRIuPTR " (%" PRIuPTR " / %" PRIuPTR " free indices)\n", list->strings_size, list->strings_allocated, list->free_indices_size, list->free_indices_allocated);
	fprintf(fp, "ref pointers: %" PRIuPTR " stored / %" PRIuPTR "\n", list->ref_storage_stored, list->ref_storage_allocated);

	for (qcvm_ref_counted_string_t *str = list->strings; str < list->strings + list->strings_size; str++)
	{
		if (!str->str)
			continue;

		const qcvm_string_t id = (qcvm_string_t) -((str - list->strings) + 1);

		fprintf(fp, "%i\t%s\t%" PRIuPTR "\n", id, str->str, str->ref_count);

		for (qcvm_ref_storage_hash_t *hashed = list->ref_storage_data; hashed < list->ref_storage_data + list->ref_storage_allocated; hashed++)
		{
			if (!hashed->ptr)
				continue;
			else if (hashed->id != id)
				continue;

			const qcvm_string_t current_id = *(qcvm_string_t *)(hashed->ptr);
			const bool still_has_string = current_id == hashed->id;

			fprintf(fp, "\t%s\t%s\t%s (%u)\n", qcvm_dump_pointer(list->vm, (const qcvm_global_t *)hashed->ptr), qcvm_stack_entry(list->vm, &hashed->stack, false), still_has_string ? "valid" : "invalid", current_id);
		}
	}
}
#endif

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
	hashed->ptr = NULL;
	hashed->id = 0;
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

bool qcvm_string_list_check_ref_unset(qcvm_string_list_t *list, const void *ptr, const size_t span, const bool assume_changed)
{
	START_TIMER(list->vm, StringCheckUnset);

	bool any_unset = false;

	for (size_t i = 0; i < span; i++)
	{
		const qcvm_global_t *gptr = (const qcvm_global_t *)ptr + i;

		qcvm_ref_storage_hash_t *hashed = qcvm_string_list_get_storage_hash(list, Q_hash_pointer((uint32_t)gptr, list->ref_storage_allocated));

		for (; hashed; hashed = hashed->hash_next)
			if (hashed->ptr == gptr)
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

		any_unset = true;
	}

	END_TIMER(list->vm, PROFILE_TIMERS);

	return any_unset;
}

qcvm_string_t *qcvm_string_list_has_ref(qcvm_string_list_t *list, const void *ptr, qcvm_ref_storage_hash_t **hashed_ptr)
{
	START_TIMER(list->vm, StringHasRef);

	qcvm_ref_storage_hash_t *hashed = qcvm_string_list_get_storage_hash(list, Q_hash_pointer((uint32_t)ptr, list->ref_storage_allocated)), *next;

	for (; hashed; hashed = next)
	{
		next = hashed->hash_next;

		if (hashed->ptr == ptr)
		{
			qcvm_string_t *rv = &hashed->id;

			END_TIMER(list->vm, PROFILE_TIMERS);

			if (hashed_ptr)
				*hashed_ptr = hashed;

			return rv;
		}
	}
	
	END_TIMER(list->vm, PROFILE_TIMERS);
	return NULL;
}

void qcvm_string_list_mark_refs_copied(qcvm_string_list_t *list, const void *src, const void *dst, const size_t span)
{
	START_TIMER(list->vm, StringMarkRefsCopied);

	// grab list of fields that have strings
	for (size_t i = 0; i < span; i++)
	{
		const qcvm_global_t *sptr = (const qcvm_global_t *)src + i;
		qcvm_string_t *sstr = qcvm_string_list_has_ref(list, sptr, NULL);

		const qcvm_global_t *dptr = (const qcvm_global_t *)dst + i;
		qcvm_ref_storage_hash_t *hashed;
		qcvm_string_t *dstr = qcvm_string_list_has_ref(list, dptr, &hashed);

		// dst already has a string, check if it's the same ID
		if (dstr)
		{
			// we're copying same string, so just skip
			if (sstr && *sstr == *dstr)
				continue;

			// different strings, unref us
			qcvm_string_list_release(list, *dstr);
			qcvm_string_list_ref_unlink(list, hashed);
		}

		// no new string, so keep going
		if (!sstr)
			continue;
		
		// mark them as being inside of src as well now
		qcvm_string_list_mark_ref_copy(list, *sstr, dptr);
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
		qcvm_store_or_find_string(list->vm, s, len, true);
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

			const int32_t index = (int32_t) list->registered;
			func->id = (qcvm_func_t)(-(index + 1));
			list->list[index] = builtin;
			list->registered++;
			return;
		}
	}

	qcvm_debug(list->vm, "No builtin to assign to %s\n", name);
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

static void qcvm_field_wrap_list_init(qcvm_field_wrap_list_t *list, qcvm_t *vm)
{
	list->vm = vm;
	list->wraps = (qcvm_field_wrapper_t *)qcvm_alloc(list->vm, sizeof(qcvm_field_wrapper_t) * vm->field_real_size);
}

void qcvm_field_wrap_list_register(qcvm_field_wrap_list_t *list, const char *field_name, const size_t field_offset, const size_t struct_offset, qcvm_field_setter_t setter)
{
	for (qcvm_definition_t *f = list->vm->fields; f < list->vm->fields + list->vm->fields_size; f++)
	{
		if (f->name_index == STRING_EMPTY)
			continue;
		else if (strcmp(qcvm_get_string(list->vm, f->name_index), field_name))
			continue;

		assert((f->global_index + field_offset) >= 0 && (f->global_index + field_offset) < list->vm->field_real_size);

		qcvm_field_wrapper_t *wrapper = &list->wraps[f->global_index + field_offset];
		*wrapper = (qcvm_field_wrapper_t) {
			f,
			f->global_index + field_offset,
			strncmp(field_name, "client.", 7) == 0,
			struct_offset,
			setter
		};
		return;
	}

	list->vm->warning("QCVM WARNING: can't find field %s in progs\n", field_name);
}

void qcvm_field_wrap_list_check_set(qcvm_field_wrap_list_t *list, const void *ptr, const size_t span)
{
	// FIXME: this shouldn't be required ideally...
	if (!list->vm)
		return;

	START_TIMER(list->vm, WrapApply);

	// no entities involved in this wrap check (or no entities to check yet)
	if (ptr < list->vm->edicts || ptr >= (void *)((uint8_t *)list->vm->edicts + (list->vm->edict_size * list->vm->max_edicts)))
	{
		END_TIMER(list->vm, PROFILE_TIMERS);
		return;
	}

	// check where we're starting
	void *start = (void *)((uint8_t *)ptr - (uint8_t *)list->vm->edicts);
	edict_t *ent = (edict_t *)qcvm_itoe(list->vm, (int32_t)((ptrdiff_t)start / list->vm->edict_size));
	size_t offset = (const uint32_t *)ptr - (uint32_t *)ent;
	const int32_t *sptr = (const int32_t *)ptr;

	for (size_t i = 0; i < span; i++, sptr++, offset++)
	{
		// we're wrapping over to a new entity
		if (offset >= list->vm->field_real_size)
		{
			ent++;
			offset = 0;
		}

		const qcvm_field_wrapper_t *wrap = &list->vm->field_wraps.wraps[offset];

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

	END_TIMER(list->vm, PROFILE_TIMERS);
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
#endif

void *qcvm_alloc(const qcvm_t *vm, size_t size)
{
	return vm->alloc(size);
}

void qcvm_mem_free(const qcvm_t *vm, void *ptr)
{
	vm->free(ptr);
}

void qcvm_set_allowed_stack(qcvm_t *vm, const void *ptr, const size_t length)
{
	if (length % 4 != 0)
		qcvm_error(vm, "QCVM 'Allowed Stack' must be a multiple of 4.");

	vm->allowed_stack = ptr;
	vm->allowed_stack_size = length;
}

qcvm_global_t *qcvm_get_global(qcvm_t *vm, const qcvm_global_t g)
{
#ifdef ALLOW_INSTRUMENTING
	if ((vm->profile_flags & PROFILE_FIELDS) && vm->state.current >= 0 && vm->state.stack[vm->state.current].profile)
		vm->state.stack[vm->state.current].profile->fields[NumGlobalsFetched][vm->profiler_mark]++;
#endif

	return vm->global_data + g;
}

const qcvm_global_t *qcvm_get_const_global(const qcvm_t *vm, const qcvm_global_t g)
{
#ifdef ALLOW_INSTRUMENTING
	if ((vm->profile_flags & PROFILE_FIELDS) && vm->state.current >= 0 && vm->state.stack[vm->state.current].profile)
		vm->state.stack[vm->state.current].profile->fields[NumGlobalsFetched][vm->profiler_mark]++;
#endif

	return vm->global_data + g;
}

void *qcvm_get_global_ptr(qcvm_t *vm, const qcvm_global_t global, const size_t value_size)
{
	const qcvm_pointer_t address = *(const qcvm_pointer_t*)qcvm_get_const_global(vm, global);

	assert((value_size % 4) == 0);

	if (!qcvm_pointer_valid(vm, address, false, value_size))
		qcvm_error(vm, "bad address");

	return (void *)(qcvm_resolve_pointer(vm, address));
}

void qcvm_set_global(qcvm_t *vm, const qcvm_global_t global, const void *value, const size_t value_size)
{
#ifdef ALLOW_INSTRUMENTING
	if ((vm->profile_flags & PROFILE_FIELDS) && vm->state.current >= 0 && vm->state.stack[vm->state.current].profile)
		vm->state.stack[vm->state.current].profile->fields[NumGlobalsSet][vm->profiler_mark]++;
#endif

	if (global == GLOBAL_NULL)
		qcvm_error(vm, "attempt to overwrite 0");

	assert((value_size % 4) == 0);

	void *dst = qcvm_get_global(vm, global);
	memcpy(dst, value, value_size);
	qcvm_string_list_check_ref_unset(&vm->dynamic_strings, dst, value_size / sizeof(qcvm_global_t), false);
	qcvm_field_wrap_list_check_set(&vm->field_wraps, dst, value_size / sizeof(qcvm_global_t));
}

qcvm_string_t qcvm_set_global_str(qcvm_t *vm, const qcvm_global_t global, const char *value, const size_t len, const bool copy)
{
	qcvm_string_t str = qcvm_store_or_find_string(vm, value, len, copy);
	qcvm_set_global_typed_value(qcvm_string_t, vm, global, str);

	if (qcvm_string_list_is_ref_counted(&vm->dynamic_strings, str))
		qcvm_string_list_mark_ref_copy(&vm->dynamic_strings, str, qcvm_get_global(vm, global));

	return str;
}

qcvm_string_t qcvm_set_string_ptr(qcvm_t *vm, void *ptr, const char *value, const size_t len, const bool copy)
{
	qcvm_string_t str = qcvm_store_or_find_string(vm, value, len, copy);
	*(qcvm_string_t *)ptr = str;
	qcvm_string_list_check_ref_unset(&vm->dynamic_strings, ptr, sizeof(qcvm_string_t) / sizeof(qcvm_global_t), false);
	qcvm_field_wrap_list_check_set(&vm->field_wraps, ptr, sizeof(qcvm_string_t) / sizeof(qcvm_global_t));

	if (qcvm_string_list_is_ref_counted(&vm->dynamic_strings, str))
		qcvm_string_list_mark_ref_copy(&vm->dynamic_strings, str, ptr);

	return str;
}

// safe way of copying globals between other globals
void qcvm_copy_globals(qcvm_t *vm, const qcvm_global_t dst, const qcvm_global_t src, const size_t size)
{
	const size_t span = size / sizeof(qcvm_global_t);

	const void *src_ptr = qcvm_get_global(vm, src);
	void *dst_ptr = qcvm_get_global(vm, dst);

	memcpy(dst_ptr, src_ptr, size);

	qcvm_string_list_mark_refs_copied(&vm->dynamic_strings, src_ptr, dst_ptr, span);
	qcvm_field_wrap_list_check_set(&vm->field_wraps, dst_ptr, span);
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

	for (qcvm_stack_t *s = vm->state.stack; s < &vm->state.stack[vm->state.current] && s->function; s++)
	{
		if (compact)
			str = qcvm_temp_format(vm, "%s->%s", str, qcvm_stack_entry(vm, s, compact));
		else
			str = qcvm_temp_format(vm, "%s%s\n", str, qcvm_stack_entry(vm, s, compact));
	}

	return str;
}

edict_t *qcvm_ent_to_entity(const qcvm_t *vm, const qcvm_ent_t ent, bool allow_invalid)
{
	if (ent == ENT_INVALID)
	{
		if (!allow_invalid)
			return NULL;
		else
			return qcvm_itoe(vm, -1);
	}
	else if (ent == ENT_WORLD)
		return qcvm_itoe(vm, 0);

	assert(ent >= -1 && ent < MAX_EDICTS);

	return qcvm_itoe(vm, ent);
}

qcvm_ent_t qcvm_entity_to_ent(const qcvm_t *vm, const edict_t *ent)
{
	if (ent == NULL)
		return ENT_INVALID;

	assert(ent->s.number == ((uint8_t *)ent - (uint8_t *)vm->edicts) / vm->edict_size);

	return (qcvm_ent_t)ent->s.number;
}

edict_t *qcvm_argv_entity(const qcvm_t *vm, const uint8_t d)
{
	return qcvm_ent_to_entity(vm, *qcvm_get_const_global_typed(qcvm_ent_t, vm, qcvm_global_offset(GLOBAL_PARM0, d * 3)), false);
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
	const qcvm_ent_t val = qcvm_entity_to_ent(vm, value);
	qcvm_set_global_typed_value(int32_t, vm, GLOBAL_RETURN, val);
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
		qcvm_set_global_str(vm, GLOBAL_RETURN, str, strlen(str), true); // dynamic
		return;
	}

	const qcvm_string_t s = (str == NULL || *str == 0) ? STRING_EMPTY : (qcvm_string_t)(str - vm->string_data);
	qcvm_set_global_typed_value(qcvm_string_t, vm, GLOBAL_RETURN, s);
}

void qcvm_return_pointer(qcvm_t *vm, const qcvm_pointer_t ptr)
{
#ifdef _DEBUG
	if (!qcvm_pointer_valid(vm, ptr, false, sizeof(qcvm_global_t)))
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
			*rstr = (qcvm_string_t) -((s - vm->dynamic_strings.strings) + 1);
			END_TIMER(vm, PROFILE_TIMERS);
			return true;
		}
	}
	
	END_TIMER(vm, PROFILE_TIMERS);
	return false;
}

// Note: DOES NOT ACQUIRE IF REF COUNTED!!
qcvm_string_t qcvm_store_or_find_string(qcvm_t *vm, const char *value, const size_t len, const bool copy)
{
	// check built-ins
	qcvm_string_t str;

	if (qcvm_find_string(vm, value, &str))
		return str;

	if (copy)
	{
		char *strcopy = (char *)qcvm_alloc(vm, (sizeof(char) * len) + 1);
		memcpy(strcopy, value, sizeof(char) * len);
		strcopy[len] = 0;
		value = strcopy;
	}

	return qcvm_string_list_store(&vm->dynamic_strings, value, len);
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
		return (qcvm_eval_result_t) { .type = TYPE_POINTER, .ptr = (void *)(*(const int32_t *)(ptr)) };
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

			return qcvm_value_from_ptr(field, qcvm_resolve_pointer(vm, qcvm_get_entity_field_pointer(vm, qcvm_ent_to_entity(vm, ent, false), (int32_t)field->global_index)));
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

void *qcvm_fetch_handle(qcvm_t *vm, const int32_t id)
{
	if (id <= 0 || id > vm->handles.size)
		qcvm_error(vm, "invalid handle ID");

	void *handle = vm->handles.data[id - 1].handle;

	if (!handle)
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

void *qcvm_itoe(const qcvm_t *vm, const int32_t n)
{
	if (n == ENT_INVALID)
		return NULL;

	return (uint8_t *)vm->edicts + (n * vm->edict_size);
}

qcvm_pointer_t qcvm_get_entity_field_pointer(qcvm_t *vm, edict_t *ent, const int32_t field)
{
	const qcvm_pointer_t ptr = qcvm_make_pointer(vm, QCVM_POINTER_ENTITY, (int32_t *)ent + field);

#ifdef _DEBUG
	if (!qcvm_pointer_valid(vm, ptr, false, sizeof(qcvm_global_t)))
		qcvm_error(vm, "Returning invalid entity field pointer");
#endif

	return ptr;
}

bool qcvm_pointer_valid(const qcvm_t *vm, const qcvm_pointer_t pointer, const bool allow_null, const size_t len)
{
	switch (pointer.type)
	{
	case QCVM_POINTER_NULL:
	default:
		return allow_null && !len && !pointer.offset;
	case QCVM_POINTER_GLOBAL:
		return (pointer.offset + len) <= vm->global_size * sizeof(qcvm_global_t);
	case QCVM_POINTER_ENTITY:
		return (pointer.offset + len) <= vm->edict_size * vm->max_edicts * sizeof(qcvm_global_t);
	case QCVM_POINTER_STACK:
		return vm->allowed_stack && (pointer.offset + len) <= vm->allowed_stack_size;
	}
}

void *qcvm_resolve_pointer(const qcvm_t *vm, const qcvm_pointer_t address)
{
	switch (address.type)
	{
	case QCVM_POINTER_NULL:
	default:
		return NULL;
	case QCVM_POINTER_GLOBAL:
		return (uint8_t *)vm->global_data + address.offset;
	case QCVM_POINTER_ENTITY:
		return (uint8_t *)vm->edicts + address.offset;
	case QCVM_POINTER_STACK:
		return (uint8_t *)vm->allowed_stack + address.offset;
	}
}

qcvm_pointer_t qcvm_make_pointer(const qcvm_t *vm, const qcvm_pointer_type_t type, const void *pointer)
{
	switch (type)
	{
	case QCVM_POINTER_NULL:
	default:
		return (qcvm_pointer_t) { 0, type };
	case QCVM_POINTER_GLOBAL:
		return (qcvm_pointer_t) { (uint32_t)((const uint8_t *)pointer - (const uint8_t *)vm->global_data), type };
	case QCVM_POINTER_ENTITY:
		return (qcvm_pointer_t) { (uint32_t)((const uint8_t *)pointer - (const uint8_t *)vm->edicts), type };
	case QCVM_POINTER_STACK:
		return (qcvm_pointer_t) { (uint32_t)((const uint8_t *)pointer - (const uint8_t *)vm->allowed_stack), type };
	}
}

qcvm_pointer_t qcvm_offset_pointer(const qcvm_t *vm, const qcvm_pointer_t pointer, const size_t offset)
{
	return (qcvm_pointer_t) { (uint32_t)(pointer.offset + offset), pointer.type };
}

void qcvm_call_builtin(qcvm_t *vm, qcvm_function_t *function)
{
	qcvm_builtin_t func;

	if (!(func = qcvm_builtin_list_get(&vm->builtins, function->id)))
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

	qcvm_enter(vm, function);

	while (1)
	{
		// get next statement
		qcvm_stack_t *current = &vm->state.stack[vm->state.current];
		const qcvm_statement_t *statement = ++current->statement;

#ifdef ALLOW_INSTRUMENTING
		if (vm->profile_flags & PROFILE_FIELDS)
			current->profile->fields[NumInstructions][vm->profiler_mark]++;
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

		START_OPCODE_TIMER(vm, code);

		func(vm, statement->args, &enter_depth);

		END_TIMER(vm, PROFILE_OPCODES);

#ifdef ALLOW_PROFILING
		if (vm->profile_flags & PROFILE_SAMPLES)
		{
			if (!--vm->sample_id)
			{
				vm->sample_data[statement - vm->statements].count[vm->profiler_mark]++;
				vm->sample_id = vm->sample_rate;
			}
		}
#endif

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
			vm->warning("QCVM WARNING: Code contains old-school negative-indexed builtin. Use #0 for all builtins!");
			func->id = 0;
		}
		
		if (func->id == 0 && func->name_index)
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
		}

		fclose(fp);
	}

#ifdef ALLOW_INSTRUMENTING
	vm->profile_data = (qcvm_profile_t *)qcvm_alloc(vm, sizeof(qcvm_profile_t) * vm->functions_size);
#endif

#ifdef ALLOW_PROFILING
	vm->sample_data = (qcvm_sampling_t *)qcvm_alloc(vm, sizeof(qcvm_sampling_t) * vm->statements_size);
#endif

#if defined(ALLOW_INSTRUMENTING) || defined(ALLOW_PROFILING)
	if (vm->profile_flags & PROFILE_CONTINUOUS)
	{
		fp = fopen(qcvm_temp_format(vm, "%s%s.perf", vm->path, vm->profile_name), "rb");

		if (fp)
		{
#ifdef ALLOW_INSTRUMENTING
			fread(vm->profile_data, sizeof(qcvm_profile_t), vm->functions_size, fp);
			fread(vm->opcode_timers, sizeof(qcvm_profile_timer_t), OP_NUMOPS, fp);
			fread(vm->timers, sizeof(qcvm_profile_timer_t), OP_NUMOPS, fp);
#endif
#ifdef ALLOW_PROFILING
			fread(vm->sample_data, sizeof(qcvm_sampling_t), vm->statements_size, fp);
#endif

			fclose(fp);

			vm->debug_print(qcvm_temp_format(vm, "QCVM: continuing profile of %s\n", vm->profile_name));
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
			field_offset += field->id == TYPE_VECTOR ? 3 : 1;
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
		vm->field_real_size = maxsz(vm->field_real_size, field->global_index + (field->id == TYPE_VECTOR ? 3 : 1));

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
}

void qcvm_check(qcvm_t *vm)
{
	qcvm_setup_fields(vm);
	
	qcvm_init_field_map(vm);
	
	qcvm_field_wrap_list_init(&vm->field_wraps, vm);

	qcvm_check_builtins(vm);
}

#ifdef ALLOW_INSTRUMENTING
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

#if defined(ALLOW_INSTRUMENTING) || defined(ALLOW_PROFILING)
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
#if defined(ALLOW_INSTRUMENTING) || defined(ALLOW_PROFILING)
	if (vm->profile_flags & PROFILE_CONTINUOUS)
	{
		FILE *fp = fopen(qcvm_temp_format(vm, "%s%s.perf", vm->path, vm->profile_name), "wb");

#ifdef ALLOW_INSTRUMENTING
		fwrite(vm->profile_data, sizeof(qcvm_profile_t), vm->functions_size, fp);
		fwrite(vm->opcode_timers, sizeof(vm->opcode_timers), 1, fp);
		fwrite(vm->timers, sizeof(vm->timers), 1, fp);
#endif
#ifdef ALLOW_PROFILING
		fwrite(vm->sample_data, sizeof(qcvm_sampling_t), vm->statements_size, fp);
#endif

		fclose(fp);
	}
#endif

#ifdef ALLOW_PROFILING
	if (vm->profile_flags & PROFILE_SAMPLES)
	{
		for (size_t m = 0; m < TOTAL_MARKS; m++)
		{
			const char *mark = mark_names[m];
			FILE *fp = fopen(qcvm_temp_format(vm, "%sprf_%s_profile.csv", vm->path, mark), "wb");

			fprintf(fp, "ID,Path,Count\n");

			for (size_t i = 0; i < vm->statements_size; i++)
			{
				const qcvm_sampling_t *sample = vm->sample_data + i;

				if (!sample->count[m])
					continue;

				fprintf(fp, "%" PRIuPTR ",%s:%i,%llu\n", i, qcvm_function_for(vm, vm->statements + i), qcvm_line_number_for(vm, vm->statements + i), sample->count[m]);
			}

			fclose(fp);
		}
	}
#endif
	
#ifdef ALLOW_INSTRUMENTING
	for (size_t m = 0; m < TOTAL_MARKS; m++)
	{
		const char *mark = mark_names[m];

		if (vm->profile_flags & (PROFILE_FUNCTIONS | PROFILE_FIELDS))
		{
			FILE *fp = fopen(qcvm_temp_format(vm, "%sprf_%s_profile.csv", vm->path, mark), "wb");
			double all_total = 0;
		
			for (size_t i = 0; i < vm->functions_size; i++)
			{
				const qcvm_profile_t *profile = vm->profile_data + i;

				if (!profile->fields[NumSelfCalls][m] && !profile->self[m] && !profile->ext[m])
					continue;

				const double total = Q_time_adjust(profile->self[m]) / 1000000.0;
				all_total += total;
			}

			fprintf(fp, "ID,Name,Total (ms),Self(ms),Funcs(ms),Total (%%),Self (%%)");
	
			for (size_t i = 0; i < TotalProfileFields; i++)
				fprintf(fp, ",%s", profile_type_names[i]);
	
			fprintf(fp, "\n");

			for (size_t i = 0; i < vm->functions_size; i++)
			{
				const qcvm_profile_t *profile = vm->profile_data + i;

				if (!profile->fields[NumSelfCalls][m] && !profile->self[m] && !profile->ext[m])
					continue;

				const qcvm_function_t *ff = vm->functions + i;
				const char *name = qcvm_get_string(vm, ff->name_index);
		
				const double self = Q_time_adjust(profile->self[m]) / 1000000.0;
				const double ext = Q_time_adjust(profile->ext[m]) / 1000000.0;
				const double total = self + ext;

				fprintf(fp, "%" PRIuPTR ",%s,%f,%f,%f,%f,%f", i, name, total, self, ext, (total / all_total) * 100, (self / all_total) * 100);
		
				for (qcvm_profiler_field_t f = 0; f < TotalProfileFields; f++)
					fprintf(fp, ",%" PRIuPTR "", profile->fields[f][m]);

				fprintf(fp, "\n");
			}

			fclose(fp);
		}
	
		if (vm->profile_flags & PROFILE_TIMERS)
		{
			FILE *fp = fopen(qcvm_temp_format(vm, "%sprf_%s_timers.csv", vm->path, mark), "wb");
			double all_total = 0;

			for (size_t i = 0; i < TotalTimerFields; i++)
			{
				const qcvm_profile_timer_t *timer = &vm->timers[i][m];
				const double total = Q_time_adjust(timer->time[m]) / 1000000.0;
				all_total += total;
			}

			fprintf(fp, "Name,Count,Total (ms),Avg (ns),%%\n");

			for (size_t i = 0; i < TotalTimerFields; i++)
			{
				const qcvm_profile_timer_t *timer = &vm->timers[i][m];
				const double total = Q_time_adjust(timer->time[m]) / 1000000.0;

				fprintf(fp, "%s,%" PRIuPTR ",%f,%f,%f\n", timer_type_names[i], timer->count[m], total, (total / timer->count[m]) * 1000, (total / all_total) * 100);
			}

			fclose(fp);
		}
	
		if (vm->profile_flags & PROFILE_OPCODES)
		{
			FILE *fp = fopen(qcvm_temp_format(vm, "%sprf_%s_opcodes.csv", vm->path, mark), "wb");
			double all_total = 0;

			for (size_t i = 0; i < OP_NUMOPS; i++)
			{
				const qcvm_profile_timer_t *timer = &vm->opcode_timers[i][m];

				if (!timer->count[m])
					continue;

				const double total = Q_time_adjust(timer->time[m]) / 1000000.0;
				all_total += total;
			}

			fprintf(fp, "ID,Count,Total (ms),Avg (ns),%%\n");

			for (size_t i = 0; i < OP_NUMOPS; i++)
			{
				const qcvm_profile_timer_t *timer = &vm->opcode_timers[i][m];

				if (!timer->count[m])
					continue;

				const double total = Q_time_adjust(timer->time[m]) / 1000000.0;

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
}