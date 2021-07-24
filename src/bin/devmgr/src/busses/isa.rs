use crate::bus::Bus;
use crate::devtree::DeviceIdent;
use twz::device::{BusType, Device};

pub struct IsaBus {
	root: Device,
}

impl Bus for IsaBus {
	fn get_bus_root(&self) -> &Device {
		&self.root
	}

	fn get_bus_type() -> BusType {
		BusType::Isa
	}

	fn new(root: Device) -> Self {
		IsaBus { root: root }
	}

	fn identify(&self, dev: &mut Device) -> Option<DeviceIdent> {
		let hdr = dev.get_device_hdr();
		Some(DeviceIdent::new(Self::get_bus_type(), 0, hdr.devid, 0, 0))
	}
}
