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

enum { STATE_VECTOR_LENGTH = 624 };
enum { STATE_VECTOR_M      = 397 }; /* changes to STATE_VECTOR_LENGTH also require changes to this */

static struct
{
	uint32_t mt[STATE_VECTOR_LENGTH];
	int32_t index;
} qcvm_mt;

enum { UPPER_MASK		= (int32_t)0x80000000 };
enum { LOWER_MASK		= 0x7fffffff };
enum { TEMPERING_MASK_B	= (int32_t)0x9d2c5680 };
enum { TEMPERING_MASK_C	= (int32_t)0xefc60000 };

void Q_srand(const uint32_t seed)
{
	qcvm_mt.mt[0] = seed & 0xffffffff;

	for (qcvm_mt.index = 1; qcvm_mt.index < STATE_VECTOR_LENGTH; qcvm_mt.index++)
		qcvm_mt.mt[qcvm_mt.index] = (6069 * qcvm_mt.mt[qcvm_mt.index - 1]) & 0xffffffff;
}

static uint32_t Q_rand()
{
	unsigned long y;
	static unsigned long mag[2] = { 0x0, 0x9908b0df };

	if (qcvm_mt.index >= STATE_VECTOR_LENGTH || qcvm_mt.index < 0)
	{
		int kk;

		if (qcvm_mt.index >= STATE_VECTOR_LENGTH+1 || qcvm_mt.index < 0)
			Q_srand(4357);

		for (kk = 0; kk < STATE_VECTOR_LENGTH - STATE_VECTOR_M; kk++)
		{
			y = (qcvm_mt.mt[kk] & UPPER_MASK) | (qcvm_mt.mt[kk+1] & LOWER_MASK);
			qcvm_mt.mt[kk] = qcvm_mt.mt[kk+STATE_VECTOR_M] ^ (y >> 1) ^ mag[y & 0x1];
		}

		for (; kk < STATE_VECTOR_LENGTH - 1; kk++)
		{
			y = (qcvm_mt.mt[kk] & UPPER_MASK) | (qcvm_mt.mt[kk+1] & LOWER_MASK);
			qcvm_mt.mt[kk] = qcvm_mt.mt[kk+(STATE_VECTOR_M-STATE_VECTOR_LENGTH)] ^ (y >> 1) ^ mag[y & 0x1];
		}

		y = (qcvm_mt.mt[STATE_VECTOR_LENGTH-1] & UPPER_MASK) | (qcvm_mt.mt[0] & LOWER_MASK);
			 qcvm_mt.mt[STATE_VECTOR_LENGTH-1] = qcvm_mt.mt[STATE_VECTOR_M-1] ^ (y >> 1) ^ mag[y & 0x1];
			 qcvm_mt.index = 0;
	}

	y = qcvm_mt.mt[qcvm_mt.index++];
	y ^= (y >> 11);
	y ^= (y << 7) & TEMPERING_MASK_B;
	y ^= (y << 15) & TEMPERING_MASK_C;
	y ^= (y >> 18);
	return y;
}

static uint32_t Q_rand_uniform(uint32_t n)
{
    uint32_t x, r;

    do
	{
        x = Q_rand();
        r = x % n;
    } while (x - r > (-n));

    return r;
}

vec_t frand()
{
	return (vec_t)(Q_rand()) / 0xffffffffu;
}

vec_t frand_m(const vec_t max)
{
	return frand() * max;
}

vec_t frand_mm(const vec_t min, const vec_t max)
{
	return frand() * (max - min) + min;
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