#define QCVM_INTERNAL
#include "shared/shared.h"
#include "game.h"
#include "g_vm.h"
#include "vm_math.h"

static void F_OP_DONE(qcvm_t *vm, const qcvm_operands_t operands, int *depth)
{
	qcvm_leave(vm);
	(*depth)--;
}

static void F_OP_RETURN(qcvm_t *vm, const qcvm_operands_t operands, int *depth)
{
	if (operands.a != GLOBAL_NULL)
		qcvm_copy_globals_typed(qcvm_global_t[3], vm, GLOBAL_RETURN, operands.a);

	qcvm_leave(vm);
	(*depth)--;
}

#define F_OP_MUL(F_OP, TLeft, TRight, TResult) \
static void F_OP(qcvm_t *vm, const qcvm_operands_t operands, int *depth) \
{ \
	const TLeft a = *qcvm_get_global_typed(TLeft, vm, operands.a); \
	const TRight b = *qcvm_get_global_typed(TRight, vm, operands.b); \
	const TResult result = a * b; \
	qcvm_set_global_typed_value(TResult, vm, operands.c, result); \
}

F_OP_MUL(F_OP_MUL_F, vec_t, vec_t, vec_t)
F_OP_MUL(F_OP_MUL_I, int32_t, int32_t, int32_t)
F_OP_MUL(F_OP_MUL_IF, int32_t, vec_t, vec_t)
F_OP_MUL(F_OP_MUL_FI, vec_t, int32_t, vec_t)
#undef F_OP_MUL

static void F_OP_MUL_V(qcvm_t *vm, const qcvm_operands_t operands, int *depth)
{
	const vec3_t a = *qcvm_get_global_typed(vec3_t, vm, operands.a);
	const vec3_t b = *qcvm_get_global_typed(vec3_t, vm, operands.b);
	const vec_t result = DotProduct(a, b);
	qcvm_set_global_typed_value(vec_t, vm, operands.c, result);
}

static void F_OP_MUL_VF(qcvm_t *vm, const qcvm_operands_t operands, int *depth)
{
	const vec3_t a = *qcvm_get_global_typed(vec3_t, vm, operands.a);
	const vec_t b = *qcvm_get_global_typed(vec_t, vm, operands.b);
	const vec3_t result = VectorScaleF(a, b);
	qcvm_set_global_typed_value(vec3_t, vm, operands.c, result);
}

static void F_OP_MUL_FV(qcvm_t *vm, const qcvm_operands_t operands, int *depth)
{
	const vec_t a = *qcvm_get_global_typed(vec_t, vm, operands.a);
	const vec3_t b = *qcvm_get_global_typed(vec3_t, vm, operands.b);
	const vec3_t result = VectorScaleF(b, a);
	qcvm_set_global_typed_value(vec3_t, vm, operands.c, result);
}

static void F_OP_MUL_VI(qcvm_t *vm, const qcvm_operands_t operands, int *depth)
{
	const vec3_t a = *qcvm_get_global_typed(vec3_t, vm, operands.a);
	const int32_t b = *qcvm_get_global_typed(int32_t, vm, operands.b);
	const vec3_t result = VectorScaleI(a, b);
	qcvm_set_global_typed_value(vec3_t, vm, operands.c, result);
}

static void F_OP_MUL_IV(qcvm_t *vm, const qcvm_operands_t operands, int *depth)
{
	const int32_t a = *qcvm_get_global_typed(int32_t, vm, operands.a);
	const vec3_t b = *qcvm_get_global_typed(vec3_t, vm, operands.b);
	const vec3_t result = VectorScaleI(b, a);
	qcvm_set_global_typed_value(vec3_t, vm, operands.c, result);
}

#define F_OP_DIV(F_OP, TLeft, TRight, TResult) \
static void F_OP(qcvm_t *vm, const qcvm_operands_t operands, int *depth) \
{ \
	const TLeft a = *qcvm_get_global_typed(TLeft, vm, operands.a); \
	const TRight b = *qcvm_get_global_typed(TRight, vm, operands.b); \
	const TResult result = a / b; \
	qcvm_set_global_typed_value(TResult, vm, operands.c, result); \
}

F_OP_DIV(F_OP_DIV_F, vec_t, vec_t, vec_t)
F_OP_DIV(F_OP_DIV_I, int32_t, int32_t, int32_t)
F_OP_DIV(F_OP_DIV_IF, int32_t, vec_t, vec_t)
F_OP_DIV(F_OP_DIV_FI, vec_t, int32_t, vec_t)
#undef F_OP_DIV

static void F_OP_DIV_VF(qcvm_t *vm, const qcvm_operands_t operands, int *depth)
{
	const vec3_t a = *qcvm_get_global_typed(vec3_t, vm, operands.a);
	const vec_t b = *qcvm_get_global_typed(vec_t, vm, operands.b);
	const vec3_t result = VectorDivideF(a, b);
	qcvm_set_global_typed_value(vec3_t, vm, operands.c, result);
}

#define F_OP_ADD(F_OP, TLeft, TRight, TResult) \
static void F_OP(qcvm_t *vm, const qcvm_operands_t operands, int *depth) \
{ \
	const TLeft a = *qcvm_get_global_typed(TLeft, vm, operands.a); \
	const TRight b = *qcvm_get_global_typed(TRight, vm, operands.b); \
	const TResult result = a + b; \
	qcvm_set_global_typed_value(TResult, vm, operands.c, result); \
}

F_OP_ADD(F_OP_ADD_F, vec_t, vec_t, vec_t)
F_OP_ADD(F_OP_ADD_I, int32_t, int32_t, int32_t)
F_OP_ADD(F_OP_ADD_FI, vec_t, int32_t, vec_t)
F_OP_ADD(F_OP_ADD_IF, int32_t, vec_t, vec_t)
#undef F_OP_ADD

static void F_OP_ADD_V(qcvm_t *vm, const qcvm_operands_t operands, int *depth)
{
	const vec3_t a = *qcvm_get_global_typed(vec3_t, vm, operands.a);
	const vec3_t b = *qcvm_get_global_typed(vec3_t, vm, operands.b);
	const vec3_t result = VectorAdd(a, b);
	qcvm_set_global_typed_value(vec3_t, vm, operands.c, result);
}

