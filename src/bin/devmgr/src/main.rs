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
mod devnode;
mod devtree;
mod driver;
#[macro_use]
mod drivers;
mod devidentstring;

use twz::device::DeviceData;
use twz::kso::KSOType;
use twz::obj::ProtFlags;

#[repr(C)]
#[derive(Default, Debug)]
struct Node {
	ptr: twz::ptr::Pptr<i32>,
}

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
		base.ptr.set(obj1.base(), tx);
	})
	.unwrap();

	let base = obj2.base();
	println!("::: {:p}", base.ptr);
	println!("::: {}", *base.ptr.lea());

	println!("{:#?}", obj2.base());

	loop {}
}

#[derive(Clone, Copy, Debug)]
struct Foo {
	x: u32,
}

#[derive(Clone, Copy, Debug)]
struct Bar {
	y: u32,
}

use twzobj::queue::{Queue, QueueEntry, QueueFlags};
fn main() {
	twz::use_runtime();

	let s = twz::obj::CreateSpec::new(
		twz::obj::LifetimeType::Volatile,
		twz::obj::BackingType::Normal,
		twz::obj::CreateFlags::DFL_READ | twz::obj::CreateFlags::DFL_WRITE,
	);
	let q = Queue::<Foo, Bar>::create(&s, 8, 8).unwrap();

	let qe = QueueEntry::new(41, Foo { x: 42 });

	let ret = q.submit(&qe, QueueFlags::none());
	println!("submitted: {:?}", ret);
	let ret = q.receive(QueueFlags::none());
	println!("received: {:?}", ret);

	let qe2 = QueueEntry::new(43, Bar { y: 44 });

	let ret = q.complete(&qe2, QueueFlags::none());
	println!("submittedc: {:?}", ret);
	let ret = q.get_completed(QueueFlags::none());
	println!("receivedc: {:?}", ret);

	loop {}
	//test();
	//
	//

	let root = twz::kso::get_root();

	let subtree = root.get_subtree(KSOType::Device).unwrap();
	let dir = subtree.get_dir().unwrap();
	for c in dir {
		let kso = c.into_kso::<DeviceData, { KSOType::Device }>(ProtFlags::READ).unwrap();
		let dev = kso.into_device();
		for dc in dev.get_children() {
			let kso = dc.into_generic_kso();
		}
	}

	let mut rd = drivers::register();

	let mut tree = devtree::DevTree::enumerate_busses(&subtree).expect("failed to enumerate busses");
	tree.init_busses();
	tree.init_devices(&mut rd);

	let kec_instance = drivers::kec::Instance::new();
	loop {
		use std::{thread, time};
		let ten_millis = time::Duration::from_millis(1000);
		thread::sleep(ten_millis);
	}

	//twz::sapi::sapi_create_name("devmgr").expect("failed to init sapi");
}
