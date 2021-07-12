#pragma once
#include <stdarg.h>

/** @file
 * @brief Kernel logging support. This file is automatically included.
 */

/** Log a message, using format string 'fmt' with printf-like semantics. */
__attribute__((format(printf, 1, 2))) int printk(const char *fmt, ...);

/** Similar to \ref printk, except pass a va_list directly. */
int vprintk(const char *fmt, va_list args);

#define PR128FMT "%#0lx%#016lx"
#define PR128FMTd "%#0lx:%#016lx"

#define PR128(x) (uint64_t)(x >> 64), (uint64_t)x
