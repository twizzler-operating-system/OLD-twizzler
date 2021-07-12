#pragma once

/** @file
 * @brief Some general utility functions.
 *
 * This file is always included.
 */

#ifndef ASSEMBLY

#include <err.h>
#include <printk.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define likely(x) __builtin_expect(x, 1)
#define unlikely(x) __builtin_expect(x, 0)

typedef long ssize_t;
typedef unsigned __int128 uint128_t;

/** Round up a value to the nearest power of 2 */
static inline unsigned long long __round_up_pow2(unsigned int a)
{
	return ((a & (a - 1)) == 0) ? a : 1ull << (sizeof(a) * 8 - __builtin_clz(a));
}

#define flag_if_notzero(f, x) ({ (f) ? x : 0; })

#define align_down(x, s) ({ (x) & ~(s - 1); })

#define align_up(x, s)                                                                             \
	({                                                                                             \
		typeof(x) __y = (x);                                                                       \
		size_t __sz = (s);                                                                         \
		((__y - 1) & ~(__sz - 1)) + __sz;                                                          \
	})

#define is_aligned(x, s) ({ (uintptr_t)(x) % (uintptr_t)(s) == 0; })

#define __orderedbefore(x) (x - 1)
#define __orderedafter(x) (x + 1)

/** Declare a function as a global constructor (and will be executed when the kernel runs global
 * constructors, see \ref kernelinit). The order of this global constructor relative to others is
 * not defined. */
#define __initializer __attribute__((used, constructor, no_instrument_function))

/** Similar to \ref __initializer, but defines an order. Functions registered with a smaller number
 * 'x' will run first. Functions registered with the same value of 'x' will not have a defined
 * order.
 *
 * Generally, the system will declare a number of major ordering values that can be used. For
 * example, say there is a constructor that must run after the ACPI tables are setup. The system
 * might define a ACPI_INITIALIZER_ORDER define so that other functions could be declared with
 *     __orderedinitializer(__orderedafter(ACPI_INITIALIZER_ORDER)) static void _init() {...}
 * and thus will run after the ACPI_INITIALIZER_ORDER point. If a function needs to be run after
 * multiple of these points, they can be summed:
 *     __orderedafter(ACPI_INITIALIZER_ORDER + IOAPIC_INITIALIZED_ORDER)
 */
#define __orderedinitializer(x) __attribute__((used, constructor(x + 3000), no_instrument_function))

#define ___concat(x, y) x##y
#define __concat(x, y) ___concat(x, y)

#define __get_macro2(_1, _2, NAME, ...) NAME

#define stringify_define(x) stringify(x)
#define stringify(x) #x

#define array_len(x) (sizeof((x)) / sizeof((x)[0]))

#define container_of(ptr, type, member)                                                            \
	({                                                                                             \
		const typeof(((type *)0)->member) *__mptr = (ptr);                                         \
		(type *)((char *)__mptr - offsetof(type, member));                                         \
	})

#define __unused __attribute__((unused))
#define __packed __attribute__((packed))

#define __noinstrument __attribute__((no_instrument_function))

#define READ 0
#define WRITE 1

/** See the C library function */
long strtol(char *str, char **end, int base);
#endif
