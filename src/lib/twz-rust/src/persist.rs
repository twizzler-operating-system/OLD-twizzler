use crate::arch;

fn flush_line(ptr: *const i8) {
    if arch::has_clwb() {
        unsafe {
            asm!("clwb [{}]", in(reg) ptr)
        }
    } else if arch::has_clflushopt() {
        unsafe {
            asm!("clflushopt [{}]", in(reg) ptr)
        }
    } else {
        unsafe {
            asm!("clflush [{}]", in(reg) ptr)
        }
    }
}

pub(crate) fn flush_lines<T>(data: &T) {
    let mut raw = unsafe {std::mem::transmute::<&T, *const i8>(data) };
    let len = std::mem::size_of::<T>();
    let first_line_len = raw.align_offset(arch::CACHE_LINE_SIZE);
    let mut counter = first_line_len;
    flush_line(raw);
    raw = unsafe { raw.offset(first_line_len as isize) };
    while counter < len {
        flush_line(raw);
        counter += arch::CACHE_LINE_SIZE;
    }
}

pub(crate) fn pfence() {
    unsafe {
        asm!("sfence");
    }
}

