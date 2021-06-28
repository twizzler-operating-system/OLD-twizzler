list(APPEND KERNEL_SOURCES
	arch/x86_64/acpi.c
	arch/x86_64/debug.c
	arch/x86_64/entry.c
	arch/x86_64/gate.S
	arch/x86_64/hpet.c
	arch/x86_64/idt.c
	arch/x86_64/init.c
	arch/x86_64/interrupt.S
	arch/x86_64/ioapic.c
	arch/x86_64/kconf.c
	arch/x86_64/madt.c
	arch/x86_64/memory.c
	arch/x86_64/nfit.c
	arch/x86_64/objspace.c
	arch/x86_64/pit.c
	arch/x86_64/processor.c
	arch/x86_64/rdrand.c
	arch/x86_64/start.S
	arch/x86_64/table.c
	arch/x86_64/thread.c
	arch/x86_64/trampoline.S
	arch/x86_64/virtmem.c
	arch/x86_64/vmx.c
	arch/x86_64/x2apic.c
)

list(APPEND KERNEL_C_FLAGS
	"-mno-red-zone"
	"-mno-sse"
	"-mcmodel=kernel"
	"-mno-avx"
)

set(CRTI_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/arch/x86_64/crti.S)
set(CRTN_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/arch/x86_64/crtn.S)

set_source_files_properties(arch/x86_64/crti.S PROPERTIES COMPILE_FLAGS -DASSEMBLY)
set_source_files_properties(arch/x86_64/crtn.S PROPERTIES COMPILE_FLAGS -DASSEMBLY)
set_source_files_properties(arch/x86_64/interrupt.S PROPERTIES COMPILE_FLAGS -DASSEMBLY)
set_source_files_properties(arch/x86_64/start.S PROPERTIES COMPILE_FLAGS -DASSEMBLY)
set_source_files_properties(arch/x86_64/trampoline.S PROPERTIES COMPILE_FLAGS -DASSEMBLY)
set_source_files_properties(arch/x86_64/gate.S PROPERTIES COMPILE_FLAGS -DASSEMBLY)