#define F_OP_SUB(F_OP, TLeft, TRight, TResult) \
static void F_OP(qcvm_t *vm, const qcvm_operands_t operands, int *depth) \
{ \
	const TLeft a = *qcvm_get_global_typed(TLeft, vm, operands.a); \
	const TRight b = *qcvm_get_global_typed(TRight, vm, operands.b); \
	const TResult result = a - b; \
	qcvm_set_global_typed_value(TResult, vm, operands.c, result); \
}

F_OP_SUB(F_OP_SUB_F, vec_t, vec_t, vec_t)
F_OP_SUB(F_OP_SUB_I, int32_t, int32_t, int32_t)
F_OP_SUB(F_OP_SUB_FI, vec_t, int32_t, vec_t)
F_OP_SUB(F_OP_SUB_IF, int32_t, vec_t, vec_t)
#undef F_OP_SUB

static void F_OP_SUB_V(qcvm_t *vm, const qcvm_operands_t operands, int *depth)
{
	const vec3_t a = *qcvm_get_global_typed(vec3_t, vm, operands.a);
	const vec3_t b = *qcvm_get_global_typed(vec3_t, vm, operands.b);
	const vec3_t result = VectorSubtract(a, b);
	qcvm_set_global_typed_value(vec3_t, vm, operands.c, result);
}

#define F_OP_EQ(F_OP, TLeft, TRight, TResult) \
static void F_OP(qcvm_t *vm, const qcvm_operands_t operands, int *depth) \
{ \
	const TLeft a = *qcvm_get_global_typed(TLeft, vm, operands.a); \
	const TRight b = *qcvm_get_global_typed(TRight, vm, operands.b); \
	const TResult result = a == b; \
	qcvm_set_global_typed_value(TResult, vm, operands.c, result); \
}

F_OP_EQ(F_OP_EQ_F, vec_t, vec_t, vec_t)
F_OP_EQ(F_OP_EQ_E, qcvm_ent_t, qcvm_ent_t, vec_t)
F_OP_EQ(F_OP_EQ_FNC, qcvm_func_t, qcvm_func_t, vec_t)
F_OP_EQ(F_OP_EQ_I, int32_t, int32_t, int32_t)
F_OP_EQ(F_OP_EQ_IF, int32_t, vec_t, int32_t)
F_OP_EQ(F_OP_EQ_FI, vec_t, int32_t, int32_t)
#undef F_OP_EQ

static void F_OP_EQ_V(qcvm_t *vm, const qcvm_operands_t operands, int *depth)
{
	const vec3_t a = *qcvm_get_global_typed(vec3_t, vm, operands.a);
	const vec3_t b = *qcvm_get_global_typed(vec3_t, vm, operands.b);
	const vec_t result = VectorEquals(a, b);
	qcvm_set_global_typed_value(vec_t, vm, operands.c, result);
}

static void F_OP_EQ_S(qcvm_t *vm, const qcvm_operands_t operands, int *depth)
{
	const qcvm_string_t a = *qcvm_get_global_typed(qcvm_string_t, vm, operands.a);
	const qcvm_string_t b = *qcvm_get_global_typed(qcvm_string_t, vm, operands.b);
	vec_t result;

	if (a == b)
		result = 1;
	else
		result = !strcmp(qcvm_get_string(vm, a), qcvm_get_string(vm, b));
	
	qcvm_set_global_typed_value(vec_t, vm, operands.c, result);
}

#define F_OP_NE(F_OP, TLeft, TRight, TResult) \
static void F_OP(qcvm_t *vm, const qcvm_operands_t operands, int *depth) \
{ \
	const TLeft a = *qcvm_get_global_typed(TLeft, vm, operands.a); \
	const TRight b = *qcvm_get_global_typed(TRight, vm, operands.b); \
	const TResult result = a != b; \
	qcvm_set_global_typed_value(TResult, vm, operands.c, result); \
}

F_OP_NE(F_OP_NE_F, vec_t, vec_t, vec_t)
F_OP_NE(F_OP_NE_E, qcvm_ent_t, qcvm_ent_t, vec_t)
F_OP_NE(F_OP_NE_FNC, qcvm_func_t, qcvm_func_t, vec_t)
F_OP_NE(F_OP_NE_I, int32_t, int32_t, int32_t)
F_OP_NE(F_OP_NE_IF, int32_t, vec_t, int32_t)
F_OP_NE(F_OP_NE_FI, vec_t, int32_t, int32_t)
#undef F_OP_NE

static void F_OP_NE_V(qcvm_t *vm, const qcvm_operands_t operands, int *depth)
{
	const vec3_t a = *qcvm_get_global_typed(vec3_t, vm, operands.a);
	const vec3_t b = *qcvm_get_global_typed(vec3_t, vm, operands.b);
	const vec_t result = !VectorEquals(a, b);
	qcvm_set_global_typed_value(vec_t, vm, operands.c, result);
}

static void F_OP_NE_S(qcvm_t *vm, const qcvm_operands_t operands, int *depth)
{
	const qcvm_string_t a = *qcvm_get_global_typed(qcvm_string_t, vm, operands.a);
	const qcvm_string_t b = *qcvm_get_global_typed(qcvm_string_t, vm, operands.b);
	vec_t result;

	if (a == b)
		result = 0;
	else
		result = !!strcmp(qcvm_get_string(vm, a), qcvm_get_string(vm, b));

	qcvm_set_global_typed_value(vec_t, vm, operands.c, result);
}

#define F_OP_LE(F_OP, TLeft, TRight, TResult) \
static void F_OP(qcvm_t *vm, const qcvm_operands_t operands, int *depth) \
{ \
	const TLeft a = *qcvm_get_global_typed(TLeft, vm, operands.a); \
	const TRight b = *qcvm_get_global_typed(TRight, vm, operands.b); \
	const TResult result = a <= b; \
	qcvm_set_global_typed_value(TResult, vm, operands.c, result); \
}

F_OP_LE(F_OP_LE_F, vec_t, vec_t, vec_t)
F_OP_LE(F_OP_LE_I, int32_t, int32_t, int32_t)
F_OP_LE(F_OP_LE_IF, int32_t, vec_t, int32_t)
F_OP_LE(F_OP_LE_FI, vec_t, int32_t, int32_t)
#undef F_OP_LE

#define F_OP_GE(F_OP, TLeft, TRight, TResult) \
static void F_OP(qcvm_t *vm, const qcvm_operands_t operands, int *depth) \
{ \
	const TLeft a = *qcvm_get_global_typed(TLeft, vm, operands.a); \
	const TRight b = *qcvm_get_global_typed(TRight, vm, operands.b); \
	const TResult result = a >= b; \
	qcvm_set_global_typed_value(TResult, vm, operands.c, result); \
}

