extern "C"
{
	#include "shared/shared.h"
	#include "g_time.h"
};

#include <chrono>

using clk = std::chrono::high_resolution_clock;

vec_t qcvm_cpp_now(void)
{
	static auto start = clk::now();
	return std::chrono::duration<vec_t>(clk::now() - start).count();
}