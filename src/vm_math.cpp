#include "shared/shared.h"
#include "game.h"
#include "g_vm.h"

static void QC_fabsf(QCVM &vm)
{
	const auto &v = vm.ArgvFloat(0);
	vm.ReturnFloat(fabsf(v));
}

static void QC_sqrtf(QCVM &vm)
{
	const auto &v = vm.ArgvFloat(0);
	vm.ReturnFloat(sqrtf(v));
}

static void QC_sinf(QCVM &vm)
{
	const auto &v = vm.ArgvFloat(0);
	vm.ReturnFloat(sinf(v));
}

static void QC_cosf(QCVM &vm)
{
	const auto &v = vm.ArgvFloat(0);
	vm.ReturnFloat(cosf(v));
}

static void QC_atan2f(QCVM &vm)
{
	const auto &x = vm.ArgvFloat(0);
	const auto &y = vm.ArgvFloat(1);
	vm.ReturnFloat(atan2f(x, y));
}

static void QC_atanf(QCVM &vm)
{
	const auto &x = vm.ArgvFloat(0);
	vm.ReturnFloat(atanf(x));
}

static void QC_asinf(QCVM &vm)
{
	const auto &x = vm.ArgvFloat(0);
	vm.ReturnFloat(asinf(x));
}

static void QC_isnan(QCVM &vm)
{
	const auto &x = vm.ArgvFloat(0);
	vm.ReturnFloat(isnan(x));
}

static void QC_floorf(QCVM &vm)
{
	const auto &v = vm.ArgvFloat(0);
	vm.ReturnFloat(floorf(v));
}

static void QC_ceilf(QCVM &vm)
{
	const auto &v = vm.ArgvFloat(0);
	vm.ReturnFloat(ceilf(v));
}

static void QC_roundf(QCVM &vm)
{
	const auto &v = vm.ArgvFloat(0);
	vm.ReturnFloat(roundf(v));
}

static void QC_tanf(QCVM &vm)
{
	const auto &v = vm.ArgvFloat(0);
	vm.ReturnFloat(tanf(v));
}

/*
=====================================================================

  MT19337 PRNG

=====================================================================
*/

#include <random>
#include <ctime>

static std::mt19937 mt(static_cast<uint32_t>(time(NULL)));

inline uint32_t Q_rand(void)
{
	return mt();
}

inline uint32_t Q_rand_uniform(uint32_t n)
{
	return std::uniform_int_distribution<uint32_t>(0, n - 1)(mt);
}

float frand()
{
	return std::uniform_real<float>()(mt);
}

float frand(const float &max)
{
	return std::uniform_real<float>(0.f, max)(mt);
}

float frand(const float &min, const float &max)
{
	return std::uniform_real<float>(min, max)(mt);
}

static void QC_Q_rand(QCVM &vm)
{
	vm.ReturnInt(static_cast<int32_t>(Q_rand() & 0x7FFFFFFF));
}

static void QC_Q_rand_uniform(QCVM &vm)
{
	vm.ReturnInt(static_cast<int32_t>(Q_rand_uniform(vm.ArgvInt32(0))));
}


void InitMathBuiltins(QCVM &vm)
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