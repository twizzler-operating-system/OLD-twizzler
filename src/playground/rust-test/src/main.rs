#![feature(asm)]

extern crate twz;

extern "C" fn foo() {
	unsafe {
		asm!("mov rax, $0; mov [rax], rcx");
	}
}

fn main() {
	twz::use_runtime();
	foo();
	panic!("Hello");
}
