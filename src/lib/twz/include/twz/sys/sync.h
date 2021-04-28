#pragma once

#ifdef __cplusplus
#include <atomic>
using std::atomic_ulong;
extern "C" {
#endif

struct timespec;
int twz_thread_sync(int op, atomic_ulong *addr, uint64_t val, struct timespec *timeout);

struct sys_thread_sync_args;
void twz_thread_sync_init(struct sys_thread_sync_args *args,
  int op,
  atomic_ulong *addr,
  unsigned long val);

int twz_thread_sync32(int op, atomic_uint_least32_t *addr, uint32_t val, struct timespec *timeout);

int twz_thread_sync_multiple(size_t count, struct sys_thread_sync_args *, struct timespec *);

#ifdef __cplusplus
}
#endif
