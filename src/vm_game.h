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
void InitGIBuiltins(QCVM &vm);
void InitGameBuiltins(QCVM &vm);
void InitExtBuiltins(QCVM &vm);
void InitStringBuiltins(QCVM &vm);
void InitMemBuiltins(QCVM &vm);
void InitDebugBuiltins(QCVM &vm);
void InitMathBuiltins(QCVM &vm);

// exports from modules
float frand();
float frand(const float &max);
float frand(const float &min, const float &max);

void SyncPlayerState(QCVM &vm, edict_t *ent);

#ifdef ALLOW_DEBUGGING
void CheckDebuggerCommands();
void SendDebuggerCommand(const std::string &cmd);
void WaitForDebuggerCommands();
#endif
