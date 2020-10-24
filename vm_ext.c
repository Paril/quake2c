#include "shared/shared.h"
#include "vm.h"
#include "vm_ext.h"

static void QC_ModInt(qcvm_t *vm)
{
	const int a = qcvm_argv_int32(vm, 0);
	const int b = qcvm_argv_int32(vm, 1);

	qcvm_return_int32(vm, a % b);
}

static void QC_func_get(qcvm_t *vm)
{
	const char *s = qcvm_argv_string(vm, 0);
	qcvm_func_t func = qcvm_find_function_id(vm, s);
	qcvm_return_func(vm, func);
}

static void QC_handle_free(qcvm_t *vm)
{
	const int32_t id = qcvm_argv_int32(vm, 0);
	qcvm_handle_free(vm, qcvm_fetch_handle(vm, id));
}

typedef struct
{
	qcvm_t			*vm;
	qcvm_pointer_t	elements;
	qcvm_pointer_t	ctx;
	qcvm_function_t *func;
} qsort_context_t;

#ifdef _WIN32
static int QC_qsort_callback(void *ctx, const void *a, const void *b)
#else
static int QC_qsort_callback(const void *a, const void *b, void *ctx)
#endif
{
	qsort_context_t *context = (qsort_context_t *)ctx;
	qcvm_pointer_t a_ptr = qcvm_make_pointer(context->vm, context->elements.raw.type, a);
	qcvm_pointer_t b_ptr = qcvm_make_pointer(context->vm, context->elements.raw.type, b);
	
	qcvm_set_global_typed_value(qcvm_pointer_t, context->vm, GLOBAL_PARM0, a_ptr);
	qcvm_set_global_typed_value(qcvm_pointer_t, context->vm, GLOBAL_PARM1, b_ptr);
	qcvm_set_global_typed_value(qcvm_pointer_t, context->vm, GLOBAL_PARM2, context->ctx);
	qcvm_execute(context->vm, context->func);

	return *qcvm_get_global_typed(int32_t, context->vm, GLOBAL_RETURN);
}

static void QC_qsort(qcvm_t *vm)
{
	const qcvm_pointer_t elements = qcvm_argv_pointer(vm, 0);
	const int32_t num = qcvm_argv_int32(vm, 1);
	const int32_t size_of_element = qcvm_argv_int32(vm, 2);
	void *address;

	if (elements.raw.type == QCVM_POINTER_HANDLE || !qcvm_resolve_pointer(vm, elements, false, num * size_of_element, &address))
		qcvm_error(vm, "bad pointer");

	qcvm_function_t *comparator_func = qcvm_get_function(vm, qcvm_argv_int32(vm, 3));

	qsort_context_t context = { vm, elements, { 0 }, comparator_func };

	if (vm->state.argc > 4)
		context.ctx = qcvm_argv_pointer(vm, 4);

	qsort_s(address, num, size_of_element, QC_qsort_callback, &context);
}

void qcvm_init_ext_builtins(qcvm_t *vm)
{
	qcvm_register_builtin(ModInt);
	qcvm_register_builtin(func_get);
	qcvm_register_builtin(handle_free);
	qcvm_register_builtin(qsort);
}