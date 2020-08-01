#include "shared/shared.h"
#include "game.h"
#include "g_vm.h"

static void QC_fabsf(qcvm_t *vm)
{
	const vec_t v = qcvm_argv_float(vm, 0);
	qcvm_return_float(vm, fabsf(v));
}

static void QC_sqrtf(qcvm_t *vm)
{
	const vec_t v = qcvm_argv_float(vm, 0);
	qcvm_return_float(vm, sqrtf(v));
}

static void QC_sinf(qcvm_t *vm)
{
	const vec_t v = qcvm_argv_float(vm, 0);
	qcvm_return_float(vm, sinf(v));
}

static void QC_cosf(qcvm_t *vm)
{
	const vec_t v = qcvm_argv_float(vm, 0);
	qcvm_return_float(vm, cosf(v));
}

static void QC_atan2f(qcvm_t *vm)
{
	const vec_t x = qcvm_argv_float(vm, 0);
	const vec_t y = qcvm_argv_float(vm, 1);
	qcvm_return_float(vm, atan2f(x, y));
}

static void QC_atanf(qcvm_t *vm)
{
	const vec_t x = qcvm_argv_float(vm, 0);
	qcvm_return_float(vm, atanf(x));
}

static void QC_asinf(qcvm_t *vm)
{
	const vec_t x = qcvm_argv_float(vm, 0);
	qcvm_return_float(vm, asinf(x));
}

static void QC_isnan(qcvm_t *vm)
{
	const vec_t x = qcvm_argv_float(vm, 0);
	qcvm_return_float(vm, isnan(x));
}

static void QC_floorf(qcvm_t *vm)
{
	const vec_t v = qcvm_argv_float(vm, 0);
	qcvm_return_float(vm, floorf(v));
}

static void QC_ceilf(qcvm_t *vm)
{
	const vec_t v = qcvm_argv_float(vm, 0);
	qcvm_return_float(vm, ceilf(v));
}

static void QC_roundf(qcvm_t *vm)
{
	const vec_t v = qcvm_argv_float(vm, 0);
	qcvm_return_float(vm, roundf(v));
}

static void QC_tanf(qcvm_t *vm)
{
	const vec_t v = qcvm_argv_float(vm, 0);
	qcvm_return_float(vm, tanf(v));
}

/*
=====================================================================

  MT19337 PRNG

=====================================================================
*/

#include <random>
#include <ctime>

static std::mt19937 mt((size_t)time(NULL));

inline uint32_t Q_rand(void)
{
	return mt();
}

inline uint32_t Q_rand_uniform(uint32_t n)
{
	return std::uniform_int_distribution<uint32_t>(0, n - 1)(mt);
}

vec_t frand()
{
	return std::uniform_real<vec_t>()(mt);
}

vec_t frand(const vec_t max)
{
	return std::uniform_real<vec_t>(0.f, max)(mt);
}

vec_t frand(const vec_t min, const vec_t max)
{
	return std::uniform_real<vec_t>(min, max)(mt);
}

static void QC_Q_rand(qcvm_t *vm)
{
	qcvm_return_int32(vm, Q_rand() & 0x7FFFFFFF);
}

static void QC_Q_rand_uniform(qcvm_t *vm)
{
	qcvm_return_int32(vm, Q_rand_uniform(qcvm_argv_int32(vm, 0)));
}

void InitMathBuiltins(qcvm_t *vm)
{
	RegisterBuiltin(fabsf);
	RegisterBuiltin(sqrtf);
	RegisterBuiltin(sinf);
	RegisterBuiltin(cosf);
	RegisterBuiltin(atan2f);
	RegisterBuiltin(floorf);
	RegisterBuiltin(ceilf);
	RegisterBuiltin(roundf);
	RegisterBuiltin(tanf);
	RegisterBuiltin(atanf);
	RegisterBuiltin(asinf);
	RegisterBuiltin(isnan);
	
	RegisterBuiltin(Q_rand);
	RegisterBuiltin(Q_rand_uniform);
}