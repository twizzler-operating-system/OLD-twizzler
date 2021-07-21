use crate::kso::{KSOAttachIterator, KSODirAttachments, KSOHdr, KSOType, KSO};
use crate::obj::Twzobj;
use crate::TwzErr;

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

#[repr(u64)]

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

pub struct KSODevice {
	pub hdr: KSOHdr,
	pub bustype: u64,
	pub devtype: u64,
	pub devid: u64,
	pub syncs: [std::sync::atomic::AtomicU64; MAX_DEVICE_SYNCS],
	pub interrupts: [DeviceInterrupt; MAX_DEVICE_INTERRUPTS],
	pub attached: KSODirAttachments,
}

pub struct Device {
	kso: KSO,
	children: Vec<Vec<Option<KSO>>>,
}

#[repr(C)]

pub struct DeviceMMIOHdr {
	hdr: KSOHdr,
	pub info: u64,
	pub flags: u64,
	pub length: u64,
	pub resv: u64,
}

impl KSO {
	pub fn into_device(self) -> Device {
		Device {
			kso: self,
			children: vec![vec![], vec![], vec![]],
		}
	}
}

use std::convert::TryFrom;

impl Device {
	pub fn get_kso_name(&self) -> &str {
		self.kso.name()
	}

	pub fn get_children<'a>(&'a self) -> KSOAttachIterator<'a> {
		self.kso.get_dir().unwrap().into_iter()
	}

	fn get_child_obj<'a>(&'a mut self, idx: usize, chtype: usize) -> Result<&'a KSO, TwzErr> {
		if idx >= self.children[chtype].len() {
			self.children[chtype].resize(idx + 1, None);
		}

		if self.children[chtype][idx].is_none() {
			let mut count = 0;

			let mut obj: Option<KSO> = None;

			for child in self.get_children() {
				if child.id != 0 && (child.info & 0xffffffff) == chtype as u64 && child.attype == KSOType::Data as u32 {
					if count == idx {
						obj = Some(KSO::try_from(child)?);
					}
				}
			}

			self.children[chtype][idx] = obj;
		}

		if let Some(ref obj) = self.children[chtype][idx] {
			Ok(obj)
		} else {
			Err(TwzErr::Invalid)
		}
	}

	pub fn get_child_mmio<'a, T>(&'a mut self, idx: usize) -> Result<(&'a DeviceMMIOHdr, &'a mut T), TwzErr> {
		let kso = self.get_child_obj(idx, DEVICE_CHILD_MMIO as usize)?;

		let t = unsafe { kso.obj.offset_lea_mut::<T>(crate::obj::OBJ_NULLPAGE_SIZE * 2) };

		let h = kso.obj.base::<DeviceMMIOHdr>();

		Ok((h, t))
	}

	pub fn get_child_info<'a, T>(&'a mut self, idx: usize) -> Result<&'a T, TwzErr> {
		let kso = self.get_child_obj(idx, DEVICE_CHILD_INFO as usize)?;

		let h = kso.obj.base::<T>();

		Ok(h)
	}

	pub fn get_child_device(&self, idx: usize) -> Result<KSO, TwzErr> {
		let mut count = 0;

		for child in self.get_children() {
			if child.id != 0
				&& (child.info & 0xffffffff) == DEVICE_CHILD_DEVICE
				&& child.attype == KSOType::Device as u32
			{
				if count == idx {
					let obj = KSO::try_from(child)?;

					return Ok(obj);
				}
			}
		}

		Err(TwzErr::Invalid)
	}

	pub fn get_device_hdr(&self) -> &KSODevice {
		self.kso.obj.base::<KSODevice>()
	}
}
