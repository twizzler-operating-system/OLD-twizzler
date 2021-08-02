#![feature(asm)]

use twz;

extern "C" fn foo() {
	unsafe {
		asm!("mov rax, $0; mov [rax], rcx");
	}
}

fn main() {
	foo();
	panic!("Hello");
}
