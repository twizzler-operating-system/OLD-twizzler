pub(crate) unsafe fn raw_syscall(num: i64, arg0: u64, arg1: u64, arg2: u64, arg3: u64, arg4: u64, arg5: u64) -> i64 {
	let mut num = num;
	asm!("syscall",
        inout("rax") num,
        in("rdi") arg0,
        in("rsi") arg1,
        in("rdx") arg2,
        in("r8") arg3,
        in("r9") arg4,
        in("r10") arg5,
        out("r11") _,
        out("rcx") _);
	num
}
