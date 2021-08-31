use crate::obj::ObjID;
use std::fmt;
#[repr(i32)]
#[derive(Copy, Clone, PartialEq, Debug)]
enum FaultTypeID {
	Object = 0,
	Null = 1,
	Exception = 2,
	Sctx = 3,
	Double = 4,
	Page = 5,
	Pptr = 6,
	Signal = 7,
}

use std::convert::TryInto;
impl std::convert::TryFrom<i32> for FaultTypeID {
	type Error = i32;
	fn try_from(val: i32) -> Result<Self, Self::Error> {
		match val {
			0 => Ok(FaultTypeID::Object),
			1 => Ok(FaultTypeID::Null),
			2 => Ok(FaultTypeID::Exception),
			3 => Ok(FaultTypeID::Sctx),
			4 => Ok(FaultTypeID::Double),
			5 => Ok(FaultTypeID::Page),
			6 => Ok(FaultTypeID::Pptr),
			7 => Ok(FaultTypeID::Signal),
			_ => Err(val),
		}
	}
}

#[derive(Copy, Clone, Debug)]
#[repr(C)]
struct SignalInfo {
	args: [u64; 4],
}

#[derive(Copy, Clone)]
#[repr(C)]
struct FaultInfo {
	view: ObjID,
	addr: u64,
	flags: u64,
}

impl std::fmt::Debug for FaultInfo {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
		write!(
			f,
			"FaultInfo {{ view: {:x}, addr: {:x}, flags: {:x} }}",
			self.view, self.addr, self.flags
		)
	}
}

const OBJECT_READ: u64 = 1;
const OBJECT_WRITE: u64 = 2;
const OBJECT_EXEC: u64 = 4;
const OBJECT_NOMAP: u64 = 8;
const OBJECT_EXIST: u64 = 16;
const OBJECT_INVALID: u64 = 32;
const OBJECT_UNKNOWN: u64 = 64;
const OBJECT_UNSIZED: u64 = 128;

#[derive(Copy, Clone)]
#[repr(C)]
struct ObjectInfo {
	id: ObjID,
	ip: u64,
	addr: u64,
	flags: u64,
	pad: u64,
}

macro_rules! flag_print {
	( $fm:expr, $fl:expr, $n:expr, $first:expr ) => {
		if $fl & $n > 0 {
			if !$first {
				write!($fm, " | ")?;
			}
			$first = false;
			write!($fm, stringify!($n))
		} else {
			Ok(())
		}
	};
}

impl std::fmt::Debug for ObjectInfo {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
		write!(
			f,
			"ObjectInfo {{ id: {:x}, ip: {:x}, addr: {:x}, flags: ",
			self.id, self.ip, self.addr
		)?;
		let mut first = true;
		flag_print!(f, self.flags, OBJECT_READ, first)?;
		flag_print!(f, self.flags, OBJECT_WRITE, first)?;
		flag_print!(f, self.flags, OBJECT_EXEC, first)?;
		flag_print!(f, self.flags, OBJECT_NOMAP, first)?;
		flag_print!(f, self.flags, OBJECT_EXIST, first)?;
		flag_print!(f, self.flags, OBJECT_INVALID, first)?;
		flag_print!(f, self.flags, OBJECT_UNKNOWN, first)?;
		#[allow(unused_assignments)]
		flag_print!(f, self.flags, OBJECT_UNSIZED, first)?;
		Ok(())
	}
}

#[derive(Copy, Clone)]
#[repr(C)]
struct NullInfo {
	ip: u64,
	addr: u64,
}

impl std::fmt::Debug for NullInfo {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
		write!(f, "NullInfo {{ ip: {:x}, addr: {:x} }}", self.ip, self.addr)
	}
}

#[derive(Copy, Clone)]
#[repr(C)]
struct ExceptionInfo {
	ip: u64,
	code: u64,
	arg: u64,
	pad: u64,
}

impl std::fmt::Debug for ExceptionInfo {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
		write!(
			f,
			"ExceptionInfo {{ ip: {:x}, code: {}, arg: {:x} }}",
			self.ip, self.code, self.arg
		)
	}
}

#[derive(Copy, Clone)]
#[repr(C)]
struct SctxInfo {
	target: ObjID,
	ip: u64,
	addr: u64,
	pneed: u32,
	pad: u32,
	pad2: u64,
}

impl std::fmt::Debug for SctxInfo {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
		write!(
			f,
			"SctxInfo {{ target: {:x}, ip: {:x}, addr: {:x}, pneed: {:x} }}",
			self.target, self.ip, self.addr, self.pneed
		)
	}
}

#[derive(Copy, Clone, Debug)]
#[repr(C)]
struct DoubleFaultInfo<T> {
	fault: u32,
	info: u32,
	len: u32,
	resv: u32,
	data: T,
}

#[derive(Copy, Clone)]
#[repr(C)]
struct PageInfo {
	id: ObjID,
	addr: u64,
	pgnr: u64,
	info: u64,
	ip: u64,
}

