use crate::bus::Bus;
use crate::devnode;
use crate::devtree::DeviceIdent;
use crate::driver::Driver;
use std::thread::{spawn, JoinHandle};
use twz::device::{BusType, Device, DEVICE_ID_SERIAL};

struct Instance {
	device: Device,
	_ident: DeviceIdent,
	thread: Option<JoinHandle<()>>,
	nodes: Vec<devnode::DeviceNode>,
}

use std::io::Write;
use twz::device::DeviceEvent;
use twz::obj::{ProtFlags, Twzobj};
use twzobj::pty::{PtyClientHdr, PtyServerHdr};
fn serial_interrupt_thread(instance: std::sync::Arc<std::sync::Mutex<Instance>>) {
	let instance = instance.lock().unwrap();
	let client = Twzobj::<PtyClientHdr>::init_guid(instance.nodes[0].id, ProtFlags::READ | ProtFlags::WRITE);
	let server = Twzobj::<PtyServerHdr>::init_guid(instance.nodes[1].id, ProtFlags::READ | ProtFlags::WRITE);
	let poll_result = twzobj::io::poll(&server, twzobj::io::PollStates::READ).unwrap();
	let mut buffer = [0; 1024];
	loop {
		if poll_result.is_ready(twzobj::io::PollStates::READ) {
			let read_result = twzobj::io::read(&server, &mut buffer, twzobj::io::ReadFlags::NONBLOCK).unwrap();
			if let twzobj::io::ReadOutput::Done(len) = read_result {
				if let Ok(s) = std::str::from_utf8(&buffer[0..len]) {
					print!("{}", s);
					std::io::stdout().flush();
				} else {
					eprintln!("[serial] got non-utf8 data on serial pty");
				}
			}
		}

		for event in instance.device.check_for_events() {
			match event {
				DeviceEvent::DeviceSync(0, ch) => {
					twzobj::io::write(&server, &[ch as u8], twzobj::io::WriteFlags::none());
				}
				DeviceEvent::DeviceSync(x, y) => {
					eprintln!("[serial] unknown device sync event {} {}", x, y);
				}
				DeviceEvent::DeviceInterrupt(x, y, z) => {
					eprintln!("[serial] unhandled device interrupt {} {} {}", x, y, z);
				}
			}
		}

		if !poll_result.is_ready(twzobj::io::PollStates::READ) {
			instance.device.wait_for_event(&[poll_result.event()]);
		}
	}
}

use twzobj::pty;
impl Instance {
	fn new(device: Device, ident: &DeviceIdent) -> std::sync::Arc<std::sync::Mutex<Instance>> {
		let spec = twz::obj::CreateSpec::new(
			twz::obj::LifetimeType::Volatile,
			twz::obj::BackingType::Normal,
			twz::obj::CreateFlags::DFL_READ | twz::obj::CreateFlags::DFL_WRITE,
		);
		let (client, server) = pty::create_pty_pair(&spec, &spec).unwrap();
		let nodes = devnode::allocate(&[("ptyc", client.id()), ("ptys", server.id())]);
		let inst = std::sync::Arc::new(std::sync::Mutex::new(Instance {
			device: device,
			_ident: *ident,
			thread: None,
			nodes: nodes,
		}));

		let inst2 = inst.clone();
		{
			let mut inst = inst.lock().unwrap();
			inst.thread = Some(spawn(|| serial_interrupt_thread(inst2)));
		}
		devnode::publish(&inst.lock().unwrap().nodes[0]);
		inst
	}
}

#[derive(Default)]
struct SerialDriver {
	instances: Vec<std::sync::Arc<std::sync::Mutex<Instance>>>,
}

impl Driver for SerialDriver {
	fn supported() -> Vec<DeviceIdent> {
		vec![DeviceIdent::new(BusType::Isa, 0, DEVICE_ID_SERIAL, 0, 0)]
	}

	fn start(&mut self, _bus: &Box<dyn Bus>, device: Device, ident: &DeviceIdent) {
		//self.instances.push(Instance::new(device, ident));
	}
}

crate::mod_driver_register!(SerialDriver);
