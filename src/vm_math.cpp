#include "shared/shared.h"
#include "game.h"
#include "g_vm.h"

static void QC_fabsf(QCVM &vm)
{
	const auto &v = vm.ArgvFloat(0);
	vm.Return(fabsf(v));
}

static void QC_sqrtf(QCVM &vm)
{
	const auto &v = vm.ArgvFloat(0);
	vm.Return(sqrtf(v));
}

static void QC_sinf(QCVM &vm)
{
	const auto &v = vm.ArgvFloat(0);
	vm.Return(sinf(v));
}

static void QC_cosf(QCVM &vm)
{
	const auto &v = vm.ArgvFloat(0);
	vm.Return(cosf(v));
}

static void QC_atan2f(QCVM &vm)
{
	const auto &x = vm.ArgvFloat(0);
	const auto &y = vm.ArgvFloat(1);
	vm.Return(atan2f(x, y));
}

static void QC_atanf(QCVM &vm)
{
	const auto &x = vm.ArgvFloat(0);
	vm.Return(atanf(x));
}

static void QC_asinf(QCVM &vm)
{
	const auto &x = vm.ArgvFloat(0);
	vm.Return(asinf(x));
}

static void QC_isnan(QCVM &vm)
{
	const auto &x = vm.ArgvFloat(0);
	vm.Return(isnan(x));
}

static void QC_floorf(QCVM &vm)
{
	const auto &v = vm.ArgvFloat(0);
	vm.Return(floorf(v));
}

static void QC_ceilf(QCVM &vm)
{
	const auto &v = vm.ArgvFloat(0);
	vm.Return(ceilf(v));
}

static void QC_roundf(QCVM &vm)
{
	const auto &v = vm.ArgvFloat(0);
	vm.Return(roundf(v));
}

static void QC_tanf(QCVM &vm)
{
	const auto &v = vm.ArgvFloat(0);
	vm.Return(tanf(v));
}



/*
=====================================================================

  MT19337 PRNG

=====================================================================
*/

#include <random>

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
	vm.Return(static_cast<int32_t>(Q_rand() & 0x7FFFFFFF));
}

static void QC_Q_rand_uniform(QCVM &vm)
{
	vm.Return(static_cast<int32_t>(Q_rand_uniform(vm.ArgvInt32(0))));
}

static void QC_frandom(QCVM &vm)
{
	vm.Return(frand());
}

static void QC_crandom(QCVM &vm)
{
	vm.Return(frand(-1, 1));
}

static void QC_frandomv(QCVM &vm)
{
	vm.Return(vec3_t { frand(), frand(), frand() });
}

static void QC_crandomv(QCVM &vm)
{
	vm.Return(vec3_t { frand(-1, 1), frand(-1, 1), frand(-1, 1) });
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
	RegisterBuiltin(frandom);
	RegisterBuiltin(crandom);
	RegisterBuiltin(frandomv);
	RegisterBuiltin(crandomv);
}