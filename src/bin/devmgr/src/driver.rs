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
