use crate::bus::Bus;
use twz::device::{BusType, Device};
use twz::kso::KSOHdr;
use twz::TwzErr;

const PCIE_BUS_HEADER_MAGIC: u32 = 0x88582323;
const PCIE_HEADER_MULTIFUNCTION: u8 = 1 << 7;
const KACTION_CMD_PCIE_INIT_DEVICE: u64 = 1;

pub struct PcieBus {
	root: Device,
	devidx: usize,
}

#[derive(Debug, Copy, Clone)]
#[repr(C)]
struct PcieInfo {
	hdr: KSOHdr,
	magic: u32,
	start_bus: u32,
	end_bus: u32,
	segnr: u32,
	flags: u64,
}

#[derive(Debug, Copy, Clone)]
#[repr(C)]
struct PcieFunctionInfo {
	hdr: KSOHdr,
	deviceid: u16,
	vendorid: u16,
	classid: u16,
	subclassid: u16,
	progif: u16,
	flags: u16,
	bus: u16,
	device: u16,
	function: u16,
	segment: u16,
	header_type: u8,
	resv2: u8,
	resv: u16,
	prefetch: [u32; 6],
	bars: [u64; 6],
	barsz: [u64; 6],
}

#[repr(C, packed)]
struct PcieConfigSpaceHdr {
	/* 0x00 */
	vendor_id: u16,
	device_id: u16,
	/* 0x04 */
	command: u16,
	status: u16,
	/* 0x08 */
	revision: u8,
	progif: u8,
	subclass: u8,
	class_code: u8,
	/* 0x0C */
	cache_line_size: u8,
	latency_timer: u8,
	header_type: u8,
	bist: u8,
}

#[repr(C, packed)]
struct PcieConfigSpaceDevice {
	hdr: PcieConfigSpaceHdr,
	/* 0x10 */
	bar: [u32; 6],
	/* 0x28 */
	cardbus_cis_pointer: u32,
	/* 0x2C */
	subsystem_vendor_id: u16,
	subsystem_id: u16,
	/* 0x30 */
	expansion_rom_base_address: u32,
	/* 0x34 */
	cap_ptr: u32,
	/* 0x38 */
	reserved1: u32,
	/* 0x3C */
	interrupt_line: u8,
	interrupt_pin: u8,
	min_grant: u8,
	max_latency: u8,
}

#[repr(C, packed)]
struct PcieConfigSpaceBridge {
	hdr: PcieConfigSpaceHdr,
	bar: [u32; 2],
	primary_bus_nr: u8,
	secondary_bus_nr: u8,
	subordinate_bus_nr: u8,
	secondary_latency_timer: u8,
	io_base: u8,
	io_limit: u8,
	secondary_status: u8,
	memory_base: u16,
	memory_limit: u16,
	pref_memory_base: u16,
	pref_memory_limit: u16,
	/* 28 */
	pref_base_upper: u32,
	pref_limit_upper: u32,
	io_base_upper: u16,
	io_limit_upper: u16,
	cap_ptr: u32,
	exp_rom_base: u32,
	interrupt_line: u8,
	interrupt_pin: u8,
	bridge_control: u16,
}

impl PcieBus {
	fn setup_device(&mut self, info: &PcieInfo, fninfo: &PcieFunctionInfo, device: Device) {
		println!("setup {} {} {}", fninfo.bus, fninfo.device, fninfo.function);
	}

	fn init_device(&mut self, info: &PcieInfo, bus: u32, device: u32, function: u32) {
		let wc: u64 = 0;
		self.root.kaction(
			KACTION_CMD_PCIE_INIT_DEVICE,
			(info.segnr as u64) << 16 | (bus as u64) << 8 | (device as u64) << 3 | (function as u64) | (wc << 32),
		);

		let dev = self.root.get_child_device(self.devidx);
		if let Ok(dev) = dev {
			self.devidx += 1;

			let mut dev = dev.into_device();
			let devinfo = *dev.get_child_info::<PcieFunctionInfo>(0).unwrap();
			if devinfo.bus as u32 != bus || devinfo.device as u32 != device || devinfo.function as u32 != function {
				println!(
					"[pcie] failed to create child object for device {} {} {}",
					bus, device, function
				);
			} else {
				self.setup_device(info, &devinfo, dev);
			}
		}
	}

	fn init_bridge(&mut self, info: &PcieInfo, bus: u32, device: u32, function: u32) -> Option<u32> {
		let addr = ((bus - info.start_bus) as u64) << 20 | (device as u64) << 15 | (function as u64) << 12;
		let mmio = self.root.get_child_mmio::<PcieConfigSpaceBridge>(0, addr);

		if let Ok((_, mmio)) = mmio {
			Some(mmio.secondary_bus_nr as u32)
		} else {
			None
		}
	}

	fn scan_bus(&mut self, info: &PcieInfo, bus: u32) -> Vec<(u8, u32, u32, u32)> {
		let mut devices = vec![];
		'outer: for device in 0..32 {
			'inner: for function in 0..8 {
				let addr = ((bus - info.start_bus) as u64) << 20 | (device as u64) << 15 | (function as u64) << 12;
				let mmio = self.root.get_child_mmio::<PcieConfigSpaceHdr>(0, addr);

				if let Ok((_, mmio)) = mmio {
					let vendor = mmio.vendor_id;
					if vendor != 0xffff {
						/* Okay, this is a real device */
						devices.push((mmio.header_type, bus, device, function));
					}
					if function == 0 && (mmio.header_type & PCIE_HEADER_MULTIFUNCTION) == 0 {
						break 'inner;
					}
				} else {
					break 'outer;
				}
			}
		}
		devices
	}
}

impl Bus for PcieBus {
	fn get_bus_root(&self) -> &Device {
		&self.root
	}

	fn get_bus_type() -> BusType {
		BusType::Pcie
	}

	fn new(root: Device) -> Self {
		PcieBus { root: root, devidx: 0 }
	}

	fn init(&mut self) -> Result<(), TwzErr> {
		let info = *self.root.get_child_info::<PcieInfo>(0)?;

		if info.magic != PCIE_BUS_HEADER_MAGIC {
			return Err(TwzErr::Invalid);
		}

		/* the root complex uses bus 0 as its main, and we expect any bridges to be placed on this
		 * bus to give us access to other busses. */
		let mut devices = self.scan_bus(&info, 0);

		/* scan through any bridges, adding devices to the list */
		let mut i = 0;
		loop {
			let (hdr_type, bus, device, function) = devices[i];
			if hdr_type & 0x7f == 1 {
				let secondary_bus = self.init_bridge(&info, bus, device, function);
				if let Some(secondary_bus) = secondary_bus {
					let mut children = self.scan_bus(&info, secondary_bus);
					devices.append(&mut children);
				}
			}
			i += 1;
			if i >= devices.len() {
				break;
			}
		}

		/* finally, actually initialize devices */
		for (_, bus, device, function) in devices {
			self.init_device(&info, bus, device, function);
		}

		Ok(())
	}
}
