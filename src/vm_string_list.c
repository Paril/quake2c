#define QCVM_INTERNAL
#include "shared/shared.h"
#include "vm.h"
#include "vm_string.h"

qcvm_string_t qcvm_string_list_store(qcvm_t *vm, const char *str, const size_t len)
{
	qcvm_string_list_t *list = &vm->dynamic_strings;
	int32_t index;

	if (list->free_indices_size)
		index = (-list->free_indices[--list->free_indices_size]) - 1;
	else
	{
		if (list->strings_size == list->strings_allocated)
		{
			list->strings_allocated += REF_STRING_RESERVE;
			qcvm_ref_counted_string_t *old_strings = list->strings;
			list->strings = (qcvm_ref_counted_string_t *)qcvm_alloc(vm, sizeof(qcvm_ref_counted_string_t) * list->strings_allocated);
			if (old_strings)
			{
				memcpy(list->strings, old_strings, sizeof(qcvm_ref_counted_string_t) * list->strings_size);
				qcvm_mem_free(vm, old_strings);
			}
			qcvm_debug(vm, "Increased ref string storage to %u due to \"%s\"\n", list->strings_allocated, str);
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

void qcvm_string_list_unstore(qcvm_t *vm, const qcvm_string_t id)
{
	qcvm_string_list_t *list = &vm->dynamic_strings;
	const int32_t index = (int32_t)(-id) - 1;

	assert(index >= 0 && index < list->strings_size);

	qcvm_ref_counted_string_t *str = &list->strings[index];

	assert(!str->ref_count);

	qcvm_mem_free(vm, (void *)str->str);

	*str = (qcvm_ref_counted_string_t) { NULL, 0, 0 };

	if (list->free_indices_size == list->free_indices_allocated)
	{
		list->free_indices_allocated += FREE_STRING_RESERVE;
		qcvm_string_t *old_free_indices = list->free_indices;
		list->free_indices = (qcvm_string_t *)qcvm_alloc(vm, sizeof(qcvm_string_t) * list->free_indices_allocated);
		if (old_free_indices)
		{
			memcpy(list->free_indices, old_free_indices, sizeof(qcvm_string_t) * list->free_indices_size);
			qcvm_mem_free(vm, old_free_indices);
		}
		qcvm_debug(vm, "Increased free string storage to %u\n", list->free_indices_allocated);
	}

	list->free_indices[list->free_indices_size++] = id;
}

size_t qcvm_string_list_get_length(const qcvm_t *vm, const qcvm_string_t id)
{
	const qcvm_string_list_t *list = &vm->dynamic_strings;
	const int32_t index = (int32_t)(-id) - 1;
	assert(index >= 0 && index < list->strings_size);
	return list->strings[index].length;
}

const char *qcvm_string_list_get(const qcvm_t *vm, const qcvm_string_t id)
{
	const qcvm_string_list_t *list = &vm->dynamic_strings;
	const int32_t index = (int32_t)(-id) - 1;
	assert(index >= 0 && index < list->strings_size);
	assert(list->strings[index].str);
	return list->strings[index].str;
}

void qcvm_string_list_acquire(qcvm_t *vm, const qcvm_string_t id)
{
	qcvm_string_list_t *list = &vm->dynamic_strings;
	START_TIMER(vm, StringAcquire);
	
	const int32_t index = (int32_t)(-id) - 1;

	assert(index >= 0 && index < list->strings_size);

	list->strings[index].ref_count++;

	END_TIMER(vm, PROFILE_TIMERS);
}

void qcvm_string_list_release(qcvm_t *vm, const qcvm_string_t id)
{
	qcvm_string_list_t *list = &vm->dynamic_strings;
	START_TIMER(vm, StringRelease);
	
	const int32_t index = (int32_t)(-id) - 1;

	assert(index >= 0 && index < list->strings_size);

	qcvm_ref_counted_string_t *str = &list->strings[index];

	assert(str->ref_count);
		
	str->ref_count--;

	if (!str->ref_count)
		qcvm_string_list_unstore(vm, id);

	END_TIMER(vm, PROFILE_TIMERS);
}

static void qcvm_string_list_ref_link(qcvm_t *vm, uint32_t hash, const qcvm_string_t id, const void *ptr)
{
	qcvm_string_list_t *list = &vm->dynamic_strings;

	// see if we need to expand the hashes list
	if (!list->ref_storage_free)
	{
		const size_t old_size = list->ref_storage_allocated;
		list->ref_storage_allocated = Q_next_pow2(list->ref_storage_allocated + (REF_STRING_RESERVE * 2));

		qcvm_debug(vm, "Increased ref string pointer storage to %u due to \"%s\"\n", list->ref_storage_allocated, qcvm_get_string(vm, id));

		qcvm_ref_storage_hash_t	*old_ref_storage_data = list->ref_storage_data;
		list->ref_storage_data = (qcvm_ref_storage_hash_t*)qcvm_alloc(vm, sizeof(qcvm_ref_storage_hash_t) * list->ref_storage_allocated);

		list->ref_storage_free = NULL;

		if (old_ref_storage_data)
		{
			memcpy(list->ref_storage_data, old_ref_storage_data, sizeof(qcvm_ref_storage_hash_t) * old_size);
			qcvm_mem_free(vm, old_ref_storage_data);
		}

		if (list->ref_storage_hashes)
			qcvm_mem_free(vm, list->ref_storage_hashes);

		list->ref_storage_hashes = (qcvm_ref_storage_hash_t**)qcvm_alloc(vm, sizeof(qcvm_ref_storage_hash_t*) * list->ref_storage_allocated);

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
	if (vm->state.current >= 0)
		hashed->stack = vm->state.stack[vm->state.current];
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
void qcvm_string_list_dump_refs(FILE *fp, qcvm_t *vm)
{
	qcvm_string_list_t *list = &vm->dynamic_strings;

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

			fprintf(fp, "\t%s\t%s\t%s (%u)\n", qcvm_dump_pointer(vm, (const qcvm_global_t *)hashed->ptr), qcvm_stack_entry(vm, &hashed->stack, false), still_has_string ? "valid" : "invalid", current_id);
		}
	}
}
#endif

static void qcvm_string_list_ref_unlink(qcvm_t *vm, qcvm_ref_storage_hash_t *hashed)
{
	qcvm_string_list_t *list = &vm->dynamic_strings;

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

static inline qcvm_ref_storage_hash_t *qcvm_string_list_get_storage_hash(qcvm_t *vm, const uint32_t hash)
{
	qcvm_string_list_t *list = &vm->dynamic_strings;

	if (!list->ref_storage_stored)
		return NULL;

	return list->ref_storage_hashes[hash];
}

void qcvm_string_list_mark_ref_copy(qcvm_t *vm, const qcvm_string_t id, const void *ptr)
{
	qcvm_string_list_t *list = &vm->dynamic_strings;
	START_TIMER(vm, StringMark);

	uint32_t hash = Q_hash_pointer((uint32_t)ptr, list->ref_storage_allocated);
	qcvm_ref_storage_hash_t *hashed = qcvm_string_list_get_storage_hash(vm, hash);

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
				END_TIMER(vm, PROFILE_TIMERS);
				return;
			}
			
			// we're stomping over another string, so unlink us
			qcvm_string_list_ref_unlink(vm, hashed);
			break;
		}
	}

	// increase ref count
	qcvm_string_list_acquire(vm, id);

	// link!
	qcvm_string_list_ref_link(vm, hash, id, ptr);

	END_TIMER(vm, PROFILE_TIMERS);
}

bool qcvm_string_list_check_ref_unset(qcvm_t *vm, const void *ptr, const size_t span, const bool assume_changed)
{
	qcvm_string_list_t *list = &vm->dynamic_strings;
	START_TIMER(vm, StringCheckUnset);

	bool any_unset = false;

	for (size_t i = 0; i < span; i++)
	{
		const qcvm_global_t *gptr = (const qcvm_global_t *)ptr + i;

		qcvm_ref_storage_hash_t *hashed = qcvm_string_list_get_storage_hash(vm, Q_hash_pointer((uint32_t)gptr, list->ref_storage_allocated));

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
		qcvm_string_list_release(vm, old);

		// unlink
		qcvm_string_list_ref_unlink(vm, hashed);

		any_unset = true;
	}

	END_TIMER(vm, PROFILE_TIMERS);

	return any_unset;
}

qcvm_string_t *qcvm_string_list_has_ref(qcvm_t *vm, const void *ptr, qcvm_ref_storage_hash_t **hashed_ptr)
{
	qcvm_string_list_t *list = &vm->dynamic_strings;
	START_TIMER(vm, StringHasRef);

	qcvm_ref_storage_hash_t *hashed = qcvm_string_list_get_storage_hash(vm, Q_hash_pointer((uint32_t)ptr, list->ref_storage_allocated)), *next;

	for (; hashed; hashed = next)
	{
		next = hashed->hash_next;

		if (hashed->ptr == ptr)
		{
			qcvm_string_t *rv = &hashed->id;

			END_TIMER(vm, PROFILE_TIMERS);

			if (hashed_ptr)
				*hashed_ptr = hashed;

			return rv;
		}
	}
	
	END_TIMER(vm, PROFILE_TIMERS);
	return NULL;
}

void qcvm_string_list_mark_refs_copied(qcvm_t *vm, const void *src, const void *dst, const size_t span)
{
	START_TIMER(vm, StringMarkRefsCopied);

	// grab list of fields that have strings
	for (size_t i = 0; i < span; i++)
	{
		const qcvm_global_t *sptr = (const qcvm_global_t *)src + i;
		qcvm_string_t *sstr = qcvm_string_list_has_ref(vm, sptr, NULL);

		const qcvm_global_t *dptr = (const qcvm_global_t *)dst + i;
		qcvm_ref_storage_hash_t *hashed;
		qcvm_string_t *dstr = qcvm_string_list_has_ref(vm, dptr, &hashed);

		// dst already has a string, check if it's the same ID
		if (dstr)
		{
			// we're copying same string, so just skip
			if (sstr && *sstr == *dstr)
				continue;

			// different strings, unref us
			qcvm_string_list_release(vm, *dstr);
			qcvm_string_list_ref_unlink(vm, hashed);
		}

		// no new string, so keep going
		if (!sstr)
			continue;
		
		// mark them as being inside of src as well now
		qcvm_string_list_mark_ref_copy(vm, *sstr, dptr);
	}

	END_TIMER(vm, PROFILE_TIMERS);
}

bool qcvm_string_list_is_ref_counted(qcvm_t *vm, const qcvm_string_t id)
{
	qcvm_string_list_t *list = &vm->dynamic_strings;
	const int32_t index = (int32_t)(-id) - 1;
	return index < list->strings_size && list->strings[index].str;
}

qcvm_string_backup_t qcvm_string_list_pop_ref(qcvm_t *vm, const void *ptr)
{
	qcvm_string_list_t *list = &vm->dynamic_strings;
	START_TIMER(vm, StringPopRef);

	qcvm_ref_storage_hash_t *hashed = qcvm_string_list_get_storage_hash(vm, Q_hash_pointer((uint32_t)ptr, list->ref_storage_allocated));

	for (; hashed; hashed = hashed->hash_next)
		if (hashed->ptr == ptr)
			break;

	assert(hashed);

	const qcvm_string_t id = hashed->id;

	const qcvm_string_backup_t popped_ref = (qcvm_string_backup_t) { ptr, id };

	qcvm_string_list_ref_unlink(vm, hashed);

	END_TIMER(vm, PROFILE_TIMERS);

	return popped_ref;
}

void qcvm_string_list_push_ref(qcvm_t *vm, const qcvm_string_backup_t *backup)
{
	qcvm_string_list_t *list = &vm->dynamic_strings;
	START_TIMER(vm, StringPushRef);

	const uint32_t hash = Q_hash_pointer((uint32_t)backup->ptr, list->ref_storage_allocated);
	qcvm_ref_storage_hash_t *hashed = qcvm_string_list_get_storage_hash(vm, hash);

	for (; hashed; hashed = hashed->hash_next)
	{
		if (hashed->ptr == backup->ptr)
		{
			// somebody stole our ptr >:(
			if (backup->id == hashed->id)
			{
				// ..oh maybe it was us. no-op!
				END_TIMER(vm, PROFILE_TIMERS);
				return;
			}

			qcvm_string_list_release(vm, hashed->id);
			qcvm_string_list_ref_unlink(vm, hashed);
			break;
		}
	}

	const int32_t index = (int32_t)(-backup->id) - 1;

	// simple restore
	if ((index >= 0 && index < list->strings_size) && list->strings[index].str)
	{
		qcvm_string_list_ref_link(vm, hash, backup->id, backup->ptr);
		END_TIMER(vm, PROFILE_TIMERS);
		return;
	}

	qcvm_error(vm, "unable to push string backup");
}

void qcvm_string_list_write_state(qcvm_t *vm, FILE *fp)
{
	qcvm_string_list_t *list = &vm->dynamic_strings;

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

void qcvm_string_list_read_state(qcvm_t *vm, FILE *fp)
{
	while (true)
	{
		size_t len;

		fread(&len, sizeof(len), 1, fp);

		if (!len)
			break;

		char *s = qcvm_temp_buffer(vm, len);
		fread(s, sizeof(char), len, fp);
		s[len] = 0;

		// does not acquire, since entity/game state does that itself
		qcvm_store_or_find_string(vm, s, len, true);
	}
}

qcvm_string_t qcvm_set_string_ptr(qcvm_t *vm, void *ptr, const char *value, const size_t len, const bool copy)
{
	qcvm_string_t str = qcvm_store_or_find_string(vm, value, len, copy);
	*(qcvm_string_t *)ptr = str;
	qcvm_string_list_check_ref_unset(vm, ptr, sizeof(qcvm_string_t) / sizeof(qcvm_global_t), false);
	qcvm_field_wrap_list_check_set(&vm->field_wraps, ptr, sizeof(qcvm_string_t) / sizeof(qcvm_global_t));

	if (qcvm_string_list_is_ref_counted(vm, str))
		qcvm_string_list_mark_ref_copy(vm, str, ptr);

	return str;
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

	// check dynamic strings.
	// Note that this search is not hashed... yet.
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

	return qcvm_string_list_store(vm, value, len);
}

const char *qcvm_get_string(const qcvm_t *vm, const qcvm_string_t str)
{
	if (str < 0)
		return qcvm_string_list_get(vm, str);
	else if ((size_t)str >= vm->string_size)
		qcvm_error(vm, "bad string");

	return vm->string_data + (size_t)str;
}

size_t qcvm_get_string_length(const qcvm_t *vm, const qcvm_string_t str)
{
	if (str < 0)
		return qcvm_string_list_get_length(vm, str);
	else if ((size_t)str >= vm->string_size)
		qcvm_error(vm, "bad string");

	return vm->string_lengths[(size_t)str];
}