impl std::fmt::Debug for PageInfo {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
		write!(
			f,
			"PageInfo {{ id: {:x}, pgnr: {}, ip: {:x}, addr: {:x}, info: {:x} }}",
			self.id, self.pgnr, self.ip, self.addr, self.info
		)
	}
}

#[derive(Copy, Clone, Debug)]
#[repr(i32)]
enum PptrType {
	Unknown = 0,
	Invalid = 1,
	Resolve = 2,
	Resources = 3,
	Derive = 4,
}

#[derive(Copy, Clone)]
#[repr(C)]
struct PptrInfo {
	id: ObjID,
	fot_entry: u64,
	ip: u64,
	info: u32,
	retval: u32,
	flags: u64,
	name: *const u8,
	ptr: u64,
}

impl std::fmt::Debug for PptrInfo {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
		let info = match self.info {
			0 => PptrType::Unknown,
			1 => PptrType::Invalid,
			2 => PptrType::Resolve,
			3 => PptrType::Resources,
			4 => PptrType::Derive,
			_ => panic!("encountered unexpected PptrType {}", self.info),
		};
		write!(f, "PptrInfo {{ id: {:x}, fot_entry: {}, ip: {:x}, info: {:?}, retval: {}, flags: {:x}, name: {:p}, ptr: {:x} }}",
               self.id, self.fot_entry, self.ip, info, self.retval, self.flags, self.name, self.ptr)
	}
}

#[derive(Debug)]
enum Fault {
	Object(ObjectInfo),
	Null(NullInfo),
	Exception(ExceptionInfo),
	Sctx(SctxInfo),
	Page(PageInfo),
	Pptr(PptrInfo),
	Signal(SignalInfo),
}

fn catching_fault_handler(fid: i32, info: *mut std::ffi::c_void) {
	let fault = unsafe {
		match fid.try_into() {
			Ok(FaultTypeID::Object) => {
				Fault::Object(*(std::mem::transmute::<*mut std::ffi::c_void, &ObjectInfo>(info)))
			}
			Ok(FaultTypeID::Null) => Fault::Null(*(std::mem::transmute::<*mut std::ffi::c_void, &NullInfo>(info))),
			Ok(FaultTypeID::Exception) => {
				Fault::Exception(*(std::mem::transmute::<*mut std::ffi::c_void, &ExceptionInfo>(info)))
			}
			Ok(FaultTypeID::Sctx) => Fault::Sctx(*(std::mem::transmute::<*mut std::ffi::c_void, &SctxInfo>(info))),
			Ok(FaultTypeID::Page) => Fault::Page(*(std::mem::transmute::<*mut std::ffi::c_void, &PageInfo>(info))),
			Ok(FaultTypeID::Pptr) => Fault::Pptr(*(std::mem::transmute::<*mut std::ffi::c_void, &PptrInfo>(info))),
			Ok(FaultTypeID::Signal) => {
				Fault::Signal(*(std::mem::transmute::<*mut std::ffi::c_void, &SignalInfo>(info)))
			}
			Ok(FaultTypeID::Double) => {
				let subfault_tmp = std::mem::transmute::<*mut std::ffi::c_void, &DoubleFaultInfo<()>>(info);
				if (subfault_tmp.fault as i32).try_into() == Ok(FaultTypeID::Double) {
					panic!("encountered double fault while handing double fault");
				}
				panic!("unhandled upcall double fault {:?}", subfault_tmp);
			}
			Err(e) => panic!("encountered unknown fault type {}", e),
		}
	};
	panic!("unhandled upcall fault\n     {:?}\n     ", fault);
}

/* TODO: make this thread-local or atomic */
static mut __FAULT_UNWINDING: bool = false;

#[no_mangle]
pub extern "C" fn __twz_fault_handler(fid: i32, info: *mut std::ffi::c_void) {
	/* If we encounter problems when panicking, the runtime will issue an abort via illegal
	 * instruction. Ensure that we don't end up in an infinite loop, there. */
	unsafe {
		if __FAULT_UNWINDING && fid == 2 {
			eprintln!("encountered abort during unwinding, exiting.");
			crate::thread::exit(fid + 256);
		}
	}
	/* We need to catch any unwinding here so we can mark the __fault_unwinding bool as true. */
	let res = std::panic::catch_unwind(|| catching_fault_handler(fid, info));
	if let Err(err) = res {
		unsafe {
			__FAULT_UNWINDING = true;
			/* make sure that the libtwz runtime also doesn't pass us any exception handling in
			 * case of abort causing that. */
			std::panic::resume_unwind(err);
		}
	}
}

extern "C" {
	fn twz_fault_runtime_fault_takeover(call: extern "C" fn(fid: i32, info: *mut std::ffi::c_void));
}

pub(crate) fn __twz_fault_runtime_init() {
	unsafe {
		/* tell libtwz to call us for all of these faults */
		twz_fault_runtime_fault_takeover(__twz_fault_handler);

		/* but also, we can handle the upcalls ourselves, so lets register our upcall handler */
		crate::kso::view::View::current().set_upcall_entry(
			crate::arch::fault::__twz_fault_upcall_entry,
			crate::arch::fault::__twz_fault_upcall_entry,
		);
	}
}