F_OP_GE(F_OP_GE_F, vec_t, vec_t, vec_t)
F_OP_GE(F_OP_GE_I, int32_t, int32_t, int32_t)
F_OP_GE(F_OP_GE_IF, int32_t, vec_t, int32_t)
F_OP_GE(F_OP_GE_FI, vec_t, int32_t, int32_t)
#undef F_OP_GE

#define F_OP_LT(F_OP, TLeft, TRight, TResult) \
static void F_OP(qcvm_t *vm, const qcvm_operands_t operands, int *depth) \
{ \
	const TLeft a = *qcvm_get_global_typed(TLeft, vm, operands.a); \
	const TRight b = *qcvm_get_global_typed(TRight, vm, operands.b); \
	const TResult result = a < b; \
	qcvm_set_global_typed_value(TResult, vm, operands.c, result); \
}

F_OP_LT(F_OP_LT_F, vec_t, vec_t, vec_t)
F_OP_LT(F_OP_LT_I, int32_t, int32_t, int32_t)
F_OP_LT(F_OP_LT_IF, int32_t, vec_t, int32_t)
F_OP_LT(F_OP_LT_FI, vec_t, int32_t, int32_t)
#undef F_OP_LT

#define F_OP_GT(F_OP, TLeft, TRight, TResult) \
static void F_OP(qcvm_t *vm, const qcvm_operands_t operands, int *depth) \
{ \
	const TLeft a = *qcvm_get_global_typed(TLeft, vm, operands.a); \
	const TRight b = *qcvm_get_global_typed(TRight, vm, operands.b); \
	const TResult result = a > b; \
	qcvm_set_global_typed_value(TResult, vm, operands.c, result); \
}

F_OP_GT(F_OP_GT_F, vec_t, vec_t, vec_t)
F_OP_GT(F_OP_GT_I, int32_t, int32_t, int32_t)
F_OP_GT(F_OP_GT_IF, int32_t, vec_t, int32_t)
F_OP_GT(F_OP_GT_FI, vec_t, int32_t, int32_t)
#undef F_OP_GT

#define F_OP_LOAD(F_OP, TType) \
static void F_OP(qcvm_t *vm, const qcvm_operands_t operands, int *depth) \
{ \
	edict_t *ent = qcvm_ent_to_entity(*qcvm_get_global_typed(qcvm_ent_t, vm, operands.a), true); \
	int32_t field_offset = *qcvm_get_global_typed(int32_t, vm, operands.b); \
	TType *field_value = (TType *)((int32_t*)ent + field_offset); \
	qcvm_set_global_typed_ptr(TType, vm, operands.c, field_value); \
	qcvm_string_list_mark_if_has_ref(&vm->dynamic_strings, field_value, qcvm_get_global(vm, operands.c), sizeof(TType) / sizeof(qcvm_global_t)); \
}

F_OP_LOAD(F_OP_LOAD_F, vec_t)
F_OP_LOAD(F_OP_LOAD_V, vec3_t)
F_OP_LOAD(F_OP_LOAD_S, qcvm_string_t)
F_OP_LOAD(F_OP_LOAD_ENT, qcvm_ent_t)
F_OP_LOAD(F_OP_LOAD_FLD, int32_t)
F_OP_LOAD(F_OP_LOAD_FNC, qcvm_func_t)
F_OP_LOAD(F_OP_LOAD_I, int32_t)
F_OP_LOAD(F_OP_LOAD_P, int32_t)
#undef F_OP_LOAD

static void F_OP_ADDRESS(qcvm_t *vm, const qcvm_operands_t operands, int *depth)
{
	edict_t *ent = qcvm_ent_to_entity(*qcvm_get_global_typed(qcvm_ent_t, vm, operands.a), true);
	const int32_t field = *qcvm_get_global_typed(int32_t, vm, operands.b);
	const int32_t address = qcvm_entity_field_address(ent, field);
	qcvm_set_global_typed_value(int32_t, vm, operands.c, address);
}

#define F_OP_STORE_SAME(F_OP, TType) \
static void F_OP(qcvm_t *vm, const qcvm_operands_t operands, int *depth) \
{ \
	qcvm_copy_globals_typed(TType, vm, operands.b, operands.a); \
}

#define F_OP_STORE_DIFF(F_OP, TType, TResult) \
static void F_OP(qcvm_t *vm, const qcvm_operands_t operands, int *depth) \
{ \
	qcvm_copy_globals_safe(TResult, TType, vm, operands.b, operands.a); \
}

F_OP_STORE_SAME(F_OP_STORE_F, vec_t)
F_OP_STORE_SAME(F_OP_STORE_V, vec3_t)
F_OP_STORE_SAME(F_OP_STORE_S, qcvm_string_t)
F_OP_STORE_SAME(F_OP_STORE_ENT, qcvm_ent_t)
F_OP_STORE_SAME(F_OP_STORE_FLD, int32_t)
F_OP_STORE_SAME(F_OP_STORE_FNC, qcvm_func_t)
F_OP_STORE_SAME(F_OP_STORE_I, int32_t)
F_OP_STORE_DIFF(F_OP_STORE_IF, int32_t, vec_t)
F_OP_STORE_DIFF(F_OP_STORE_FI, vec_t, int32_t)
F_OP_STORE_SAME(F_OP_STORE_P, int32_t)
#undef F_OP_STORE

#define F_OP_STOREP(F_OP, TType, TResult) \
static void F_OP(qcvm_t *vm, const qcvm_operands_t operands, int *depth) \
{ \
	size_t address = *qcvm_get_global_typed(int32_t, vm, operands.b) + (*qcvm_get_global_typed(int32_t, vm, operands.c) * sizeof(qcvm_global_t)); \
	const TType *value = qcvm_get_global_typed(TType, vm, operands.a); \
\
	if (!qcvm_pointer_valid(vm, address, false, sizeof(TResult))) \
		qcvm_error(vm, "invalid address"); \
\
	const size_t span = sizeof(TType) / sizeof(qcvm_global_t); \
\
	TResult *address_ptr = (TResult *)address; \
	*address_ptr = *value; \
	qcvm_string_list_check_ref_unset(&vm->dynamic_strings, address_ptr, span, false); \
\
	qcvm_string_list_mark_if_has_ref(&vm->dynamic_strings, &value, address_ptr, span); \
}

