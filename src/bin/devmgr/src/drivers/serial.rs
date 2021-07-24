use crate::bus::Bus;
use crate::devtree::DeviceIdent;
use crate::driver::Driver;
use std::thread::{sleep, spawn, JoinHandle};
use twz::device::{BusType, Device, DEVICE_ID_SERIAL};

struct Instance {
	device: Device,
	ident: DeviceIdent,
	thread: Option<JoinHandle<()>>,
}

fn serial_interrupt_thread(instance: std::sync::Arc<std::sync::Mutex<Instance>>) {
	loop {
		let instance = instance.lock().unwrap();

		for event in instance.device.check_for_events() {
			println!("Got Event! {:?}", event);
		}
		instance.device.wait_for_event();
	}
}

impl Instance {
	fn new(device: Device, ident: &DeviceIdent) -> std::sync::Arc<std::sync::Mutex<Instance>> {
		let mut inst = std::sync::Arc::new(std::sync::Mutex::new(Instance {
			device,
			ident: *ident,
			thread: None,
		}));

		let inst2 = inst.clone();
		{
			let mut inst = inst.lock().unwrap();
			inst.thread = Some(spawn(|| serial_interrupt_thread(inst2)));
		}
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

	fn start(&mut self, bus: &Box<dyn Bus>, device: Device, ident: &DeviceIdent) {
		self.instances.push(Instance::new(device, ident));
	}
}

crate::mod_driver_register!(SerialDriver);
