#![feature(asm)]
#![feature(naked_functions)]

mod arch;
mod libtwz;
mod fault;
pub mod ptr;
pub mod obj;
mod persist;

#[no_mangle]
pub extern fn __twz_rust_twz_init()
{
    unsafe {
        libtwz::twz_c::twz_fault_set(6, Some(fault::__twz_fault_handler), std::ptr::null_mut());
        libtwz::twz_c::twz_fault_set(5, Some(fault::__twz_fault_handler), std::ptr::null_mut());
        libtwz::twz_c::twz_fault_set(4, Some(fault::__twz_fault_handler), std::ptr::null_mut());
        libtwz::twz_c::twz_fault_set(3, Some(fault::__twz_fault_handler), std::ptr::null_mut());
        libtwz::twz_c::twz_fault_set(2, Some(fault::__twz_fault_handler), std::ptr::null_mut());
        libtwz::twz_c::twz_fault_set(1, Some(fault::__twz_fault_handler), std::ptr::null_mut());
        libtwz::twz_c::twz_fault_set(0, Some(fault::__twz_fault_handler), std::ptr::null_mut());

        libtwz::twz_c::twz_fault_set_upcall_entry(Some(fault::__twz_fault_upcall_entry), Some(fault::__twz_fault_upcall_entry));
    }
}

#[derive(Debug)]
pub enum TxResultErr<E> {
    UserErr(E),
    OutOfLog,
}

#[derive(Debug)]
pub enum TwzErr {
    NameResolve(i32),
    OSError(i32),
    OutOfSlots,
}

#[cfg(test)]
mod tests {
    #[test]
    fn it_works() {
        assert_eq!(2 + 2, 4);
    }
}
