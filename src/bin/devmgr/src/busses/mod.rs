use crate::bus::Bus;
use twz::device::{Device,BusType};

mod isa;

pub fn create_bus(dev: Device) -> Option<Box<dyn Bus>>
{
    let bt = BusType::from_u64(dev.get_device_hdr().bustype);
    match bt {
        BusType::Isa => Some(Box::new(isa::IsaBus::new(dev))),
        _ => None,
    }
}
