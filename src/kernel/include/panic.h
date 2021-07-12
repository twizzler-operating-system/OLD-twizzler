#pragma once
#define PANIC_UNWIND 0x1
#define PANIC_CONTINUE 0x2

/** Panic. Stops the world, prints debug messages, and halts the system. If flags sets PANIC_UNWIND,
 * print a stack trace. If flags sets PANIC_CONTINUE, then do not halt the system, and continue
 * after printing messages. */
__attribute__((format(printf, 4, 5))) void __panic(const char *file,
  int linenr,
  int flags,
  const char *msg,
  ...);

/** Standard panic. Prints stack trace and stops the world. */
#define panic(msg, ...)                                                                            \
	({                                                                                             \
		__panic(__FILE__, __LINE__, PANIC_UNWIND, msg, ##__VA_ARGS__);                             \
		__builtin_unreachable();                                                                   \
	})

/** Continue panic. Prints stack trace and continues execution. */
#define panic_continue(msg, ...)                                                                   \
	__panic(__FILE__, __LINE__, PANIC_UNWIND | PANIC_CONTINUE, msg, ##__VA_ARGS__)
