use crate::bus::Bus;
use twz::device::{BusType, Device};

pub struct PcieBus {
	root: Device,
}

impl Bus for PcieBus {
	fn get_bus_root(&self) -> &Device {
		&self.root
	}

	fn get_bus_type() -> BusType {
		BusType::Pcie
	}

	fn new(root: Device) -> Self {
		PcieBus { root: root }
	}
}
