/*
 * SPDX-FileCopyrightText: 2021 Daniel Bittman <danielbittman1@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch/secctx.h>
#include <arch/x86_64-msr.h>
#include <clksrc.h>
#include <processor.h>
#include <vmm.h>
void arch_processor_reset_current_thread(struct processor *proc)
{
	proc->arch.curr = NULL;
	x86_64_secctx_switch(NULL);
	arch_mm_switch_context(NULL);
}

void arch_processor_enumerate()
{
	/* this is handled by initializers in madt.c */
}

static _Alignas(16) struct {
	uint16_t limit;
	uint64_t base;
} __attribute__((packed)) idt_ptr;

void arch_processor_reset(void)
{
	idt_ptr.limit = 0;
	asm volatile("lidt (%0)" ::"r"(&idt_ptr) : "memory");
	asm volatile("int $3");
}

#define MSR_IA32_TME_ACTIVATE 0x982
size_t arch_processor_physical_width(void)
{
	static size_t physical_address_bits = 0;
	if(!physical_address_bits) {
		uint32_t a, b, c, d;
		if(!__get_cpuid_count(0x80000008, 0, &a, &b, &c, &d))
			panic("unable to determine physical address size");
		physical_address_bits = a & 0xff;
	}
	return physical_address_bits - 1;
}

size_t arch_processor_virtual_width(void)
{
	static size_t virtual_address_bits = 0;
	if(!virtual_address_bits) {
		uint32_t a, b, c, d;
		if(!__get_cpuid_count(0x80000008, 0, &a, &b, &c, &d))
			panic("unable to determine physical address size");
		virtual_address_bits = (a >> 8) & 0xff;
	}
	return virtual_address_bits - 1;
}

void arch_processor_scheduler_wakeup(struct processor *proc)
{
	proc->flags |= PROCESSOR_HASWORK;
	if(!(x86_features.features & X86_FEATURE_MWAIT)) {
		/* if we don't have mwait, the sleeping processor is in HLT loop, which means we must send
		 * it an IPI to wake it. */
		/* TODO: check if processor is halted */
		// if(proc->flags & PROCESSOR_HALTING) {
		processor_send_ipi(proc->id, PROCESSOR_IPI_RESUME, NULL, PROCESSOR_IPI_NOWAIT);
		//}
	}
}

__noinstrument void arch_processor_halt(struct processor *proc)
{
	if(x86_features.features & X86_FEATURE_MWAIT) {
		long mw = 0;
		if(proc->arch.mwait_info++ > 1) {
			mw = 0x20;
		} else {
			clksrc_set_interrupt_countdown(100000, false);
		}

		if(proc->flags & PROCESSOR_HASWORK)
			goto wakeup;

		asm volatile("mfence; clflush 0(%0); mfence" ::"r"(&proc->flags) : "memory");
		asm volatile("monitor; mfence" ::"a"(&proc->flags), "d"(0ul), "c"(0ul) : "memory");

		if(proc->flags & PROCESSOR_HASWORK)
			goto wakeup;

		asm volatile("mwait" ::"a"(mw), "c"(0x1) : "memory");
	} else {
		int tries = 1000 /* TODO (major): calibrate this */;
		while(--tries > 0) {
			if(proc->flags & PROCESSOR_HASWORK)
				break;
			asm volatile("pause");
		}
		if(tries == 0) {
			proc->flags |= PROCESSOR_HALTING;
			if(atomic_fetch_and(&proc->flags, ~PROCESSOR_HASWORK) & PROCESSOR_HASWORK) {
				proc->flags &= ~PROCESSOR_HALTING;
				return;
			}
			asm volatile("sti; hlt");
		}
	}

wakeup:
	if(!(atomic_fetch_and(&proc->flags, ~PROCESSOR_HASWORK) & PROCESSOR_HASWORK)) {
		proc->arch.mwait_info++;
	} else {
		proc->arch.mwait_info = 0;
	}
	proc->flags &= ~(PROCESSOR_HASWORK | PROCESSOR_HALTING);

	// asm volatile("sti; hlt");
	// for(long i = 0; i < 10000000; i++) {
	//	asm volatile("pause");
	//}
}
