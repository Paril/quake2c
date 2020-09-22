#pragma once

typedef struct qcvm_hash_value_s
{
	qcvm_variant_t				value;

	size_t						index;
	uint32_t					hash_value;
	struct qcvm_hash_value_s	*hash_next;
} qcvm_hash_value_t;

typedef struct
{
	qcvm_hash_value_t	*values;
	size_t				size, allocated;
	qcvm_hash_value_t	**hashed;
	qcvm_hash_value_t	*free;
	qcvm_hash_value_t	**indexed;
} qcvm_hashset_t;

inline bool qcvm_variant_equals(const qcvm_variant_t a, const qcvm_variant_t b)
{
	if (a.type != b.type)
		return false;
	if (a.type == TYPE_VECTOR)
		return VectorEquals(a.value.vec, b.value.vec);
	return a.value.itg == b.value.itg;
}

inline uint32_t Q_hash_variant(const qcvm_variant_t variant, const size_t size)
{
	uint32_t hash;

	if (variant.type == TYPE_VECTOR)
		hash = (int32_t)(variant.value.vec.x * 73856093) ^ (int32_t)(variant.value.vec.y * 19349663) ^ (int32_t)(variant.value.vec.z * 83492791);
	else
		hash = (variant.value.itg ^ variant.type);

	return hash % size;
}

#define HASH_RESERVE 64

extern const qcvm_handle_descriptor_t hashset_descriptor;

qcvm_hashset_t *hashset_alloc(const qcvm_t *vm, const size_t reserve);
void hashset_free(const qcvm_t *vm, qcvm_hashset_t *set);
bool hashset_add(qcvm_t *vm, qcvm_hashset_t *set, const qcvm_variant_t variant);
bool hashset_remove(qcvm_t *vm, qcvm_hashset_t *set, const qcvm_variant_t variant);
bool hashset_contains(qcvm_t *vm, qcvm_hashset_t *set, const qcvm_variant_t variant);
void hashset_clear(qcvm_t *vm, qcvm_hashset_t *set);

void qcvm_init_hash_builtins(qcvm_t *vm);