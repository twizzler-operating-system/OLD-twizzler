#[repr(C)]
struct FaultFrame {
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
fn __twz_fault_upcall_entry_rust(fid: i32, info: *mut std::ffi::c_void, _frame: FaultFrame)
{
    crate::fault::__twz_fault_handler(fid, info, std::ptr::null_mut());
}

#[naked]
pub extern fn __twz_fault_upcall_entry() {
    unsafe {
        /* The kernel will enter our upcall handler here when passing us a fault. We need to
         * preseve our registers from whereever we _were_ executing. Additionally, the kernel will
         * have subtracted a significant amount off the stack pointer to get us to this point
         * (becuase of the red zone), so we'll need to adjust for the old stack frame being 128 +
         * mumble bytes above what we're given here. The CFI directives are for exception
         * unwinding, allowing the unwinder to find frame info. */
        #[inline(never)]
        asm!(".cfi_endproc",
             ".cfi_startproc",
             ".cfi_def_cfa rbp, 144",
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
             "call __twz_fault_upcall_entry_rust",
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
             "sub rsp, 144",
             "pop rbp",
             "add rsp, 136",
             "jmp -136[rsp]",
             options(noreturn));
    }
}
