#![feature(asm)]

extern crate twz;

extern "C" fn foo() {
	unsafe {
		asm!("ud2");
	}
}

fn main() {
	twz::use_runtime();
	foo();
	panic!("Hello");
}
