#include "shared/shared.h"
#include "game.h"
#include "g_vm.h"

static void QC_AngleVectors(QCVM &vm)
{
	const auto &v = vm.ArgvVector(0);

	vec3_t forward, right, up;
	AngleVectors(v, forward, right, up);

	vm.SetGlobal<vec3_t>(global_t::PARM1, forward);
	vm.SetGlobal<vec3_t>(global_t::PARM2, right);
	vm.SetGlobal<vec3_t>(global_t::PARM3, up);
}

static void QC_VectorNormalize(QCVM &vm)
{
	auto v = vm.ArgvVector(0);
	vm.Return(VectorNormalize(v));
	vm.SetGlobal<vec3_t>(global_t::PARM0, v);
}

static void vectoangles(const vec3_t &value1, vec3_t &angles)
{
	float	forward;
	float	yaw, pitch;
	
	if (value1[1] == 0 && value1[0] == 0)
	{
		yaw = 0;
		if (value1[2] > 0)
			pitch = 90;
		else
			pitch = 270;
	}
	else
	{
	// PMM - fixed to correct for pitch of 0
		if (value1[0])
			yaw = (atan2(value1[1], value1[0]) * 180 / M_PI);
		else if (value1[1] > 0)
			yaw = 90;
		else
			yaw = 270;

		if (yaw < 0)
			yaw += 360;

		forward = sqrt (value1[0]*value1[0] + value1[1]*value1[1]);
		pitch = (atan2(value1[2], forward) * 180 / M_PI);
		if (pitch < 0)
			pitch += 360;
	}

	angles[PITCH] = -pitch;
	angles[YAW] = yaw;
	angles[ROLL] = 0;
}

static void QC_vectoangles(QCVM &vm)
{
	auto &v = vm.ArgvVector(0);
	vec3_t angles;
	vectoangles(v, angles);
	vm.SetGlobal<vec3_t>(global_t::PARM1, angles);
}

static void QC_CrossProduct(QCVM &vm)
{
	auto &a = vm.ArgvVector(0);
	auto &b = vm.ArgvVector(1);
	vec3_t c;
	CrossProduct(a, b, c);
	vm.SetGlobal<vec3_t>(global_t::PARM2, c);
}

static void QC_VectorLength(QCVM &vm)
{
	auto &a = vm.ArgvVector(0);
	vm.Return(VectorLength(a));
}

void InitVectorBuiltins(QCVM &vm)
{
	RegisterBuiltin(AngleVectors);
	RegisterBuiltin(VectorNormalize);
	RegisterBuiltin(vectoangles);
	RegisterBuiltin(CrossProduct);
	RegisterBuiltin(VectorLength);
}