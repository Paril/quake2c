#pragma once

// used by multiple vm_ files
typedef struct
{
	int	msec;
	int	buttons;
	vec3_t angles;
	int	forwardmove, sidemove, upmove;
	int	impulse;		// remove?
	int	lightlevel;		// light level the player is standing on
} QC_usercmd_t;

// Builtin "modules"
void qcvm_init_gi_builtins(qcvm_t *vm);
void qcvm_init_game_builtins(qcvm_t *vm);
void qcvm_init_ext_builtins(qcvm_t *vm);
void qcvm_init_string_builtins(qcvm_t *vm);
void qcvm_init_mem_builtins(qcvm_t *vm);
void qcvm_init_debug_builtins(qcvm_t *vm);
void qcvm_init_math_builtins(qcvm_t *vm);

// exports from modules
void Q_srand(const uint32_t seed);
vec_t frand(void);
vec_t frand_m(const vec_t max);
vec_t frand_mm(const vec_t min, const vec_t max);

void SyncPlayerState(qcvm_t *vm, edict_t *ent);

// if you specify old_buffer, you are in charge of freeing it
char *qcvm_temp_buffer(const qcvm_t *vm, const size_t len);
char *qcvm_temp_format(const qcvm_t *vm, const char *format, ...);

#ifdef ALLOW_DEBUGGING
void qcvm_check_debugger_commands(qcvm_t *vm);
void qcvm_send_debugger_command(const qcvm_t *vm, const char *cmd);
void qcvm_wait_for_debugger_commands(qcvm_t *vm);
#endif