F_OP_STOREP(F_OP_STOREP_F, vec_t, vec_t)
F_OP_STOREP(F_OP_STOREP_V, vec3_t, vec3_t)
F_OP_STOREP(F_OP_STOREP_S, qcvm_string_t, qcvm_string_t)
F_OP_STOREP(F_OP_STOREP_ENT, qcvm_ent_t, qcvm_ent_t)
F_OP_STOREP(F_OP_STOREP_FLD, int32_t, int32_t)
F_OP_STOREP(F_OP_STOREP_FNC, qcvm_func_t, qcvm_func_t)
F_OP_STOREP(F_OP_STOREP_I, int32_t, int32_t)
F_OP_STOREP(F_OP_STOREP_IF, int32_t, vec_t)
F_OP_STOREP(F_OP_STOREP_FI, vec_t, int32_t)
#undef F_OP_STOREP

#define F_OP_NOT(F_OP, TType, TResult) \
static void F_OP(qcvm_t *vm, const qcvm_operands_t operands, int *depth) \
{ \
	const TType a = *qcvm_get_global_typed(TType, vm, operands.a); \
	const TResult result = !a; \
	qcvm_set_global_typed_value(TResult, vm, operands.c, result); \
}

F_OP_NOT(F_OP_NOT_F, vec_t, vec_t)
F_OP_NOT(F_OP_NOT_FNC, qcvm_func_t, vec_t)
F_OP_NOT(F_OP_NOT_ENT, qcvm_ent_t, vec_t)
F_OP_NOT(F_OP_NOT_I, int32_t, int32_t)
#undef F_OP_NOT

static void F_OP_NOT_V(qcvm_t *vm, const qcvm_operands_t operands, int *depth)
{
	const vec3_t a = *qcvm_get_global_typed(vec3_t, vm, operands.a);
	const vec_t result = VectorEmpty(a);
	qcvm_set_global_typed_value(vec_t, vm, operands.c, result);
}

static void F_OP_NOT_S(qcvm_t *vm, const qcvm_operands_t operands, int *depth)
{
	const qcvm_string_t a = *qcvm_get_global_typed(qcvm_string_t, vm, operands.a);
	const vec_t result = a == STRING_EMPTY || !*qcvm_get_string(vm, a);
	qcvm_set_global_typed_value(vec_t, vm, operands.c, result);
}

#ifdef ALLOW_PROFILING
#define PROFILE_COND_JUMP \
	if (vm->profile_flags & PROFILE_FIELDS) \
		current->profile->fields[NumConditionalJumps]++
#else
#define PROFILE_COND_JUMP
#endif

#define F_OP_IF(F_OP, TType) \
static void F_OP(qcvm_t *vm, const qcvm_operands_t operands, int *depth) \
{ \
	if (*qcvm_get_global_typed(TType, vm, operands.a)) \
	{ \
		qcvm_stack_t *current = &vm->state.stack[vm->state.current]; \
		current->statement += (int16_t)current->statement->args.b - 1; \
		PROFILE_COND_JUMP; \
	} \
}

F_OP_IF(F_OP_IF_I, vec_t)
F_OP_IF(F_OP_IF_F, int32_t)
#undef F_OP_IF

static void F_OP_IF_S(qcvm_t *vm, const qcvm_operands_t operands, int *depth)
{
	const qcvm_string_t s = *qcvm_get_global_typed(qcvm_string_t, vm, operands.a);

	if (s != STRING_EMPTY && *qcvm_get_string(vm, s))
	{
		qcvm_stack_t *current = &vm->state.stack[vm->state.current];
		current->statement += (int16_t)current->statement->args.b - 1;
		PROFILE_COND_JUMP;
	}
}

#define F_OP_IFNOT(F_OP, TType) \
static void F_OP(qcvm_t *vm, const qcvm_operands_t operands, int *depth) \
{ \
	if (!*qcvm_get_global_typed(TType, vm, operands.a)) \
	{ \
		qcvm_stack_t *current = &vm->state.stack[vm->state.current]; \
		current->statement += (int16_t)current->statement->args.b - 1; \
		PROFILE_COND_JUMP; \
	} \
}

F_OP_IFNOT(F_OP_IFNOT_I, vec_t)
F_OP_IFNOT(F_OP_IFNOT_F, int32_t)
#undef F_IFNOT

static void F_OP_IFNOT_S(qcvm_t *vm, const qcvm_operands_t operands, int *depth)
{
	const qcvm_string_t s = *qcvm_get_global_typed(qcvm_string_t, vm, operands.a);

	if (s == STRING_EMPTY || !*qcvm_get_string(vm, s))
	{
		qcvm_stack_t *current = &vm->state.stack[vm->state.current];
		current->statement += (int16_t)current->statement->args.b - 1;
		PROFILE_COND_JUMP;
	}
}

#ifdef ALLOW_PROFILING
#define PROFILE_FUNCTION_CALL \
	if (vm->profile_flags & PROFILE_FIELDS) \
	{ \
		qcvm_stack_t *current = &vm->state.stack[vm->state.current]; \
		current->profile->fields[NumFuncCalls]++; \
	}
#else
#define PROFILE_FUNCTION_CALL
#endif

#define F_OP_CALL(F_OP, num_args, hexen) \
static void F_OP(qcvm_t *vm, const qcvm_operands_t operands, int *depth) \
{ \
	if (num_args >= 2 && hexen) \
		qcvm_copy_globals_typed(qcvm_global_t[3], vm, GLOBAL_PARM1, operands.c); \
\
	if (num_args >= 1 && hexen) \
		qcvm_copy_globals_typed(qcvm_global_t[3], vm, GLOBAL_PARM0, operands.b); \
\
	const int32_t enter_func = *qcvm_get_global_typed(int32_t, vm, operands.a); \
\
	vm->state.argc = num_args; \
	if (enter_func <= 0 || enter_func >= vm->functions_size) \
		qcvm_error(vm, "NULL function"); \
\
	PROFILE_FUNCTION_CALL; \
\
	qcvm_function_t *call = &vm->functions[enter_func]; \
\
	if (!call->id) \
		qcvm_error(vm, "Tried to call missing function %s", qcvm_get_string(vm, call->name_index)); \
\
	if (call->id < 0) /* negative statements are built in functions */ \
	{ \
		qcvm_call_builtin(vm, call); \
		return; \
	} \
\
	(*depth)++; \
	qcvm_enter(vm, call); \
}

