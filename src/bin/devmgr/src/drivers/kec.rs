use crate::bus::Bus;
use crate::devnode;
use crate::devtree::DeviceIdent;
use crate::driver::Driver;
use std::thread::{spawn, JoinHandle};
use twz::device::{BusType, Device, DEVICE_ID_SERIAL};

pub struct Instance {
	writethread: Option<JoinHandle<()>>,
	readthread: Option<JoinHandle<()>>,
	nodes: Vec<devnode::DeviceNode>,
}

use std::io::Write;
use twz::device::DeviceEvent;
use twz::obj::{ProtFlags, Twzobj};
use twzobj::pty::{PtyClientHdr, PtyServerHdr};
fn kec_write_thread(instance: std::sync::Arc<std::sync::Mutex<Instance>>) {
	let server = {
		let instance = instance.lock().unwrap();
		Twzobj::<PtyServerHdr>::init_guid(instance.nodes[1].id, ProtFlags::READ | ProtFlags::WRITE)
	};
	let mut buffer = [0; 1024];
	loop {
		let read_result = twzobj::io::read(&server, &mut buffer, twzobj::io::ReadFlags::none()).unwrap();
		if let twzobj::io::ReadOutput::Done(len) = read_result {
			twz::sys::kec_write(&buffer[0..len], twz::sys::KECWriteFlags::none());
		}
	}
}

fn kec_read_thread(instance: std::sync::Arc<std::sync::Mutex<Instance>>) {
	let server = {
		let instance = instance.lock().unwrap();
		Twzobj::<PtyServerHdr>::init_guid(instance.nodes[1].id, ProtFlags::READ | ProtFlags::WRITE)
	};
	let mut buffer = [0; 1024];
	loop {
		let result = twz::sys::kec_read(&mut buffer, twz::sys::KECReadFlags::none());
		if let Ok(buf) = result {
			twzobj::io::write(&server, buf, twzobj::io::WriteFlags::none());
		}
	}
}

use twzobj::pty;
impl Instance {
	pub fn new() -> std::sync::Arc<std::sync::Mutex<Instance>> {
		let spec = twz::obj::CreateSpec::new(
			twz::obj::LifetimeType::Volatile,
			twz::obj::BackingType::Normal,
			twz::obj::CreateFlags::DFL_READ | twz::obj::CreateFlags::DFL_WRITE,
		);
		let (client, server) = pty::create_pty_pair(&spec, &spec).unwrap();
		let nodes = devnode::allocate(&[("kec_ptyc", client.id()), ("kec_ptys", server.id())]);
		let inst = std::sync::Arc::new(std::sync::Mutex::new(Instance {
			readthread: None,
			writethread: None,
			nodes: nodes,
		}));

		let inst2 = inst.clone();
		{
			let mut inst = inst.lock().unwrap();
			inst.writethread = Some(spawn(|| kec_write_thread(inst2)));
		}
		let inst2 = inst.clone();
		{
			let mut inst = inst.lock().unwrap();
			inst.readthread = Some(spawn(|| kec_read_thread(inst2)));
		}
		devnode::publish(&inst.lock().unwrap().nodes[0]);
		inst
	}
}

#[derive(Default)]
struct KECDriver {
	instances: Vec<std::sync::Arc<std::sync::Mutex<Instance>>>,
}
