pub mod view;

use crate::flexarray::{FlexArray, FlexArrayField};
use crate::obj::{ObjID, Twzobj};
use crate::TwzErr;
use std::convert::TryFrom;

#[derive(Copy, Clone)]
pub enum KSOType {
	None = 0,
	View = 1,
	SecCtx = 2,
	Thread = 3,
	Root = 4,
	Device = 5,
	Directory = 6,
	Data = 7,
	Max = 8,
}

const KSO_NAME_MAXLEN: usize = 1024;

#[repr(C)]
#[derive(Debug)]
pub struct KSOAttachment {
	pub(crate) id: ObjID,
	pub(crate) info: u64,
	pub(crate) attype: u32,
	pub(crate) flags: u32,
}

#[derive(Debug, Copy, Clone)]
#[repr(C)]
pub struct KSOHdr {
	name: [u8; KSO_NAME_MAXLEN],
	version: u32,
	dir_offset: u32,
	resv2: u64,
}

#[repr(C)]
pub struct KSODirAttachments {
	pub(crate) flags: u64,
	pub(crate) count: u64,
	pub(crate) children: FlexArrayField<KSOAttachment>,
}

impl KSODirAttachments {
	pub fn len(&self) -> usize {
		self.count as usize
	}
}

#[repr(C)]
pub struct KSORootHdr {
	pub hdr: KSOHdr,
	pub attached: KSODirAttachments,
}

#[repr(C)]
pub struct KSODirHdr {
	pub hdr: KSOHdr,
	pub attached: KSODirAttachments,
}

#[derive(Clone)]
pub struct KSO {
	pub(crate) obj: Twzobj<KSOHdr>,
}

pub struct KSOAttachIterator<'a> {
	count: usize,
	curr: usize,
	attach: &'a [KSOAttachment],
}

impl TryFrom<&KSOAttachment> for KSO {
	type Error = TwzErr;
	fn try_from(at: &KSOAttachment) -> Result<Self, Self::Error> {
		if at.id != 0 {
			Ok(KSO {
				obj: Twzobj::init_guid(at.id, crate::obj::ProtFlags::Read),
			})
		} else {
			Err(TwzErr::Invalid)
		}
	}
}

use std::convert::TryInto;
impl KSO {
	pub fn name(&self) -> &str {
		let hdr = self.obj.base(None);
		unsafe {
			std::ffi::CStr::from_ptr(((&hdr.name) as *const u8) as *const i8)
				.to_str()
				.unwrap()
		}
	}

	pub fn get_dir(&self) -> Option<&KSODirAttachments> {
		let hdr = self.obj.base(None);
		if hdr.dir_offset == 0 {
			None
		} else {
			unsafe {
				Some(
					self.obj
						.offset_lea::<KSODirAttachments>(crate::obj::NULLPAGE_SIZE + hdr.dir_offset as u64),
				)
			}
		}
	}

	pub fn get_subtree(&self, ty: KSOType) -> Option<KSO> {
		if let Some(dir) = self.get_dir() {
			for at in dir {
				if at.info == ty as u64 {
					let kso: KSO = at.try_into().unwrap();
					return Some(kso);
				}
			}
		};
		None
	}
}

pub fn get_root() -> KSO {
	KSO {
		obj: Twzobj::init_guid(KSO_ROOT_ID, crate::obj::ProtFlags::Read),
	}
}

impl<'a> Iterator for KSOAttachIterator<'a> {
	type Item = &'a KSOAttachment;
	fn next(&mut self) -> Option<Self::Item> {
		if self.curr >= self.count {
			None
		} else {
			let ret = &self.attach[self.curr];
			self.curr += 1;
			if ret.id == 0 {
				self.next()
			} else {
				Some(ret)
			}
		}
	}
}

impl FlexArray<KSOAttachment> for KSODirAttachments {
	fn len(&self) -> usize {
		self.count as usize
	}
	fn flex_element(&self) -> &FlexArrayField<KSOAttachment> {
		&self.children
	}
}

impl<'a> IntoIterator for &'a KSODirAttachments {
	type Item = &'a KSOAttachment;
	type IntoIter = KSOAttachIterator<'a>;

	fn into_iter(self) -> Self::IntoIter {
		KSOAttachIterator {
			count: self.count as usize,
			curr: 0,
			attach: self.as_slice(),
		}
	}
}

pub const KSO_ROOT_ID: ObjID = 1;
