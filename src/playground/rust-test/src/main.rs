#![feature(asm)]

extern crate twz;

fn main() {
	twz::use_runtime();
	panic!("Hello");
}
