#![feature(asm)]

extern crate twz;

fn main() {
	twz::use_runtime();
	let x = 12313123312341234123 as *const i32;
	println!("{}", unsafe { *x });
	//panic!("Hello");
}
