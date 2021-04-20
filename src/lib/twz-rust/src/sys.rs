
const SYS_ATTACH: i32 = 4;
const SYS_BECOME: i32 = 6;

pub unsafe fn attach(pid: u128, cid: u128, flags: i32, ty: i32) -> i64 {
    let mut num = SYS_ATTACH;
	let sf: u64 = (ty & 0xffff) as u64 | (flags as u64) << 32;
    asm!("syscall",
         inout("rax") num,
         in("rdi") (pid & 0xffffffffffffffff) as u64,
         in("rsi") ((pid >> 64) & 0xffffffffffffffff) as u64,
         in("rdx") (cid & 0xffffffffffffffff) as u64,
         in("r8") ((cid >> 64) & 0xffffffffffffffff) as u64,
         in("r9") sf,
         out("r11") _,
         out("rcx") _);
    num as i64
}

#[derive(Default)]
#[repr(C)]
pub struct BecomeArgs {
	pub target_view: u128,
	pub target_rip: u64,
	pub rax: u64,
	pub rbx: u64,
	pub rcx: u64,
	pub rdx: u64,
	pub rdi: u64,
	pub rsi: u64,
	pub rsp: u64,
	pub rbp: u64,
	pub r8: u64,
	pub r9: u64,
	pub r10: u64,
	pub r11: u64,
	pub r12: u64,
	pub r13: u64,
	pub r14: u64,
	pub r15: u64,
	pub sctx_hint: u128,
}

pub unsafe fn r#become(args: *const BecomeArgs, arg0: i64, arg1: i64) -> i64 {
    let mut num = SYS_BECOME;
    asm!("syscall",
         inout("rax") num,
         in("rdi") args,
         in("rsi") arg0,
         in("rdx") arg1,
         out("r11") _,
         out("rcx") _);
    num as i64
}
