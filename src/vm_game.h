#pragma once

// used by multiple vm_ files
struct QC_usercmd_t
{
	int	msec;
	int	buttons;
	vec3_t	angles;
	int	forwardmove, sidemove, upmove;
	int	impulse;		// remove?
	int	lightlevel;		// light level the player is standing on
};

// Builtin "modules"
void InitGIBuiltins(qcvm_t *vm);
void InitGameBuiltins(qcvm_t *vm);
void InitExtBuiltins(qcvm_t *vm);
void InitStringBuiltins(qcvm_t *vm);
void InitMemBuiltins(qcvm_t *vm);
void qcvm_init_debug_builtins(qcvm_t *vm);
void InitMathBuiltins(qcvm_t *vm);

// exports from modules
vec_t frand();
vec_t frand(const vec_t max);
vec_t frand(const vec_t min, const vec_t max);

void SyncPlayerState(qcvm_t *vm, edict_t *ent);

#ifdef ALLOW_DEBUGGING
void qcvm_check_debugger_commands(qcvm_t *vm);
void qcvm_send_debugger_command(const qcvm_t *vm, const char *cmd);
void qcvm_wait_for_debugger_commands(qcvm_t *vm);
#endif
