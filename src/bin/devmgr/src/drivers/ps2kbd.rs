use crate::bus::Bus;
use crate::devtree::DeviceIdent;
use crate::driver::Driver;
use twz::device::Device;

struct Ps2kbdDriver {}

impl Driver for Ps2kbdDriver {
	fn supported() -> Vec<DeviceIdent> {
		vec![]
	}
	fn start(bus: &Box<dyn Bus>, device: Device, ident: DeviceIdent) {}
}

pub fn register() -> Box<dyn Driver> {
	Box::new(Ps2kbdDriver {})
}
