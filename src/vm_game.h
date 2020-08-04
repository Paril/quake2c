#pragma once

#ifdef KMQUAKE2_ENGINE_MOD
extern bool is_kmq2_progs;
#endif

void qcvm_sync_player_state(qcvm_t *vm, edict_t *ent);

void qcvm_init_game_builtins(qcvm_t *vm);
