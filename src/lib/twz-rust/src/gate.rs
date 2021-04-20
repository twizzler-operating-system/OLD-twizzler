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

pub fn init()
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

#[macro_export]
macro_rules! twz_gate {
    ($nr:expr, $name2: ident, $name:ident ($($arg:ident:$typ:ty),*) $lambda:expr) => {
        #[no_mangle]
        #[inline(never)]
        pub extern fn $name($($arg:$typ,)*) {
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
    