F_OP_CALL(F_OP_CALL0, 0, false)
F_OP_CALL(F_OP_CALL1, 1, false)
F_OP_CALL(F_OP_CALL2, 2, false)
F_OP_CALL(F_OP_CALL3, 3, false)
F_OP_CALL(F_OP_CALL4, 4, false)
F_OP_CALL(F_OP_CALL5, 5, false)
F_OP_CALL(F_OP_CALL6, 6, false)
F_OP_CALL(F_OP_CALL7, 7, false)
F_OP_CALL(F_OP_CALL8, 8, false)

F_OP_CALL(F_OP_CALL1H, 1, true)
F_OP_CALL(F_OP_CALL2H, 2, true)
F_OP_CALL(F_OP_CALL3H, 3, true)
F_OP_CALL(F_OP_CALL4H, 4, true)
F_OP_CALL(F_OP_CALL5H, 5, true)
F_OP_CALL(F_OP_CALL6H, 6, true)
F_OP_CALL(F_OP_CALL7H, 7, true)
F_OP_CALL(F_OP_CALL8H, 8, true)
#undef F_OP_CALL

static void F_OP_GOTO(qcvm_t *vm, const qcvm_operands_t operands, int *depth)
{
	qcvm_stack_t *current = &vm->state.stack[vm->state.current];
	current->statement += (int16_t)current->statement->args.a - 1;

#ifdef ALLOW_PROFILING
	if (vm->profile_flags & PROFILE_FIELDS)
		current->profile->fields[NumUnconditionalJumps]++;
#endif
}

#define F_OP_AND(F_OP, TLeft, TRight, TResult) \
static void F_OP(qcvm_t *vm, const qcvm_operands_t operands, int *depth) \
{ \
	const TLeft a = *qcvm_get_global_typed(TLeft, vm, operands.a); \
	const TRight b = *qcvm_get_global_typed(TRight, vm, operands.b); \
	const TResult result = a && b; \
	qcvm_set_global_typed_value(TResult, vm, operands.c, result); \
}

F_OP_AND(F_OP_AND_F, vec_t, vec_t, vec_t)
F_OP_AND(F_OP_AND_I, int32_t, int32_t, int32_t)
F_OP_AND(F_OP_AND_IF, int32_t, vec_t, int32_t)
F_OP_AND(F_OP_AND_FI, vec_t, int32_t, int32_t)
#undef F_OP_AND

#define F_OP_OR(F_OP, TLeft, TRight, TResult) \
static void F_OP(qcvm_t *vm, const qcvm_operands_t operands, int *depth) \
{ \
	const TLeft a = *qcvm_get_global_typed(TLeft, vm, operands.a); \
	const TRight b = *qcvm_get_global_typed(TRight, vm, operands.b); \
	const TResult result = a || b; \
	qcvm_set_global_typed_value(TResult, vm, operands.c, result); \
}

F_OP_OR(F_OP_OR_F, vec_t, vec_t, vec_t)
F_OP_OR(F_OP_OR_I, int32_t, int32_t, int32_t)
F_OP_OR(F_OP_OR_IF, int32_t, vec_t, int32_t)
F_OP_OR(F_OP_OR_FI, vec_t, int32_t, int32_t)
#undef F_OP_OR

#define F_OP_BITAND(F_OP, TLeft, TRight, TResult) \
static void F_OP(qcvm_t *vm, const qcvm_operands_t operands, int *depth) \
{ \
	const int32_t a = (int32_t)*qcvm_get_global_typed(TLeft, vm, operands.a); \
	const int32_t b = (int32_t)*qcvm_get_global_typed(TRight, vm, operands.b); \
	const TResult result = a & b; \
	qcvm_set_global_typed_value(TResult, vm, operands.c, result); \
}

F_OP_BITAND(F_OP_BITAND_F, vec_t, vec_t, vec_t)
F_OP_BITAND(F_OP_BITAND_I, int32_t, int32_t, int32_t)
F_OP_BITAND(F_OP_BITAND_IF, int32_t, vec_t, int32_t)
F_OP_BITAND(F_OP_BITAND_FI, vec_t, int32_t, int32_t)
#undef F_OP_BITAND

#define F_OP_BITOR(F_OP, TLeft, TRight, TResult) \
static void F_OP(qcvm_t *vm, const qcvm_operands_t operands, int *depth) \
{ \
	const int32_t a = (int32_t)*qcvm_get_global_typed(TLeft, vm, operands.a); \
	const int32_t b = (int32_t)*qcvm_get_global_typed(TRight, vm, operands.b); \
	const TResult result = a | b; \
	qcvm_set_global_typed_value(TResult, vm, operands.c, result); \
}

F_OP_BITOR(F_OP_BITOR_F, vec_t, vec_t, vec_t)
F_OP_BITOR(F_OP_BITOR_I, int32_t, int32_t, int32_t)
F_OP_BITOR(F_OP_BITOR_IF, int32_t, vec_t, int32_t)
F_OP_BITOR(F_OP_BITOR_FI, vec_t, int32_t, int32_t)
#undef F_OP_BITOR

static void F_OP_CONV_ITOF(qcvm_t *vm, const qcvm_operands_t operands, int *depth)
{
	const vec_t result = (vec_t)*qcvm_get_global_typed(int32_t, vm, operands.a);
	qcvm_set_global_typed_value(vec_t, vm, operands.c, result);
}

static void F_OP_CONV_FTOI(qcvm_t *vm, const qcvm_operands_t operands, int *depth)
{
	const int32_t result = (int32_t)*qcvm_get_global_typed(vec_t, vm, operands.a);
	qcvm_set_global_typed_value(int32_t, vm, operands.c, result);
}

static void F_OP_CP_ITOF(qcvm_t *vm, const qcvm_operands_t operands, int *depth)
{
	const size_t address = *qcvm_get_global_typed(int32_t, vm, operands.a);

	if (!qcvm_pointer_valid(vm, address, false, sizeof(int32_t)))
		qcvm_error(vm, "invalid address");

	const vec_t result = (vec_t)(*(int32_t *)address);
	qcvm_set_global_typed_value(vec_t, vm, operands.c, result);
}

static void F_OP_CP_FTOI(qcvm_t *vm, const qcvm_operands_t operands, int *depth)
{
	const size_t address = *qcvm_get_global_typed(int32_t, vm, operands.a);
	
	if (!qcvm_pointer_valid(vm, address, false, sizeof(vec_t)))
		qcvm_error(vm, "invalid address");
	
	const int32_t result = (int32_t)(*(vec_t *)address);
	qcvm_set_global_typed_value(int32_t, vm, operands.c, result);
}

