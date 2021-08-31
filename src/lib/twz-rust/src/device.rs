use crate::kso::{KSOAttachIterator, KSODirAttachments, KSOHdr, KSOType, KSO};
use crate::obj::ProtFlags;

#[repr(C)]
pub struct DeviceInterrupt {
	local: u16,
	vector: u16,
	flags: u32,
	sync: std::sync::atomic::AtomicU64,
}

pub const MAX_DEVICE_SYNCS: usize = 2;
pub const MAX_DEVICE_INTERRUPTS: usize = 32;
pub const DEVICE_CHILD_DEVICE: u64 = 0;
pub const DEVICE_CHILD_MMIO: u64 = 1;
pub const DEVICE_CHILD_INFO: u64 = 2;
pub const DEVICE_ID_SERIAL: u64 = 2;

#[repr(u64)]
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum BusType {
	Isa = 0,
	Pcie = 1,
	Usb = 2,
	Misc = 3,
	NV = 4,
	System = 1024,
}

impl BusType {
	pub fn from_u64(x: u64) -> BusType {
		unsafe { std::mem::transmute(x) }
	}
}

#[repr(C)]

pub struct DeviceData {
	pub bustype: u64,
	pub devtype: u64,
	pub devid: u64,
	pub syncs: [std::sync::atomic::AtomicU64; MAX_DEVICE_SYNCS],
	pub interrupts: [DeviceInterrupt; MAX_DEVICE_INTERRUPTS],
	pub attached: KSODirAttachments,
}

pub struct Device {
	kso: KSO<DeviceData>,
}

#[repr(C)]
pub struct DeviceMMIOData {
	pub info: u64,
	pub flags: u64,
	pub length: u64,
	pub resv: u64,
}

type DeviceMMIOHdr = KSOHdr<DeviceMMIOData>;

impl KSO<DeviceData> {
	pub fn into_device(self) -> Device {
		Device { kso: self }
	}
}

pub struct DeviceEventsIter<'a> {
	idx: usize,
	device: &'a Device,
}

#[derive(Debug)]
pub enum DeviceEvent {
	DeviceSync(usize, u64),
	DeviceInterrupt(usize, u16, u64),
}

impl<'a> Iterator for DeviceEventsIter<'a> {
	type Item = DeviceEvent;
	fn next(&mut self) -> Option<Self::Item> {
		if self.idx < MAX_DEVICE_SYNCS {
			let idx = self.idx;
			let val = self.device.get_device_hdr().syncs[idx].swap(0, std::sync::atomic::Ordering::SeqCst);
			self.idx += 1;
			if val == 0 {
				return self.next();
			}
			Some(DeviceEvent::DeviceSync(idx, val))
		} else if self.idx < (MAX_DEVICE_SYNCS + MAX_DEVICE_INTERRUPTS) {
			let idx = self.idx - MAX_DEVICE_SYNCS;
			let val = self.device.get_device_hdr().interrupts[idx]
				.sync
				.swap(0, std::sync::atomic::Ordering::SeqCst);
			self.idx += 1;
			if val == 0 {
				return self.next();
			}
			Some(DeviceEvent::DeviceInterrupt(
				idx,
				self.device.get_device_hdr().interrupts[idx].local,
				val,
			))
		} else {
			None
		}
	}
}

impl Device {
	pub fn check_for_events<'a>(&'a self) -> DeviceEventsIter<'a> {
		DeviceEventsIter { device: self, idx: 0 }
	}

	pub fn wait_for_event(&self) {
		use crate::sys::thread_sync;
		use crate::sys::ThreadSyncArgs;
		let mut vec = vec![];
		let hdr = self.get_device_hdr();
		for i in 0..MAX_DEVICE_SYNCS {
			let ts = ThreadSyncArgs::new_sleep(&hdr.syncs[i], 0);
			vec.push(ts);
		}

		for i in 0..MAX_DEVICE_INTERRUPTS {
			let ts = ThreadSyncArgs::new_sleep(&hdr.interrupts[i].sync, 0);
			vec.push(ts);
		}

		thread_sync(&mut vec, None);
	}

	pub fn get_kso_name(&self) -> &str {
		self.kso.name()
	}

	pub fn kaction(&self, cmd: i64, arg: i64) -> i32 {
		let op = crate::sys::KactionOp::new(self.kso.obj.id(), cmd, arg);
		let mut ops = [op];
		let result = crate::sys::kaction(&mut ops);
		return if result == 0 { ops[0].result() } else { result } as i32;
	}

	pub fn get_children<'a>(&'a self) -> KSOAttachIterator<'a> {
		self.kso.get_dir().unwrap().into_iter()
	}

	pub fn get_child_mmio(&mut self, idx: usize) -> Option<KSO<DeviceMMIOData>> {
		let mut count = 0;
		for child in self.get_children() {
			if child.id != 0
				&& (child.info & 0xffffffff) == DEVICE_CHILD_MMIO as u64
				&& child.attype == KSOType::Data as u32
			{
				if count == idx {
					let kso = child
						.into_kso::<DeviceMMIOData, { KSOType::Data }>(ProtFlags::READ | ProtFlags::WRITE)
						.unwrap();
					return Some(kso);
				}
				count += 1;
			}
		}
		None
	}

	/*
	pub fn get_child_mmio<'a, T>(
		&'a mut self,
		idx: usize,
		offset: u64,
	) -> Result<(&'a KSOHdr<DeviceMMIOData>, &'a mut T), TwzErr> {
		let kso = self.get_child_obj(idx, DEVICE_CHILD_MMIO as usize)?;

		let h = kso.obj.base(None);
		if offset >= h.length {
			return Err(TwzErr::Invalid);
		}
		let t = unsafe { kso.obj.offset_lea_mut::<T>(crate::obj::NULLPAGE_SIZE * 2 + offset) };

		Ok((h, t))
	}
	*/

	pub fn get_child_info<T>(&mut self, idx: usize) -> Option<KSO<T>> {
		let mut count = 0;
		for child in self.get_children() {
			if child.id != 0
				&& (child.info & 0xffffffff) == DEVICE_CHILD_INFO as u64
				&& child.attype == KSOType::Data as u32
			{
				if count == idx {
					let kso = child.into_kso::<T, { KSOType::Data }>(ProtFlags::READ).unwrap();
					return Some(kso);
				}
				count += 1;
			}
		}
		None
	}

	pub fn get_child_device(&self, idx: usize) -> Option<KSO<DeviceData>> {
		let mut count = 0;

		for child in self.get_children() {
			if child.id != 0
				&& (child.info & 0xffffffff) == DEVICE_CHILD_DEVICE
				&& child.attype == KSOType::Device as u32
			{
				if count == idx {
					let kso = child
						.into_kso::<DeviceData, { KSOType::Device }>(ProtFlags::READ | ProtFlags::WRITE)
						.unwrap();
					return Some(kso);
				}
				count += 1;
			}
		}
		None
	}

	pub fn get_device_hdr(&self) -> &DeviceData {
		self.kso.base_data()
	}
}

impl KSO<DeviceMMIOData> {
	pub fn access_offset<T>(&self, off: u64) -> &mut T {
		unsafe { self.obj.offset_lea_mut::<T>(crate::obj::NULLPAGE_SIZE * 2 + off) }
	}
}
