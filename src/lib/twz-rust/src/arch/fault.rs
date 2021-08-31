#[repr(C)]
pub struct FaultFrame {
	flags: u64,
	pad: u64,
	r15: u64,
	r14: u64,
	r13: u64,
	r12: u64,
	r11: u64,
	r10: u64,
	r9: u64,
	r8: u64,
	rbp: u64,
	rdx: u64,
	rcx: u64,
	rbx: u64,
	rax: u64,
	rdi: u64,
	rsi: u64,
	rsp: u64,
}

#[no_mangle]
#[link_section = ".text.keep"]
pub extern "C" fn __twz_fault_upcall_entry_rust(fid: i32, info: *mut std::ffi::c_void, _frame: FaultFrame) {
	crate::fault::__twz_fault_handler(fid, info);
}

#[used]
pub static UPCALL_KEEP: extern "C" fn() = __twz_fault_upcall_entry;

#[used]
pub static UPCALL_KEEP2: extern "C" fn(i32, *mut std::ffi::c_void, FaultFrame) = __twz_fault_upcall_entry_rust;

// 144 (exception?) 160 (obj)
#[link_section = ".text.keep"]
#[no_mangle]
#[naked]
pub extern "C" fn __twz_fault_upcall_entry() {
	unsafe {
		/* The kernel will enter our upcall handler here when passing us a fault. We need to
		 * preseve our registers from whereever we _were_ executing. Additionally, the kernel will
		 * have subtracted a significant amount off the stack pointer to get us to this point
		 * (becuase of the red zone), so we'll need to adjust for the old stack frame being 128 +
		 * mumble bytes above what we're given here. The CFI directives are for exception
		 * unwinding, allowing the unwinder to find frame info. */
		#[inline(never)]
		asm!(
			".cfi_signal_frame",
			".cfi_def_cfa rbp, 144",
			".cfi_offset rbp, -144",
			".cfi_offset rip, -136",
			".cfi_return_column rip",
			"push rax",
			"push rbx",
			"push rcx",
			"push rdx",
			"push rbp",
			"push r8",
			"push r9",
			"push r10",
			"push r11",
			"push r12",
			"push r13",
			"push r14",
			"push r15",
			"push r15",
			"pushf",
			"mov rdx, rsp",
			"mov rbx, rsp",
			"and rsp, 0xfffffffffffffff0",
			"call __twz_fault_upcall_entry_rust",
			"mov rsp, rbx",
			"popf",
			"pop r15",
			"pop r15",
			"pop r14",
			"pop r13",
			"pop r12",
			"pop r11",
			"pop r10",
			"pop r9",
			"pop r8",
			"pop rbp",
			"pop rdx",
			"pop rcx",
			"pop rbx",
			"pop rax",
			"pop rdi",
			"pop rsi",
			"pop rsp",
			".cfi_def_cfa rsp, 0",
			"sub rsp, 144",
			".cfi_def_cfa rsp, 144",
			"pop rbp",
			".cfi_def_cfa rsp, 136",
			"add rsp, 136",
			".cfi_def_cfa rsp, 0",
			"jmp -136[rsp]",
			options(noreturn)
		);
	}
}
