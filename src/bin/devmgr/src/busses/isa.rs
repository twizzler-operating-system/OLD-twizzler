use twz::device::{Device,BusType};
use crate::bus::Bus;

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
        IsaBus {
            root: root,
        }
    }
}

