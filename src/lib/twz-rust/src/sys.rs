const SYS_ATTACH: i64 = 4;
const SYS_BECOME: i64 = 6;
const SYS_THREAD_SYNC: i64 = 7;
const SYS_KCONF: i64 = 14;

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

pub const KCONF_RDRESET: u64 = 1;
pub const KCONF_ARCH_TSC_PSPERIOD: u64 = 1001;
pub fn kconf(cmd: u64, arg: u64) -> u64 {
	let mut num = SYS_KCONF;
	unsafe {
		asm!("syscall",
         inout("rax") num,
         in("rdi") cmd,
         in("rsi") arg,
         out("r11") _,
         out("rcx") _);
	}
	num as u64
}

#[allow(dead_code)]
pub struct ThreadSyncArgs {
	addr: *const std::sync::atomic::AtomicU64,
	arg: u64,
	res: u64,
	__resv: u64,
	op: u32,
	flags: u32,
}

impl ThreadSyncArgs {
	pub fn new_sleep(addr: &std::sync::atomic::AtomicU64, val: u64) -> ThreadSyncArgs {
		ThreadSyncArgs {
			addr: addr as *const std::sync::atomic::AtomicU64,
			arg: val,
			op: 0,
			flags: 0,
			__resv: 0,
			res: 0,
		}
	}

	pub fn new_wake(addr: &std::sync::atomic::AtomicU64, count: u64) -> ThreadSyncArgs {
		ThreadSyncArgs {
			addr: addr as *const std::sync::atomic::AtomicU64,
			arg: count,
			op: 1,
			flags: 0,
			__resv: 0,
			res: 0,
		}
	}
}

#[repr(C)]
pub struct KernelTimeSpec {
	pub sec: u64,
	pub nsec: u64,
}

impl From<std::time::Duration> for KernelTimeSpec {
	fn from(dur: std::time::Duration) -> KernelTimeSpec {
		KernelTimeSpec {
			sec: dur.as_secs(),
			nsec: dur.subsec_nanos() as u64,
		}
	}
}

pub fn thread_sync(specs: &mut [ThreadSyncArgs], timeout: Option<std::time::Duration>) -> i64 {
	let timespec: Option<KernelTimeSpec> = timeout.map(|t| t.into());

	let mut num = SYS_THREAD_SYNC;
	unsafe {
		asm!("syscall",
         inout("rax") num,
         in("rdi") specs.len(),
         in("rsi") specs.as_ptr(),
         in("rdx") if let Some(timespec) = timespec { (&timespec) as *const KernelTimeSpec } else { std::ptr::null() },
         out("r11") _,
         out("rcx") _);
	}
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
