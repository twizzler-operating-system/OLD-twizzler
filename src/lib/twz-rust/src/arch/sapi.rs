#[naked]
#[no_mangle]
pub extern fn __gate_return()
{
    unsafe {
        #[inline(never)]
    asm!(
        "movabs rax, __next_avail_stack_backup",
        "mfence",
        "movabs __next_avail_stack, rax",
        "mfence",
        "mov rdx, rdi",
        "mov r8, rsi",
        "mov r9, rdx",
        "mov r10, rcx",
        "mov rsi, r11",
        "xor rdi, rdi",
        "mov rax, 6",
        "syscall",
        "ud2", options(noreturn));

}
}
#[used]
#[no_mangle]
static mut __next_avail_stack: *mut u8 = std::ptr::null_mut();
#[used]
#[no_mangle]
static mut __next_avail_stack_backup: *mut u8 = std::ptr::null_mut();

#[link(name = "twzsec")]
#[link(name = "tomcrypt")]
#[link(name = "tommath")]
extern "C" {
    fn twz_secure_api_create(api: *mut std::ffi::c_void, name: *const i8) -> i32;
}

pub(crate) fn wrap_twz_secure_api_create(obj: &crate::obj::Twzobj, name: &str) -> i32 {
    unsafe {
        obj.alloc_libtwz_data();
        let s = std::ffi::CString::new(name).unwrap();
        twz_secure_api_create(
            obj.libtwz_data.lock().unwrap().as_mut().unwrap().data as *mut std::ffi::c_void,
            s.as_ptr())
    }
}

pub(crate) fn init()
{
    unsafe { 
        __next_avail_stack = std::alloc::alloc(
            std::alloc::Layout::from_size_align(
                0x4000,
                16
                ).expect("failed to calculate memory layout for libtwz::twzobj"));
        __next_avail_stack = __next_avail_stack.offset(0x4000);
        __next_avail_stack_backup = __next_avail_stack;
    }
}

/* TODO KUID */
#[macro_export]
macro_rules! sapi_return {
    ($ret:expr, $a0:expr, $a1:expr, $a2:expr, $a3:expr) => {
        unsafe {
            asm!(
                "push 0",
                "jmp __gate_return",
                in("rdi") $a0,
                in("rsi") $a1,
                in("rdx") $a2,
                in("rcx") $a3,
                in("r11") $ret,
                options(noreturn));
        }
    }
}

#[macro_export]
macro_rules! twz_gate {
    ($nr:expr, $name2: ident, $name:ident ($($arg:ident:$typ:ty),*) $lambda:expr) => {
        #[no_mangle]
        #[inline(never)]
        pub extern fn $name($($arg:$typ,)*) -> i64 {
            $lambda
        }

        #[naked]
        #[link_section = ".gates"]
        #[no_mangle]
        pub extern fn $name2()
        {
            unsafe {
                #[inline(never)]
                asm!(
                    concat!(".org ", stringify!($nr), "*64, 0x90"),
                    "mov rcx, r10",
                    "mov rsp, 0",
                    "lock xchg rsp, __next_avail_stack",
                    "test rsp, rsp",
                    concat!("jz ", stringify!($name2)),
                    concat!("lea rax, ", stringify!($name)),
                    "call rax",
                    "mov r11, rax",
                    "push 0",
                    "jmp __gate_return",
                    "nop",
                    "nop",
                    "nop",
                    "nop",
                    "nop",
                    "nop",
                    "nop",
                    "nop",
                    "nop",
                    "nop",
                    "nop",
                    "nop",
                    "nop",
                    "nop",
                    "nop",
                    "nop",
                    ".previous",
                    options(noreturn))
            }
        }
    }
}
 
