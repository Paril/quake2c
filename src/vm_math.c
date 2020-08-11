#include "shared/shared.h"
#include "g_vm.h"
#include "vm_math.h"
#include <float.h>

// float(float)
#define MATH_FUNC_RF_AF(name) \
static void QC_##name(qcvm_t *vm) \
{ \
	const vec_t v = qcvm_argv_float(vm, 0); \
	qcvm_return_float(vm, name(v)); \
}

// float(float, float)
#define MATH_FUNC_RF_AFF(name) \
static void QC_##name(qcvm_t *vm) \
{ \
	const vec_t v1 = qcvm_argv_float(vm, 0); \
	const vec_t v2 = qcvm_argv_float(vm, 1); \
	qcvm_return_float(vm, name(v1, v2)); \
}

// float(float, __out int)
#define MATH_FUNC_RF_AFOI(name) \
static void QC_##name(qcvm_t *vm) \
{ \
	const vec_t v1 = qcvm_argv_float(vm, 0); \
	int32_t v2; \
	const vec_t result = name(v1, &v2); \
	qcvm_return_float(vm, result); \
	qcvm_set_global_typed_value(int32_t, vm, GLOBAL_PARM1, v2); \
}

// float(float, __out float)
#define MATH_FUNC_RF_AFOF(name) \
static void QC_##name(qcvm_t *vm) \
{ \
	const vec_t v1 = qcvm_argv_float(vm, 0); \
	double v2_; \
	const vec_t result = name(v1, &v2_); \
	vec_t v2 = (vec_t)v2_; \
	qcvm_return_float(vm, result); \
	qcvm_set_global_typed_value(vec_t, vm, GLOBAL_PARM1, v2); \
}

// float(float, float)
#define MATH_FUNC_RF_AFI(name) \
static void QC_##name(qcvm_t *vm) \
{ \
	const vec_t v1 = qcvm_argv_float(vm, 0); \
	const int32_t v2 = qcvm_argv_int32(vm, 1); \
	qcvm_return_float(vm, name(v1, v2)); \
}

// int(float)
#define MATH_FUNC_RI_AF(name) \
static void QC_##name(qcvm_t *vm) \
{ \
	const vec_t v = qcvm_argv_float(vm, 0); \
	qcvm_return_int32(vm, name(v)); \
}

// float(float, float, __out int)
#define MATH_FUNC_RF_AFFOI(name) \
static void QC_##name(qcvm_t *vm) \
{ \
	const vec_t v1 = qcvm_argv_float(vm, 0); \
	const vec_t v2 = qcvm_argv_float(vm, 1); \
	int32_t v3; \
	const vec_t result = name(v1, v2, &v3); \
	qcvm_return_float(vm, result); \
	qcvm_set_global_typed_value(int32_t, vm, GLOBAL_PARM2, v2); \
}

// float(string)
#define MATH_FUNC_RF_AS(name) \
static void QC_##name(qcvm_t *vm) \
{ \
	const char *v = qcvm_argv_string(vm, 0); \
	qcvm_return_float(vm, name(v)); \
}

// int(int)
#define MATH_FUNC_RI_AI(name) \
static void QC_##name(qcvm_t *vm) \
{ \
	const int32_t v = qcvm_argv_int32(vm, 0); \
	qcvm_return_int32(vm, name(v)); \
}

// float(float, float, float)
#define MATH_FUNC_RF_AFFF(name) \
static void QC_##name(qcvm_t *vm) \
{ \
	const vec_t v1 = qcvm_argv_float(vm, 0); \
	const vec_t v2 = qcvm_argv_float(vm, 1); \
	const vec_t v3 = qcvm_argv_float(vm, 2); \
	qcvm_return_float(vm, name(v1, v2, v3)); \
}

// trig
MATH_FUNC_RF_AF(cos);
MATH_FUNC_RF_AF(sin);
MATH_FUNC_RF_AF(tan);
MATH_FUNC_RF_AF(acos);
MATH_FUNC_RF_AF(asin);
MATH_FUNC_RF_AF(atan);
MATH_FUNC_RF_AFF(atan2);

// hyperbolic
MATH_FUNC_RF_AF(cosh);
MATH_FUNC_RF_AF(sinh);
MATH_FUNC_RF_AF(tanh);
MATH_FUNC_RF_AF(acosh);
MATH_FUNC_RF_AF(asinh);
MATH_FUNC_RF_AF(atanh);

// exp/logarithmic
MATH_FUNC_RF_AF(exp);
MATH_FUNC_RF_AFOI(frexp);
MATH_FUNC_RF_AFI(ldexp);
MATH_FUNC_RF_AF(log);
MATH_FUNC_RF_AF(log10);
MATH_FUNC_RF_AFOF(modf);
MATH_FUNC_RF_AF(exp2);
MATH_FUNC_RF_AF(expm1);
MATH_FUNC_RI_AF(ilogb);
MATH_FUNC_RF_AF(log1p);
MATH_FUNC_RF_AF(log2);
MATH_FUNC_RF_AF(logb);
MATH_FUNC_RF_AFI(scalbn);

