#pragma once
#define X86_MSR_APIC_BASE 0x1B
#define X86_MSR_APIC_BASE_BSP 0x100
#define X86_MSR_APIC_BASE_X2MODE 0x400
#define X86_MSR_APIC_BASE_ENABLE 0x800

#define X86_MSR_FS_BASE 0xc0000100
#define X86_MSR_GS_BASE 0xc0000101
#define X86_MSR_KERNEL_GS_BASE 0xc0000102
#define X86_MSR_EFER 0xc0000080
#define X86_MSR_EFER_SYSCALL 0x1
#define X86_MSR_EFER_NX (1 << 11)

#define X86_MSR_VMX_TRUE_ENTRY_CTLS 0x490
#define X86_MSR_VMX_ENTRY_CTLS 0x484
#define X86_MSR_VMX_TRUE_EXIT_CTLS 0x48f
#define X86_MSR_VMX_EXIT_CTLS 0x483

#define X86_MSR_STAR 0xC0000081
#define X86_MSR_LSTAR 0xC0000082
#define X86_MSR_SFMASK 0xC0000084

#define X86_MSR_FEATURE_CONTROL 0x3A

#define X86_MSR_VMX_BASIC 0x480

#define X86_MSR_VMX_PROCBASED_CTLS2 0x48b /* does not have a "true" variant */
#define X86_MSR_VMX_PROCBASED_CTLS 0x482
#define X86_MSR_VMX_TRUE_PROCBASED_CTLS 0x48e
#define X86_MSR_VMX_PINBASED_CTLS 0x481
#define X86_MSR_VMX_TRUE_PINBASED_CTLS 0x48D

#define X86_MSR_VMX_EPT_VPID_CAP 0x48C

#define X86_MSR_VMX_VMFUNC 0x491

#define X86_MSR_MTRRCAP 0xFE
#define X86_MSR_MTRR_DEF_TYPE 0x2FF
#define X86_MSR_MTRR_PHYSBASE(n) (0x200 + 2 * (n))
#define X86_MSR_MTRR_PHYSMASK(n) (0x200 + 2 * (n) + 1)

#define X86_MSR_TSC_DEADLINE 0x6E0

#define X86_MSR_PKG_CST_CONFIG_CONTROL 0xe2
#define X86_MSR_POWER_CTL 0x1fc

#define X86_MSR_MISC_ENABLE 0x1a0

__noinstrument static inline void x86_64_rdmsr(uint32_t msr, uint32_t *lo, uint32_t *hi)
{
	asm volatile("rdmsr" : "=a"(*lo), "=d"(*hi) : "c"(msr));
}

__noinstrument static inline void x86_64_wrmsr(uint32_t msr, uint32_t lo, uint32_t hi)
{
	asm volatile("wrmsr" : : "a"(lo), "d"(hi), "c"(msr));
}
