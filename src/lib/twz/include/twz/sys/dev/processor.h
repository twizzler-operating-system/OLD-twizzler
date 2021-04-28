#pragma once

#ifdef __cplusplus
#include <atomic>
extern "C" {
#endif

struct proc_stats {
#ifdef __cplusplus
	std::atomic_uint_least64_t thr_switch;
	std::atomic_uint_least64_t syscalls;
	std::atomic_uint_least64_t sctx_switch;
	std::atomic_uint_least64_t ext_intr;
	std::atomic_uint_least64_t int_intr;
	std::atomic_uint_least64_t running;
	std::atomic_uint_least64_t shootdowns;
#else
	_Atomic uint64_t thr_switch;
	_Atomic uint64_t syscalls;
	_Atomic uint64_t sctx_switch;
	_Atomic uint64_t ext_intr;
	_Atomic uint64_t int_intr;
	_Atomic uint64_t running;
	_Atomic uint64_t shootdowns;
#endif
};

struct processor_header {
	struct proc_stats stats;
};

#ifdef __cplusplus
}
#endif
