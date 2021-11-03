use crate::bus::Bus;
use crate::devtree::DeviceIdent;
use crate::driver::Driver;
use twz::device::Device;

macro_rules! mod_hookup {
	($($module:tt), *) => {
		$( mod $module; )*
		fn do_register() -> Vec<(Box<dyn Driver>, Vec<DeviceIdent>)> {
			let mut vec = vec![];
			$( vec.push($module::register()); )*
			vec
		}
	};
}

mod_hookup! {serial}

pub mod kec;

#[macro_export]
macro_rules! mod_driver_register {
	($drv:tt) => {
		pub fn register() -> (Box<dyn Driver>, Vec<DeviceIdent>) {
			(Box::new($drv::new()), $drv::supported())
		}
	};
}

pub struct RegisteredDrivers {
	list: Vec<(Box<dyn Driver>, Vec<DeviceIdent>)>,
}

impl RegisteredDrivers {
	pub fn start_driver(&mut self, bus: &Box<dyn Bus>, device: Device, ident: DeviceIdent) {
		for (drv, idents) in &mut self.list {
			for i in idents {
				if i.is_match(&ident) {
					drv.start(bus, device, &ident);
					return;
				}
			}
		}
	}
}

pub fn register() -> RegisteredDrivers {
	let v = do_register();
	RegisteredDrivers { list: v }
}
