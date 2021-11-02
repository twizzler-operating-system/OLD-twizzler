#pragma once
#include <stdarg.h>
#include <stddef.h>

__attribute__((format(printf, 2, 3))) void _do_printk(int, const char *fmt, ...);
void _do_vprintk(int, const char *fmt, va_list args);
int vsnprintf(char *buf, size_t len, const char *fmt, va_list args);

#define PR128FMT "%#0lx%#016lx"
#define PR128FMTd "%#0lx:%#016lx"

#define PR128(x) (uint64_t)(x >> 64), (uint64_t)x

#define printk(...) _do_printk(0, __VA_ARGS__)

#define eprintk(...) _do_printk(1, __VA_ARGS__)

#define vprintk(f, a) _do_vprintk(0, f, a)

#define evprintk(f, a) _do_vprintk(1, f, a)
