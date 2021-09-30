#![feature(asm)]
#![feature(naked_functions)]
#![allow(dead_code)]
#![allow(incomplete_features)]
#![feature(const_generics)]
#![feature(once_cell)]

mod bitflags;

mod alloc;
mod arch;
mod fault;
pub mod flexarray;
pub mod refs;
//pub mod gate;
//mod libtwz;
pub mod event;
pub mod name;
pub mod obj;
mod persist;
pub mod ptr;
//pub mod queue;
//#[cfg(feature = "expose_sapi")]
//pub mod sapi;
pub mod sys;

//pub mod bstream;
pub mod device;
pub mod kso;
//pub mod log;
pub mod mutex;
//pub mod pslice;
pub mod thread;
//pub mod vec;

pub fn use_runtime() {}

#[no_mangle]
pub extern "C" fn __twz_libtwz_runtime_init() {
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
	Invalid,
	OutOfSlots,
}

pub fn foo() {}
