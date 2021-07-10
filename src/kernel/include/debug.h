#pragma once
/** @file
 * @brief Debugging services.
 *
 * See \ref debug for overall debugging support.
 */

#include <assert.h>
#include <elf.h>
#include <interrupt.h>
#include <panic.h>

/** Enter the kernel debugger due to a fatal error. */
void kernel_debug_entry(void);

/** A callframe */
struct frame {
	/** The program counter for this frame */
	uintptr_t pc;
	/** The frame pointer for this frame */
	uintptr_t fp;
};

/** Print a backtrace of kernel calls via printk. */
void debug_print_backtrace(void);
/** Print a backtrace of userspace that led to kernel entry via printk. */
void debug_print_backtrace_userspace(void);
/** Unwind a single frame, updating the frame parameter. This is architecture-specific.
 * @param frame The current frame.
 * @param[out] frame The next frame.
 * @param userspace Are we unwinding in userspace or not.
 * @return Is the next frame valid?
 */
bool arch_debug_unwind_frame(struct frame *frame, bool userspace);
/** Put a single string onto the debug console. */
void debug_puts(char *);
/** Send a character input to the debugger. */
bool debug_process_input(unsigned int c);

/** Register the location of the kernel's ELF section headers loaded by the bootloader.
 * @param sections A virtual pointer to the ELF section headers.
 * @param num The number of section headers pointed to by sections.
 * @param entsize The size of a section header.
 * @param stridx The index into sections that contains the string table for section names.
 */
void debug_elf_register_sections(Elf64_Shdr *sections, size_t num, size_t entsize, size_t stridx);

/** Get a name associated with an address in the kernel executable. This will search the symbols
 * using the ELF sections setup by debug_elf_register_sections.
 *
 * @param addr The address to lookup.
 * @return The symbol name (C string).
 */
const char *debug_symbolize(void *addr);
