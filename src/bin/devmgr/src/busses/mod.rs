use crate::bus::Bus;
use twz::device::{BusType, Device};

mod isa;
mod pcie;

pub fn create_bus(dev: Device) -> Option<Box<dyn Bus>> {
	let bt = BusType::from_u64(dev.get_device_hdr().bustype);
	match bt {
		BusType::Isa => Some(Box::new(isa::IsaBus::new(dev))),
		BusType::Pcie => Some(Box::new(pcie::PcieBus::new(dev))),
		_ => None,
	}
}
