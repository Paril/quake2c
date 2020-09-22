#pragma once

void qcvm_init_math_builtins(qcvm_t *vm);

void Q_srand(const uint32_t seed);
vec_t frand(void);
vec_t frand_m(const vec_t max);
vec_t frand_mm(const vec_t min, const vec_t max);
