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
        write!(f, "ObjectInfo {{ id: {:x}, ip: {:x}, addr: {:x}, flags: ",
               self.id, self.ip, self.addr)?;
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
    Object(ObjectInfo),
    Null(NullInfo),
    Exception(ExceptionInfo),
    Sctx(SctxInfo),
    Page(PageInfo),
    Pptr(PptrInfo),
    Signal(SignalInfo),
}

fn catching_fault_handler(fid: i32, info: *mut std::ffi::c_void, _data: *mut std::ffi::c_void) {
    let fault = unsafe { match fid.try_into() {
        Ok(FaultTypeID::Object)    => Fault::Object(*(std::mem::transmute::<*mut std::ffi::c_void, &ObjectInfo>(info))),
        Ok(FaultTypeID::Null)      => Fault::Null(*(std::mem::transmute::<*mut std::ffi::c_void, &NullInfo>(info))),
        Ok(FaultTypeID::Exception) => Fault::Exception(*(std::mem::transmute::<*mut std::ffi::c_void, &ExceptionInfo>(info))),
        Ok(FaultTypeID::Sctx)      => Fault::Sctx(*(std::mem::transmute::<*mut std::ffi::c_void, &SctxInfo>(info))),
        Ok(FaultTypeID::Page)      => Fault::Page(*(std::mem::transmute::<*mut std::ffi::c_void, &PageInfo>(info))),
        Ok(FaultTypeID::Pptr)      => Fault::Pptr(*(std::mem::transmute::<*mut std::ffi::c_void, &PptrInfo>(info))),
        Ok(FaultTypeID::Signal)    => Fault::Signal(*(std::mem::transmute::<*mut std::ffi::c_void, &SignalInfo>(info))),
        Ok(FaultTypeID::Double)    => {
            let subfault_tmp = std::mem::transmute::<*mut std::ffi::c_void, &DoubleFaultInfo<()>>(info);
            if (subfault_tmp.fault as i32).try_into() == Ok(FaultTypeID::Double) {
                panic!("encountered double fault while handing double fault");
            }
            panic!("unhandled upcall double fault {:?}", subfault_tmp);
        },
        Err(e) => panic!("encountered unknown fault type {}", e),
    }};
    panic!("unhandled upcall fault\n     {:?}\n     ", fault);
}

static mut __FAULT_UNWINDING: bool = false;

#[no_mangle]
pub extern fn __twz_fault_handler(fid: i32, info: *mut std::ffi::c_void, data: *mut std::ffi::c_void) {
    /* If we encounter problems when panicking, the runtime will issue an abort via illegal
     * instruction. Ensure that we don't end up in an infinite loop, there. */
    unsafe {
        if __FAULT_UNWINDING && fid == 2 {
            eprintln!("encountered abort during unwinding, exiting.");
            libtwz::twz_c::twz_thread_exit(fid + 256);
        }
    }
    /* We need to catch any unwinding here so we can mark the __fault_unwinding bool as true. */
    let res = std::panic::catch_unwind( || 
                                        catching_fault_handler(fid, info, data));
    if let Err(err) = res {
        unsafe {
            __FAULT_UNWINDING = true;
            /* make sure that the libtwz runtime also doesn't pass us any exception handling in
             * case of abort causing that. */
            libtwz::twz_c::twz_fault_set(FaultTypeID::Exception as i32, None, std::ptr::null_mut());
            std::panic::resume_unwind(err);
        }
    }
}

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
    __twz_fault_handler(fid, info, std::ptr::null_mut());
}

pub(crate) fn __twz_fault_runtime_init()
{
    unsafe {
        /* tell libtwz to call us for all of these faults */
        libtwz::twz_c::twz_fault_set(6, Some(__twz_fault_handler), std::ptr::null_mut());
        libtwz::twz_c::twz_fault_set(5, Some(__twz_fault_handler), std::ptr::null_mut());
        libtwz::twz_c::twz_fault_set(4, Some(__twz_fault_handler), std::ptr::null_mut());
        libtwz::twz_c::twz_fault_set(3, Some(__twz_fault_handler), std::ptr::null_mut());
        libtwz::twz_c::twz_fault_set(2, Some(__twz_fault_handler), std::ptr::null_mut());
        libtwz::twz_c::twz_fault_set(1, Some(__twz_fault_handler), std::ptr::null_mut());
        libtwz::twz_c::twz_fault_set(0, Some(__twz_fault_handler), std::ptr::null_mut());

        /* but also, we can handle the upcalls ourselves, so lets register our upcall handler */
        libtwz::twz_c::twz_fault_set_upcall_entry(Some(__twz_fault_upcall_entry), Some(__twz_fault_upcall_entry));
    }
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
