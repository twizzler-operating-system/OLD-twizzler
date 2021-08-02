use crate::arch::sys::raw_syscall;
use crate::obj::{objid_split, ObjID};

const SYS_ATTACH: i64 = 4;
const SYS_BECOME: i64 = 6;
const SYS_THREAD_SYNC: i64 = 7;
const SYS_KCONF: i64 = 14;
const SYS_THRD_CTL: i64 = 10;

pub(crate) const THRD_CTL_EXIT: i32 = 0x100;
pub fn thrd_ctl(op: i32, arg: u64) -> i64 {
	unsafe { raw_syscall(SYS_THRD_CTL, op as u64, arg, 0, 0, 0, 0) }
}

pub fn attach(pid: ObjID, cid: ObjID, flags: i32, ty: i32) -> i64 {
	let (pid_hi, pid_lo) = objid_split(pid);
	let (cid_hi, cid_lo) = objid_split(cid);
	let sf = (ty & 0xffff) as u64 | (flags as u64) << 32;
	unsafe { raw_syscall(SYS_ATTACH, pid_lo, pid_hi, cid_lo, cid_hi, sf, 0) }
}

pub const KCONF_RDRESET: u64 = 1;
pub const KCONF_ARCH_TSC_PSPERIOD: u64 = 1001;
pub fn kconf(cmd: u64, arg: u64) -> i64 {
	unsafe { raw_syscall(SYS_KCONF, cmd, arg, 0, 0, 0, 0) }
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

	let to = if let Some(timespec) = timespec {
		(&timespec) as *const KernelTimeSpec
	} else {
		std::ptr::null()
	};
	unsafe {
		raw_syscall(
			SYS_THREAD_SYNC,
			specs.len() as u64,
			specs.as_ptr() as u64,
			to as u64,
			0,
			0,
			0,
		)
	}
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

/* TODO: move this to arch-specific */
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