static void F_OP_BITXOR_I(qcvm_t *vm, const qcvm_operands_t operands, int *depth)
{
	const int32_t a = *qcvm_get_global_typed(int32_t, vm, operands.a);
	const int32_t b = *qcvm_get_global_typed(int32_t, vm, operands.b);
	const int32_t result = a ^ b;
	qcvm_set_global_typed_value(int32_t, vm, operands.c, result);
}

static void F_OP_RSHIFT_I(qcvm_t *vm, const qcvm_operands_t operands, int *depth)
{
	const int32_t a = *qcvm_get_global_typed(int32_t, vm, operands.a);
	const int32_t b = *qcvm_get_global_typed(int32_t, vm, operands.b);
	const int32_t result = a >> b;
	qcvm_set_global_typed_value(int32_t, vm, operands.c, result);
}

static void F_OP_LSHIFT_I(qcvm_t *vm, const qcvm_operands_t operands, int *depth)
{
	const int32_t a = *qcvm_get_global_typed(int32_t, vm, operands.a);
	const int32_t b = *qcvm_get_global_typed(int32_t, vm, operands.b);
	const int32_t result = a << b;
	qcvm_set_global_typed_value(int32_t, vm, operands.c, result);
}

static void F_OP_GLOBALADDRESS(qcvm_t *vm, const qcvm_operands_t operands, int *depth)
{
	const qcvm_global_t *base = qcvm_get_global(vm, operands.a);
	const ptrdiff_t offset = *qcvm_get_global_typed(int32_t, vm, operands.b);
	const int32_t address = (int32_t)(base + offset);

	if (!qcvm_pointer_valid(vm, address, false, sizeof(qcvm_global_t)))
		qcvm_error(vm, "bad pointer");

	qcvm_set_global_typed_value(int32_t, vm, operands.c, address);
}

static void F_OP_ADD_PIW(qcvm_t *vm, const qcvm_operands_t operands, int *depth)
{
	const int32_t a = *qcvm_get_global_typed(int32_t, vm, operands.a);
	const int32_t b = *qcvm_get_global_typed(int32_t, vm, operands.b);
	const int32_t result = (int32_t)(a + (b * sizeof(float)));
	qcvm_set_global_typed_value(int32_t, vm, operands.c, result);
}

#define F_OP_LOADA(F_OP, TType) \
static void F_OP(qcvm_t *vm, const qcvm_operands_t operands, int *depth) \
{ \
	const ptrdiff_t address = (ptrdiff_t)operands.a + *qcvm_get_global_typed(int32_t, vm, operands.b); \
\
	if (!qcvm_pointer_valid(vm, (ptrdiff_t)(vm->global_data + address), false, sizeof(TType))) \
		qcvm_error(vm, "Invalid pointer %x", address); \
\
	TType *field_value = (TType *)(vm->global_data + address); \
	qcvm_set_global_typed_ptr(TType, vm, operands.c, field_value); \
\
	const size_t span = sizeof(TType) / sizeof(qcvm_global_t); \
	qcvm_string_list_mark_if_has_ref(&vm->dynamic_strings, field_value, qcvm_get_global(vm, operands.c), span); \
}

F_OP_LOADA(F_OP_LOADA_F, vec_t)
F_OP_LOADA(F_OP_LOADA_V, vec3_t)
F_OP_LOADA(F_OP_LOADA_S, qcvm_string_t)
F_OP_LOADA(F_OP_LOADA_ENT, qcvm_ent_t)
F_OP_LOADA(F_OP_LOADA_FLD, int32_t)
F_OP_LOADA(F_OP_LOADA_FNC, qcvm_func_t)
F_OP_LOADA(F_OP_LOADA_I, int32_t)
#undef F_OP_LOADA

static inline void F_OP_LOADP_BASE(qcvm_t *vm, const qcvm_operands_t operands, int *depth, const size_t TType_size)
{
	const ptrdiff_t address = *qcvm_get_global_typed(int32_t, vm, operands.a) + (*qcvm_get_global_typed(int32_t, vm, operands.b) * sizeof(qcvm_global_t));

	if (!qcvm_pointer_valid(vm, address, false, TType_size))
		qcvm_error(vm, "Invalid pointer %x", address);

	void *field_value = (void *)(address);
	qcvm_set_global(vm, operands.c, field_value, TType_size);

	const size_t span = TType_size / sizeof(qcvm_global_t);
	qcvm_string_list_mark_if_has_ref(&vm->dynamic_strings, field_value, qcvm_get_global(vm, operands.c), span);
}

#define F_OP_LOADP(F_OP, TType) \
static void F_OP(qcvm_t *vm, const qcvm_operands_t operands, int *depth) \
{ \
	F_OP_LOADP_BASE(vm, operands, depth, sizeof(TType)); \
}

F_OP_LOADP(F_OP_LOADP_F, vec_t)
F_OP_LOADP(F_OP_LOADP_V, vec3_t)
F_OP_LOADP(F_OP_LOADP_S, qcvm_string_t)
F_OP_LOADP(F_OP_LOADP_ENT, qcvm_ent_t)
F_OP_LOADP(F_OP_LOADP_FLD, int32_t)
F_OP_LOADP(F_OP_LOADP_FNC, qcvm_func_t)
F_OP_LOADP(F_OP_LOADP_I, int32_t)
#undef F_OP_LOADP

static void F_OP_LOADP_C(qcvm_t *vm, const qcvm_operands_t operands, int *depth)
{
	const qcvm_string_t strid = *qcvm_get_global_typed(qcvm_string_t, vm, operands.a);
	const size_t offset = *qcvm_get_global_typed(int32_t, vm, operands.b);
	int32_t result;

	if (offset > qcvm_get_string_length(vm, strid))
		result = 0;
	else
	{
		const char *str = qcvm_get_string(vm, strid);
		result = str[offset];
	}

	qcvm_set_global_typed_value(int32_t, vm, operands.c, result);
}

static void F_OP_BOUNDCHECK(qcvm_t *vm, const qcvm_operands_t operands, int *depth)
{
#if _DEBUG
	const uint32_t a = *qcvm_get_global_typed(uint32_t, vm, operands.a);
	const uint32_t b = (uint32_t)operands.b;
	const uint32_t c = (uint32_t)operands.c;

	if (a < c || a >= b)
		qcvm_error(vm, "bounds check failed");
#endif
}

