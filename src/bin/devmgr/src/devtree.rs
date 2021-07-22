pub struct DevTree {
	busses: Vec<Box<dyn Bus>>,
}

use crate::bus::Bus;
use crate::busses::create_bus;
use std::convert::TryInto;
use twz::kso::KSO;

impl DevTree {
	pub fn enumerate_busses(root: &KSO) -> Result<DevTree, twz::TwzErr> {
		let mut vec = vec![];
		let dir = root.get_dir().unwrap();

		for chattach in dir {
			let chkso: KSO = chattach.try_into()?;
			let mut dev = chkso.into_device();

			let bus = create_bus(dev);
			if let Some(bus) = bus {
				vec.push(bus);
			}
		}

		Ok(DevTree { busses: vec })
	}

	pub fn init_busses(&mut self) {
		for bus in &mut self.busses {
			let br = bus.get_bus_root();
			println!("{}", br.get_kso_name());
			bus.init();
		}
	}
}
