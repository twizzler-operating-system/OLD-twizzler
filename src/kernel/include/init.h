#pragma once

/** @file
 * @brief Kernel initialization routines. See also the __initializer macros in \ref
 * include/system.h. */

/*! \cond PRIVATE */
#include <system.h>
struct init_call {
	void (*fn)(void *);
	void *data;
	struct init_call *next;
	const char *file;
	int line;
	bool allcpus;
};

#define __late_init_arg(c, f, a)                                                                   \
	static inline void __initializer __concat(__reg_post_init, __COUNTER__)(void)                  \
	{                                                                                              \
		static struct init_call call;                                                              \
		post_init_call_register(&call, c, f, a, __FILE__, __LINE__);                               \
	}

#define __late_init(c, f) __late_init_arg(c, f, NULL)

#define __late_init_arg_ordered(c, p, f, a)                                                        \
	static inline void __orderedinitializer(p) __concat(__reg_post_init, __COUNTER__)(void)        \
	{                                                                                              \
		static struct init_call call;                                                              \
		post_init_call_register(&call, c, f, a, __FILE__, __LINE__);                               \
	}

#define __late_init_ordered(c, p, f) __late_init_arg_ordered(c, p, f, NULL)
/*! \endcond */

/** Call a function during late-init phase of kernel startup (see \ref kernelinit).
 * First argument is required, and specifies the function to call back. Second argument is optional,
 * and specifies a void * data argument (NULL if omitted). The function should have signature void
 * (*)(void *). */
#define POST_INIT(...) __get_macro2(__VA_ARGS__, __late_init_arg, __late_init)(false, __VA_ARGS__)

/** Similar to POST_INIT, but also specifies a priority (see \ref kernelinit). */
#define POST_INIT_ORDERED(pri, ...)                                                                \
	__get_macro2(__VA_ARGS__, __late_init_arg_ordered, __late_init_ordered)(false, pri, __VA_ARGS__)

/** Similar to POST_INIT, but all CPUs will run this function concurrently. */
#define POST_INIT_ALLCPUS(...)                                                                     \
	__get_macro2(__VA_ARGS__, __late_init_arg, __late_init)(true, __VA_ARGS__)

/** Similar to POST_INIT_ORDERED, but all CPUs will run this function concurrently. */
#define POST_INIT_ORDERED_ALLCPUS(pri, ...)                                                        \
	__get_macro2(__VA_ARGS__, __late_init_arg_ordered, __late_init_ordered)(true, pri, __VA_ARGS__)

/** Register a late-init call to run.
 * @param ic a struct init_call to use to register this function. Must have static scope.
 * @param allc if true, all CPUs will run this call.
 * @param fn the function to call-back.
 * @param data an argument passed to fn.
 * @param the file name registering this call.
 * @param the line number registering this call.
 */
void post_init_call_register(struct init_call *ic,
  bool allc,
  void (*fn)(void *),
  void *data,
  const char *file,
  int line);

/** The entry point into higher-level kernel initialization routines. This is called by
 * architecture-specific code once it has setup the basic CPU state. Before calling this, the memory
 * management system is still in bootstrapping mode, and global constructors have NOT been run.
 * After calling this function, global constructors will have been run, and the memory manager will
 * be fully bootstrapped and ready. The function also boots up all secondary processors. */
void kernel_init(void);
struct processor;
/** Main kernel entry point. All processors (including BSP and secondary) call into this function
 * once they are ready. Threading is not yet setup, though, so it is possible that current_thread is
 * still NULL here. The BSP will then initialize the init thread, and all other CPUs will just call
 * thread_resume* to wait for scheduling. */
void kernel_main(struct processor *proc);
