#pragma once
#include <assert.h>
#include <elf.h>
#include <interrupt.h>
#include <panic.h>
void kernel_debug_entry(void);

#if FEATURE_SUPPORTED_UNWIND
struct frame {
	uintptr_t pc, fp;
};
void debug_print_backtrace(void);
bool arch_debug_unwind_frame(struct frame *frame, bool);
void debug_puts(char *);
bool debug_process_input(unsigned int c);

#endif

#if CONFIG_INSTRUMENT
void kernel_instrument_start(void);
#endif

void debug_elf_register_sections(Elf64_Shdr *sections, size_t num, size_t entsize, size_t stridx);
const char *debug_symbolize(void *addr);
void debug_print_backtrace_userspace(void);
