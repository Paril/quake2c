#pragma once

extern qcvm_t *qvm;

// exported from QC
typedef struct
{
	int32_t		apiversion;
	int32_t		clientsize;
	
	qcvm_func_t		InitGame;
	qcvm_func_t		ShutdownGame;
	
	qcvm_func_t		PreSpawnEntities;
	qcvm_func_t		SpawnEntities;
	qcvm_func_t		PostSpawnEntities;
	
	qcvm_func_t		ClientConnect;
	qcvm_func_t		ClientBegin;
	qcvm_func_t		ClientUserinfoChanged;
	qcvm_func_t		ClientDisconnect;
	qcvm_func_t		ClientCommand;
	qcvm_func_t		ClientThink;

	qcvm_func_t		RunFrame;

	qcvm_func_t		ServerCommand;
	
	qcvm_func_t		PreWriteGame;
	qcvm_func_t		PostWriteGame;
	
	qcvm_func_t		PreReadGame;
	qcvm_func_t		PostReadGame;

	qcvm_func_t		PreWriteLevel;
	qcvm_func_t		PostWriteLevel;
	
	qcvm_func_t		PreReadLevel;
	qcvm_func_t		PostReadLevel;
} qc_export_t;

extern qc_export_t qce;

void RestoreClientData(void);
void AssignClientPointer(edict_t *e, const bool assign);
void WipeClientPointers(void);
void WipeEntities(void);
void BackupClientData(void);

// exported, but here to prevent warnings
game_export_t *GetGameAPI (game_import_t *import);