#pragma once

typedef struct
{
	int	msec;
	int	buttons;
	vec3_t angles;
	int	forwardmove, sidemove, upmove;
	int	impulse;		// remove?
	int	lightlevel;		// light level the player is standing on
} QC_usercmd_t;

void qcvm_init_gi_builtins(qcvm_t *vm);