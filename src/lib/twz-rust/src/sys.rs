
const SYS_ATTACH: i64 = 4;
const SYS_BECOME: i64 = 6;

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
	pub rax: i64,
	pub rbx: i64,
	pub rcx: i64,
	pub rdx: i64,
	pub rdi: i64,
	pub rsi: i64,
	pub rsp: i64,
	pub rbp: i64,
	pub r8: i64,
	pub r9: i64,
	pub r10: i64,
	pub r11: i64,
	pub r12: i64,
	pub r13: i64,
	pub r14: i64,
	pub r15: i64,
	pub sctx_hint: u128,
}

pub unsafe fn r#become(args: *const BecomeArgs, arg0: i64, arg1: i64) -> Result<BecomeArgs, i64> {
    let mut num: i64 = SYS_BECOME;
    let mut rdi = std::mem::transmute::<*const BecomeArgs, i64>(args);
    let mut rsi = arg0;
    let mut rdx = arg1;
    let mut r8: i64;
    let mut r9: i64;
    let mut r10: i64;
    asm!("syscall",
         inout("rax") num,
         inout("rdi") rdi,
         inout("rsi") rsi,
         inout("rdx") rdx,
         out("r10") r10,
         out("r8") r8,
         out("r9") r9,
         out("r11") _,
         out("rcx") _);
    if num < 0 {
        return Err(num);
    }
    Ok(BecomeArgs {
        rdi: rdi,
        rsi: rsi,
        rdx: rdx,
        r10: r10,
        r8: r8,
        r9: r9,
        ..Default::default()
    })
}
