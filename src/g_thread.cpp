extern "C"
{
	#define QCVM_INTERNAL
	#include "shared/shared.h"
	#include "game.h"
	#include "g_vm.h"
	#include "g_thread.h"
};

#ifdef ALLOW_DEBUGGING
#include <thread>
#include <mutex>
#include <chrono>

qcvm_mutex_t qcvm_cpp_create_mutex(void)
{
	return reinterpret_cast<qcvm_mutex_t>(new std::mutex);
}

void qcvm_cpp_free_mutex(qcvm_mutex_t mutex)
{
	delete reinterpret_cast<std::mutex *>(mutex);
}

void qcvm_cpp_lock_mutex(qcvm_mutex_t mutex)
{
	reinterpret_cast<std::mutex *>(mutex)->lock();
}

void qcvm_cpp_unlock_mutex(qcvm_mutex_t mutex)
{
	reinterpret_cast<std::mutex *>(mutex)->unlock();
}

qcvm_thread_t qcvm_cpp_create_thread(qcvm_thread_func_t func)
{
	return reinterpret_cast<qcvm_thread_t>(new std::thread(func));
}

void qcvm_cpp_thread_sleep(const uint32_t ms)
{
	std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}
#endif