#![feature(asm)]
#![feature(naked_functions)]
#![feature(once_cell)]

use twz;
//use std::{lazy::SyncLazy, sync::Mutex};

/*twz::twz_gate!(1, __logboi_open, logboi_open (flags: i32) {
	twz::sapi_return!(0, 0, 0, 0, 0);
});*/

mod bus;
mod busses;
mod devtree;
mod driver;
#[macro_use]
mod drivers;

use std::convert::TryInto;
use twz::kso::KSO;
fn main() {
	println!("Hello!");

	let root = twz::kso::get_root().unwrap();

	let subtree = root.get_subtree(twz::kso::KSOType::Device).unwrap();
	let dir = subtree.get_dir().unwrap();
	println!("{}", dir.len());
	for c in dir {
		let kso: twz::kso::KSO = c.try_into().unwrap();
		println!("{:?} :: {}", c, kso.name());
		let mut dev = kso.into_device();
		for dc in dev.get_children() {
			let kso: twz::kso::KSO = dc.try_into().unwrap();
			println!("   {:?} :: {}", dc, kso.name());
		}

		//1let chobj: twz::obj::Twzobj = c.try_into().unwrap();
	}

	let mut rd = drivers::register();

	let mut tree = devtree::DevTree::enumerate_busses(&subtree).expect("failed to enumerate busses");
	tree.init_busses();
	tree.init_devices(&mut rd);

	loop {
		use std::{thread, time};
		let ten_millis = time::Duration::from_millis(1000);
		thread::sleep(ten_millis);
	}

	//twz::sapi::sapi_create_name("devmgr").expect("failed to init sapi");
}
