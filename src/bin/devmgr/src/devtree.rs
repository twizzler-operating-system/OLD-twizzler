pub struct DevTree {
	busses: Vec<Box<dyn Bus>>,
}

#[derive(Debug, Copy, Clone)]
pub struct DeviceIdent {
	pub bustype: twz::device::BusType,
	pub vendor_id: u64,
	pub device_id: u64,
	pub class: u64,
	pub subclass: u64,
}

impl DeviceIdent {
	pub fn is_match(&self, other: &DeviceIdent) -> bool {
		other.bustype == self.bustype
			&& ((other.class == self.class && other.subclass == self.subclass && self.class > 0 && other.class > 0)
				|| (other.vendor_id == self.vendor_id && other.device_id == self.device_id && self.device_id > 0 && other.device_id > 0))
	}

	pub fn new<T: Into<u64>>(bustype: twz::device::BusType, vendor: T, device: T, class: T, sc: T) -> DeviceIdent {
		DeviceIdent {
			bustype: bustype.into(),
			vendor_id: vendor.into(),
			device_id: device.into(),
			class: class.into(),
			subclass: sc.into(),
		}
	}
}

use crate::bus::Bus;
use crate::busses::create_bus;
use crate::devidentstring;
use crate::drivers::RegisteredDrivers;
use twz::device::DeviceData;
use twz::kso::{KSODirAttachments, KSOType, KSO};
use twz::obj::ProtFlags;

impl DevTree {
	pub fn enumerate_busses(root: &KSO<KSODirAttachments>) -> Result<DevTree, twz::TwzErr> {
		let mut vec = vec![];
		let dir = root.get_dir().unwrap();

		for chattach in dir {
			let chkso = chattach.into_kso::<DeviceData, { KSOType::Device }>(ProtFlags::READ);
			if let Some(chkso) = chkso {
				let dev = chkso.into_device();

				let bus = create_bus(dev);
				if let Some(bus) = bus {
					vec.push(bus);
				}
			}
		}

		Ok(DevTree { busses: vec })
	}

	pub fn init_busses(&mut self) {
		for bus in &mut self.busses {
			let _res = bus.init();
		}
	}

	pub fn init_devices(&mut self, drivers: &mut RegisteredDrivers) {
		for bus in &mut self.busses {
			bus.enumerate(&mut |mut dev| {
				let ident = bus.identify(&mut dev);
				if let Some(ident) = ident {
					println!("[devmgr] found device {}", ident.human_readable_string());
					drivers.start_driver(bus, dev, ident);
				}
				Ok(())
			});
		}
	}
}