// pow
MATH_FUNC_RF_AFF(pow);
MATH_FUNC_RF_AF(sqrt);
MATH_FUNC_RF_AF(cbrt);
MATH_FUNC_RF_AFF(hypot);

// error and gamma
MATH_FUNC_RF_AF(erf);
MATH_FUNC_RF_AF(erfc);
MATH_FUNC_RF_AF(tgamma);
MATH_FUNC_RF_AF(lgamma);

// rounding/remainder
MATH_FUNC_RF_AF(ceil);
MATH_FUNC_RF_AF(floor);
MATH_FUNC_RF_AFF(fmod);
MATH_FUNC_RF_AF(trunc);
MATH_FUNC_RF_AF(round);
MATH_FUNC_RI_AF(lround);
MATH_FUNC_RF_AF(rint);
MATH_FUNC_RI_AF(lrint);
MATH_FUNC_RF_AF(nearbyint);
MATH_FUNC_RF_AFF(remainder);
MATH_FUNC_RF_AFFOI(remquo);

// floating point
MATH_FUNC_RF_AFF(copysign);
MATH_FUNC_RF_AS(nan);
MATH_FUNC_RF_AFF(nextafter);
MATH_FUNC_RF_AFF(nexttoward);

// other
MATH_FUNC_RF_AF(fabs);
MATH_FUNC_RI_AI(abs);
MATH_FUNC_RF_AFFF(fma);

// classifications
MATH_FUNC_RI_AF(isfinite);
MATH_FUNC_RI_AF(isinf);
MATH_FUNC_RI_AF(isnan);
MATH_FUNC_RI_AF(isnormal);
MATH_FUNC_RI_AF(signbit);

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

void qcvm_init_math_builtins(qcvm_t *vm)
{
	// trig
	qcvm_register_builtin(cos);
	qcvm_register_builtin(sin);
	qcvm_register_builtin(tan);
	qcvm_register_builtin(acos);
	qcvm_register_builtin(asin);
	qcvm_register_builtin(atan);
	qcvm_register_builtin(atan2);

	// hyperbolic
	qcvm_register_builtin(cosh);
	qcvm_register_builtin(sinh);
	qcvm_register_builtin(tanh);
	qcvm_register_builtin(acosh);
	qcvm_register_builtin(asinh);
	qcvm_register_builtin(atanh);

	// exp/logarithmic
	qcvm_register_builtin(exp);
	qcvm_register_builtin(frexp);
	qcvm_register_builtin(ldexp);
	qcvm_register_builtin(log);
	qcvm_register_builtin(log10);
	qcvm_register_builtin(modf);
	qcvm_register_builtin(exp2);
	qcvm_register_builtin(expm1);
	qcvm_register_builtin(ilogb);
	qcvm_register_builtin(log1p);
	qcvm_register_builtin(log2);
	qcvm_register_builtin(logb);
	qcvm_register_builtin(scalbn);

	// pow
	qcvm_register_builtin(pow);
	qcvm_register_builtin(sqrt);
	qcvm_register_builtin(cbrt);
	qcvm_register_builtin(hypot);

	// error and gamma
	qcvm_register_builtin(erf);
	qcvm_register_builtin(erfc);
	qcvm_register_builtin(tgamma);
	qcvm_register_builtin(lgamma);

	// rounding/remainder
	qcvm_register_builtin(ceil);
	qcvm_register_builtin(floor);
	qcvm_register_builtin(fmod);
	qcvm_register_builtin(trunc);
	qcvm_register_builtin(round);
	qcvm_register_builtin(lround);
	qcvm_register_builtin(rint);
	qcvm_register_builtin(lrint);
	qcvm_register_builtin(nearbyint);
	qcvm_register_builtin(remainder);
	qcvm_register_builtin(remquo);

	// floating point
	qcvm_register_builtin(copysign);
	qcvm_register_builtin(nan);
	qcvm_register_builtin(nextafter);
	qcvm_register_builtin(nexttoward);

	// other functions
	qcvm_register_builtin(fabs);
	qcvm_register_builtin(abs);
	qcvm_register_builtin(fma);

	// classifications
	qcvm_register_builtin(isfinite);
	qcvm_register_builtin(isinf);
	qcvm_register_builtin(isnan);
	qcvm_register_builtin(isnormal);
	qcvm_register_builtin(signbit);

	// set the FLT_ constants
	qcvm_definition_t *def = qcvm_find_definition(vm, "FLT_MAX", TYPE_FLOAT);

	if (def)
	{
		const vec_t val = FLT_MAX;
		qcvm_set_global_typed_value(vec_t, vm, def->global_index, val);
	}

	def = qcvm_find_definition(vm, "FLT_EPSILON", TYPE_FLOAT);

	if (def)
	{
		const vec_t val = FLT_EPSILON;
		qcvm_set_global_typed_value(vec_t, vm, def->global_index, val);
	}

	def = qcvm_find_definition(vm, "FLT_MIN", TYPE_FLOAT);

	if (def)
	{
		const vec_t val = FLT_MIN;
		qcvm_set_global_typed_value(vec_t, vm, def->global_index, val);
	}
	
	// randomness
	qcvm_register_builtin(Q_rand);
	qcvm_register_builtin(Q_rand_uniform);
}