static void F_OP_MULSTOREP_F(qcvm_t *vm, const qcvm_operands_t operands, int *depth)
{
	const size_t address = *qcvm_get_global_typed(int32_t, vm, operands.b);

	if (!qcvm_pointer_valid(vm, address, false, sizeof(qcvm_global_t)))
		qcvm_error(vm, "bad pointer");

	vec_t *f = (vec_t *)address;
	const vec_t a = *qcvm_get_global_typed(vec_t, vm, operands.a);
	const vec_t result = (*f) *= a;

	qcvm_set_global_typed_value(vec_t, vm, operands.c, result);
}

static void F_OP_MULSTOREP_VF(qcvm_t *vm, const qcvm_operands_t operands, int *depth)
{
	const size_t address = *qcvm_get_global_typed(int32_t, vm, operands.b);

	if (!qcvm_pointer_valid(vm, address, false, sizeof(vec3_t)))
		qcvm_error(vm, "bad pointer");

	vec3_t *f = (vec3_t *)address;
	const vec_t a = *qcvm_get_global_typed(vec_t, vm, operands.a);
	const vec3_t result = (*f) = VectorScaleF(*f, a);
	qcvm_set_global_typed_value(vec3_t, vm, operands.c, result);
}

static void F_OP_DIVSTOREP_F(qcvm_t *vm, const qcvm_operands_t operands, int *depth)
{
	const size_t address = *qcvm_get_global_typed(int32_t, vm, operands.b);

	if (!qcvm_pointer_valid(vm, address, false, sizeof(vec_t)))
		qcvm_error(vm, "bad pointer");

	vec_t *f = (vec_t *)address;
	const vec_t a = *qcvm_get_global_typed(vec_t, vm, operands.a);
	const vec_t result = (*f) /= a;
	qcvm_set_global_typed_value(vec_t, vm, operands.c, result);
}

static void F_OP_ADDSTOREP_F(qcvm_t *vm, const qcvm_operands_t operands, int *depth)
{
	const size_t address = *qcvm_get_global_typed(int32_t, vm, operands.b);

	if (!qcvm_pointer_valid(vm, address, false, sizeof(vec_t)))
		qcvm_error(vm, "bad pointer");

	vec_t *f = (vec_t *)address;
	const vec_t a = *qcvm_get_global_typed(vec_t, vm, operands.a);
	const vec_t result = (*f) += a;
	qcvm_set_global_typed_value(vec_t, vm, operands.c, result);
}

static void F_OP_ADDSTOREP_V(qcvm_t *vm, const qcvm_operands_t operands, int *depth)
{
	const size_t address = *qcvm_get_global_typed(int32_t, vm, operands.b);

	if (!qcvm_pointer_valid(vm, address, false, sizeof(vec3_t)))
		qcvm_error(vm, "bad pointer");

	vec3_t *f = (vec3_t *)address;
	const vec3_t a = *qcvm_get_global_typed(vec3_t, vm, operands.a);
	const vec3_t result = (*f) = VectorAdd(*f, a);
	qcvm_set_global_typed_value(vec3_t, vm, operands.c, result);
}

static void F_OP_SUBSTOREP_F(qcvm_t *vm, const qcvm_operands_t operands, int *depth)
{
	const size_t address = *qcvm_get_global_typed(int32_t, vm, operands.b);

	if (!qcvm_pointer_valid(vm, address, false, sizeof(vec_t)))
		qcvm_error(vm, "bad pointer");

	vec_t *f = (vec_t *)address;
	const vec_t a = *qcvm_get_global_typed(vec_t, vm, operands.a);
	const vec_t result = (*f) -= a;
	qcvm_set_global_typed_value(vec_t, vm, operands.c, result);
}

static void F_OP_SUBSTOREP_V(qcvm_t *vm, const qcvm_operands_t operands, int *depth)
{
	const size_t address = *qcvm_get_global_typed(int32_t, vm, operands.b);

	if (!qcvm_pointer_valid(vm, address, false, sizeof(vec3_t)))
		qcvm_error(vm, "bad pointer");

	vec3_t *f = (vec3_t *)address;
	const vec3_t a = *qcvm_get_global_typed(vec3_t, vm, operands.a);
	const vec3_t result = (*f) = VectorSubtract(*f, a);
	qcvm_set_global_typed_value(vec3_t, vm, operands.c, result);
}

static void F_OP_RAND0(qcvm_t *vm, const qcvm_operands_t operands, int *depth)
{
	const vec_t result = frand();
	qcvm_set_global_typed_value(vec_t, vm, operands.c, result);
}

static void F_OP_RAND1(qcvm_t *vm, const qcvm_operands_t operands, int *depth)
{
	const vec_t result = frand_m(*qcvm_get_global_typed(vec_t, vm, operands.a));
	qcvm_set_global_typed_value(vec_t, vm, operands.c, result);
}

static void F_OP_RAND2(qcvm_t *vm, const qcvm_operands_t operands, int *depth)
{
	const vec_t result = frand_mm(*qcvm_get_global_typed(vec_t, vm, operands.a), *qcvm_get_global_typed(vec_t, vm, operands.b));
	qcvm_set_global_typed_value(vec_t, vm, operands.c, result);
}

static void F_OP_RANDV0(qcvm_t *vm, const qcvm_operands_t operands, int *depth)
{
	const vec3_t result = { frand(), frand(), frand() };
	qcvm_set_global_typed_value(vec3_t, vm, operands.c, result);
}

static void F_OP_RANDV1(qcvm_t *vm, const qcvm_operands_t operands, int *depth)
{
	const vec3_t a = *qcvm_get_global_typed(vec3_t, vm, operands.a);
	const vec3_t result = { frand_m(a.x), frand_m(a.y), frand_m(a.z) };
	qcvm_set_global_typed_value(vec3_t, vm, operands.c, result);
}

static void F_OP_RANDV2(qcvm_t *vm, const qcvm_operands_t operands, int *depth)
{
	const vec3_t a = *qcvm_get_global_typed(vec3_t, vm, operands.a);
	const vec3_t b = *qcvm_get_global_typed(vec3_t, vm, operands.b);
	const vec3_t result = { frand_mm(a.x, b.x), frand_mm(a.y, b.y), frand_mm(a.z, b.z) };
	qcvm_set_global_typed_value(vec3_t, vm, operands.c, result);
}

