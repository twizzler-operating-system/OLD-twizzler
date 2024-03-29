use crate::devtree::DeviceIdent;
use twz::device::{BusType, Device};
use twz::TwzErr;

pub trait Bus {
	fn get_bus_root(&self) -> &Device;
	fn enumerate(&self, f: &mut dyn FnMut(Device) -> Result<(), TwzErr>) {
		let mut idx = 0;
		let root = self.get_bus_root();
		loop {
			if let Some(k) = root.get_child_device(idx) {
				let _res = f(k.into_device());
				idx += 1;
			} else {
				return;
			}
		}
	}
	fn get_bus_type() -> BusType
	where
		Self: Sized;
	fn new(root: Device) -> Self
	where
		Self: Sized;
	fn init(&mut self) -> Result<(), TwzErr> {
		Ok(())
	}

	fn start_device(&self, _dev: &Device) {}

	fn identify(&self, _dev: &mut Device) -> Option<DeviceIdent> {
		None
	}
}
