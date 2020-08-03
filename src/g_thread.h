#pragma once

#ifdef ALLOW_DEBUGGING
// Wrapper for C++ threads. Blek.
#ifdef __cplusplus
extern "C"
{
#endif
	qcvm_mutex_t qcvm_cpp_create_mutex(void);
	void qcvm_cpp_free_mutex(qcvm_mutex_t);
	void qcvm_cpp_lock_mutex(qcvm_mutex_t);
	void qcvm_cpp_unlock_mutex(qcvm_mutex_t);
	qcvm_thread_t qcvm_cpp_create_thread(qcvm_thread_func_t);
	void qcvm_cpp_thread_sleep(const uint32_t);
#ifdef __cplusplus
};
#endif
#endif