#define F_OP_STOREF(F_OP, TType) \
static void F_OP(qcvm_t *vm, const qcvm_operands_t operands, int *depth) \
{ \
	edict_t *ent = qcvm_ent_to_entity(*qcvm_get_global_typed(qcvm_ent_t, vm, operands.a), true); \
	const ptrdiff_t field_offset = *qcvm_get_global_typed(int32_t, vm, operands.b); \
	const size_t address = (size_t)((int32_t*)ent + field_offset); \
	TType *field_value = (TType *)address; \
	const TType *value = qcvm_get_global_typed(TType, vm, operands.c); \
\
	*field_value = *value; \
\
	qcvm_string_list_mark_if_has_ref(&vm->dynamic_strings, value, field_value, sizeof(TType) / sizeof(qcvm_global_t)); \
}

F_OP_STOREF(F_OP_STOREF_F, vec_t)
F_OP_STOREF(F_OP_STOREF_S, qcvm_string_t)
F_OP_STOREF(F_OP_STOREF_I, int32_t)
F_OP_STOREF(F_OP_STOREF_V, vec3_t)
#undef F_OP_STOREF

#define OP(N) \
	[OP_##N] = F_OP_##N

qcvm_opcode_func_t qcvm_code_funcs[] = {
	OP(DONE),

	OP(MUL_F),
	OP(MUL_V),
	OP(MUL_FV),
	OP(MUL_VF),
	OP(MUL_I),
	OP(MUL_IF),
	OP(MUL_FI),
	OP(MUL_VI),
	OP(MUL_IV),
	
	OP(DIV_F),
	OP(DIV_I),
	OP(DIV_VF),
	OP(DIV_IF),
	OP(DIV_FI),
	
	OP(ADD_F),
	OP(ADD_V),
	OP(ADD_I),
	OP(ADD_FI),
	OP(ADD_IF),
	
	OP(SUB_F),
	OP(SUB_V),
	OP(SUB_I),
	OP(SUB_FI),
	OP(SUB_IF),
	
	OP(EQ_F),
	OP(EQ_V),
	OP(EQ_S),
	OP(EQ_E),
	OP(EQ_FNC),
	OP(EQ_I),
	OP(EQ_IF),
	OP(EQ_FI),

	OP(NE_F),
	OP(NE_V),
	OP(NE_S),
	OP(NE_E),
	OP(NE_FNC),
	OP(NE_I),
	OP(NE_IF),
	OP(NE_FI),
	
	OP(LE_F),
	OP(LE_I),
	OP(LE_IF),
	OP(LE_FI),
	
	OP(GE_F),
	OP(GE_I),
	OP(GE_IF),
	OP(GE_FI),
	
	OP(LT_F),
	OP(LT_I),
	OP(LT_IF),
	OP(LT_FI),
	
	OP(GT_F),
	OP(GT_I),
	OP(GT_IF),
	OP(GT_FI),
	
	OP(LOAD_F),
	OP(LOAD_V),
	OP(LOAD_S),
	OP(LOAD_ENT),
	OP(LOAD_FLD),
	OP(LOAD_FNC),
	OP(LOAD_I),
	OP(LOAD_P),

	OP(ADDRESS),
	
	OP(STORE_F),
	OP(STORE_V),
	OP(STORE_S),
	OP(STORE_ENT),
	OP(STORE_FLD),
	OP(STORE_FNC),
	OP(STORE_I),
	OP(STORE_IF),
	OP(STORE_FI),
	OP(STORE_P),
	
	OP(STOREP_F),
	OP(STOREP_V),
	OP(STOREP_S),
	OP(STOREP_ENT),
	OP(STOREP_FLD),
	OP(STOREP_FNC),
	OP(STOREP_I),
	OP(STOREP_IF),
	OP(STOREP_FI),

	OP(RETURN),
		
	OP(MULSTOREP_F),
	OP(MULSTOREP_VF),
		
	OP(DIVSTOREP_F),
		
	OP(ADDSTOREP_F),
	OP(ADDSTOREP_V),
		
	OP(SUBSTOREP_F),
	OP(SUBSTOREP_V),
		
	OP(NOT_F),
	OP(NOT_V),
	OP(NOT_S),
	OP(NOT_FNC),
	OP(NOT_ENT),
	OP(NOT_I),
		
	OP(IF_I),
	OP(IF_S),
	OP(IF_F),
		
	OP(IFNOT_I),
	OP(IFNOT_S),
	OP(IFNOT_F),
		
	OP(CALL0),
	OP(CALL1),
	OP(CALL2),
	OP(CALL3),
	OP(CALL4),
	OP(CALL5),
	OP(CALL6),
	OP(CALL7),
	OP(CALL8),
		
	OP(CALL1H),
	OP(CALL2H),
	OP(CALL3H),
	OP(CALL4H),
	OP(CALL5H),
	OP(CALL6H),
	OP(CALL7H),
	OP(CALL8H),

	OP(GOTO),
		
	OP(AND_F),
	OP(AND_I),
	OP(AND_IF),
	OP(AND_FI),
		
	OP(OR_F),
	OP(OR_I),
	OP(OR_IF),
	OP(OR_FI),
		
	OP(BITAND_F),
	OP(BITAND_I),
	OP(BITAND_IF),
	OP(BITAND_FI),
		
	OP(BITOR_F),
	OP(BITOR_I),
	OP(BITOR_IF),
	OP(BITOR_FI),
		
	OP(CONV_ITOF),
	OP(CONV_FTOI),
	OP(CP_ITOF),
	OP(CP_FTOI),
		
	OP(BITXOR_I),
	OP(RSHIFT_I),
	OP(LSHIFT_I),

	OP(GLOBALADDRESS),
	OP(ADD_PIW),

	OP(LOADA_F),
	OP(LOADA_V),
	OP(LOADA_S),
	OP(LOADA_ENT),
	OP(LOADA_FLD),
	OP(LOADA_FNC),
	OP(LOADA_I),

	OP(LOADP_F),
	OP(LOADP_V),
	OP(LOADP_S),
	OP(LOADP_ENT),
	OP(LOADP_FLD),
	OP(LOADP_FNC),
	OP(LOADP_I),
	OP(LOADP_C),

	OP(BOUNDCHECK),
		
	OP(RAND0),
	OP(RAND1),
	OP(RAND2),

	OP(RANDV0),
	OP(RANDV1),
	OP(RANDV2),
		
	OP(STOREF_F),
	OP(STOREF_S),
	OP(STOREF_I),
	OP(STOREF_V),
};