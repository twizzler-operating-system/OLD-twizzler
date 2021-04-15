use crate::libtwz;
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
    Signal = 7
}

use std::convert::TryFrom;
use std::convert::TryInto;
impl std::convert::TryFrom<i32> for FaultTypeID {
    type Error = i32;
    fn try_from(val: i32) -> Result<Self, Self::Error> {
        match(val) {
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
    view: libtwz::twz_c::LibtwzObjID,
    addr: u64,
    flags: u64,
}

impl std::fmt::Debug for FaultInfo {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "FaultInfo {{ view: {:x}, addr: {:x}, flags: {:x} }}",
               self.view, self.addr, self.flags)
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
    id: libtwz::twz_c::LibtwzObjID,
    ip: u64,
    addr: u64,
    flags: u64,
    pad: u64,
}

impl std::fmt::Debug for ObjectInfo {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "ObjectInfo {{ id: {:x}, ip: {:x}, addr: {:x}, flags: {:x} }}",
               self.id, self.ip, self.addr, self.flags)
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
        write!(f, "NullInfo {{ ip: {:x}, addr: {:x} }}",
               self.ip, self.addr)
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
        write!(f, "ExceptionInfo {{ ip: {:x}, code: {}, arg: {:x} }}",
               self.ip, self.code, self.arg)
    }
}

#[derive(Copy, Clone)]
#[repr(C)]
struct SctxInfo {
    target: libtwz::twz_c::LibtwzObjID,
    ip: u64,
    addr: u64,
    pneed: u32,
    pad: u32,
    pad2: u64,
}

impl std::fmt::Debug for SctxInfo {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "SctxInfo {{ target: {:x}, ip: {:x}, addr: {:x}, pneed: {:x} }}",
               self.target, self.ip, self.addr, self.pneed)
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
    id: libtwz::twz_c::LibtwzObjID,
    addr: u64,
    pgnr: u64,
    info: u64,
    ip: u64,
}

impl std::fmt::Debug for PageInfo {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "PageInfo {{ id: {:x}, pgnr: {}, ip: {:x}, addr: {:x}, info: {:x} }}",
               self.id, self.pgnr, self.ip, self.addr, self.info)
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
    id: libtwz::twz_c::LibtwzObjID,
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
    Unknown,
    Object(ObjectInfo),
    Null(NullInfo),
    Exception(ExceptionInfo),
    Sctx(SctxInfo),
    Page(PageInfo),
    Pptr(PptrInfo),
    Signal(SignalInfo),
}

fn catching_fault_handler(fid: i32, info: *mut std::ffi::c_void, data: *mut std::ffi::c_void) {
    let fault = unsafe { match(fid.try_into()) {
        Ok(FaultTypeID::Object) => Fault::Object(*(std::mem::transmute::<*mut std::ffi::c_void, &ObjectInfo>(info))),
        Ok(FaultTypeID::Null) => Fault::Null(*(std::mem::transmute::<*mut std::ffi::c_void, &NullInfo>(info))),
        Ok(FaultTypeID::Exception) => Fault::Exception(*(std::mem::transmute::<*mut std::ffi::c_void, &ExceptionInfo>(info))),
        Ok(FaultTypeID::Sctx) => Fault::Sctx(*(std::mem::transmute::<*mut std::ffi::c_void, &SctxInfo>(info))),
        Ok(FaultTypeID::Page) => Fault::Page(*(std::mem::transmute::<*mut std::ffi::c_void, &PageInfo>(info))),
        Ok(FaultTypeID::Pptr) => Fault::Pptr(*(std::mem::transmute::<*mut std::ffi::c_void, &PptrInfo>(info))),
        Ok(FaultTypeID::Signal) => Fault::Signal(*(std::mem::transmute::<*mut std::ffi::c_void, &SignalInfo>(info))),
        Ok(FaultTypeID::Double) => {
            let mut subfault_tmp = std::mem::transmute::<*mut std::ffi::c_void, &DoubleFaultInfo<()>>(info);
            if (subfault_tmp.fault as i32).try_into() == Ok(FaultTypeID::Double) {
                panic!("encountered double fault while handing double fault");
            }
            __twz_fault_handler(subfault_tmp.fault as i32, std::mem::transmute::<&(), *mut std::ffi::c_void>(&subfault_tmp.data), std::ptr::null_mut());
            return;
        },
        Err(e) => panic!("encountered unknown fault type {}", e),
    }};
    panic!("{:#?}", fault);

}

extern "C" {
    fn abort() -> !;
}



static mut __fault_unwinding: bool = false;

#[no_mangle]
pub extern fn __twz_fault_handler(fid: i32, info: *mut std::ffi::c_void, data: *mut std::ffi::c_void) {
    unsafe {
        if __fault_unwinding && fid == 2 {
            eprintln!("encountered abort during unwinding, exiting.");
            libtwz::twz_c::twz_thread_exit(fid + 256);
        }
    }
    let res = std::panic::catch_unwind( || 
                                        catching_fault_handler(fid, info, data));
    if let Err(err) = res {
        unsafe {
            __fault_unwinding = true;
            libtwz::twz_c::twz_fault_set(FaultTypeID::Exception as i32, None, std::ptr::null_mut());
            std::panic::resume_unwind(err);
            abort();
        }
    }
}

#[naked]
pub extern fn __twz_fault_upcall_entry() {
    unsafe {
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
                        "mov rdx, rsp",
                        "call __twz_fault_handler",
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
        );
    }
}
