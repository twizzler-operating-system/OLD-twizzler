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
        "mov rsi, rdi",
        "xor rdx, rdx",
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

fn init()
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
pub fn sapi_create_name(name: &str) -> Result<crate::obj::Twzobj, crate::TwzErr> {
    init();
    /* TODO: no more dfl write */
    let id = crate::libtwz::twz_object_create(crate::obj::Twzobj::TWZ_OBJ_CREATE_DFL_READ | crate::obj::Twzobj::TWZ_OBJ_CREATE_DFL_WRITE, 0, 0).map_err(|e| crate::TwzErr::OSError(e))?;
    let obj = crate::obj::Twzobj::init_guid(id)?;
    let res = wrap_twz_secure_api_create(&obj, name);
    if res < 0 {
        return Err(crate::TwzErr::OSError(-res));
    }
    let s = std::ffi::CString::new(name).unwrap();
    let res = unsafe { crate::libtwz::twz_c::twz_name_assign(obj.id(), s.as_ptr()) };
    if res < 0 {
        return Err(crate::TwzErr::OSError(-res));
    }
    Ok(obj)
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
                    "mov rsp, 0",
                    "lock xchg rsp, __next_avail_stack",
                    "test rsp, rsp",
                    concat!("jz ", stringify!($name2)),
                    concat!("lea rax, ", stringify!($name)),
                    "call rax",
                    "mov rdi, rax",
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
                    "nop",
                    "nop",
                    ".previous",
                    options(noreturn))
            }
        }
    }
}
 
