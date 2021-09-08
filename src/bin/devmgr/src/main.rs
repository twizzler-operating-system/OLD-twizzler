#![feature(asm)]
#![feature(naked_functions)]
#![feature(once_cell)]

use twz;
use twzobj;
//use std::{lazy::SyncLazy, sync::Mutex};

/*twz::twz_gate!(1, __logboi_open, logboi_open (flags: i32) {
	twz::sapi_return!(0, 0, 0, 0, 0);
});*/

mod bus;
mod busses;
mod devnode;
mod devtree;
mod driver;
#[macro_use]
mod drivers;

use std::convert::TryInto;

use twz::device::{Device, DeviceData};
use twz::kso::{KSOType, KSO};
use twz::obj::ProtFlags;

#[repr(C)]
#[derive(Default)]
struct Node {
	ptr: twz::ptr::Pptr<i32>,
}

/*
fn test() {
	let spec = twz::obj::CreateSpec::new(
		twz::obj::LifetimeType::Volatile,
		twz::obj::BackingType::Normal,
		twz::obj::CreateFlags::DFL_READ | twz::obj::CreateFlags::DFL_WRITE,
	);

	let obj1 = twz::obj::Twzobj::<i32>::create_ctor(&spec, |obj, tx| {
		println!("created object {}", obj.id());
		let mut base = obj.base_mut(tx);
		*base = 32;
	})
	.unwrap();

	let obj2 = twz::obj::Twzobj::<Node>::create_ctor(&spec, |obj, tx| {
		println!("created object {}", obj.id());
		let mut base = obj.base_mut(tx);
		base.ptr.set(obj1.base_ptr(), tx);
	})
	.unwrap();

	let base = obj2.base(None);
	println!("::: {:p}", base.ptr);
	println!("::: {}", *base.ptr.lea());

	loop {}
}*/

fn main() {
	twz::use_runtime();

	let root = twz::kso::get_root();

	let subtree = root.get_subtree(KSOType::Device).unwrap();
	let dir = subtree.get_dir().unwrap();
	println!("{}", dir.len());
	for c in dir {
		let kso = c.into_kso::<DeviceData, { KSOType::Device }>(ProtFlags::READ).unwrap();
		println!("{:?} :: {}", c, kso.name());
		let dev = kso.into_device();
		for dc in dev.get_children() {
			let kso = dc.into_generic_kso();
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
