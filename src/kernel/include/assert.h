#pragma once

/** @file
 * @brief Supply assertion support in-kernel.
 *
 * See \ref debug for more information. Assertions are replaced with no-ops if CONFIG_DEBUG is
 * disabled, thus you cannot put side-effects in an assert.
 */

#if CONFIG_DEBUG

#include <panic.h>

#define assert(cond)                                                                               \
	do {                                                                                           \
		if(!__builtin_expect(!!(cond), 0))                                                         \
			panic("assertion failure: %s", #cond);                                                 \
	} while(0)

#define assertmsg(cond, msg, ...)                                                                  \
	do {                                                                                           \
		if(!__builtin_expect(!!(cond), 0))                                                         \
			panic("assertion failure: %s -- " msg, #cond, ##__VA_ARGS__);                          \
	} while(0)

#else

/** Panic if condition is false.
 * @param cond The condition to check. Will be put into the panic message. */
#define assert(x) (void)(x)

/** Panic if condition is false, and print a printf-like message as part of the panic.
 * @param cond The condition to check. Will be put into the panic message.
 * @param msg Format string to append to the assert panic. Variable arguments after this parameter
 * are used in the format for the panic. */
#define assertmsg(x, m, ...) (void)(x)

#endif
