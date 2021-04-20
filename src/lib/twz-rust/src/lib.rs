#![feature(asm)]
#![feature(naked_functions)]

mod arch;
mod libtwz;
mod fault;
pub mod ptr;
pub mod obj;
mod persist;
pub mod queue;
mod sys;
pub mod gate;
#[cfg(feature = "expose_sapi")]
pub mod sapi;


#[no_mangle]
pub extern fn __twz_libtwz_runtime_init()
{
    fault::__twz_fault_runtime_init();
}

#[derive(Debug)]
pub enum TxResultErr<E> {
    UserErr(E),
    OSError(i32),
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