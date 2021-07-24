use crate::bus::Bus;
use crate::devtree::DeviceIdent;
use twz::device::Device;

pub trait Driver {
	fn start(&mut self, bus: &Box<dyn Bus>, device: Device, ident: &DeviceIdent);
	fn supported() -> Vec<DeviceIdent>
	where
		Self: Sized;
	fn new() -> Self
	where
		Self: Sized + Default,
	{
		std::default::Default::default()
	}
}

pub fn supports<T: Driver>(drv: Box<T>, dev: &DeviceIdent) -> bool {
	for di in &T::supported() {
		if di.is_match(dev) {
			return true;
		}
	}
	false
}
