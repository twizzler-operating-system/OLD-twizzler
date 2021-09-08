pub mod view;

use crate::flexarray::{FlexArray, FlexArrayField};
use crate::obj::{ObjID, Twzobj};

#[repr(u32)]
#[derive(Copy, Clone, PartialEq, Eq)]
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
pub struct KSOHdr<T> {
	name: [u8; KSO_NAME_MAXLEN],
	version: u32,
	dir_offset: u32,
	resv2: u64,
	pub(crate) kso_specific: T,
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

/*
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
*/

pub struct KSO<T> {
	pub(crate) obj: Twzobj<KSOHdr<T>>,
}

pub struct KSOAttachIterator<'a> {
	count: usize,
	curr: usize,
	attach: &'a [KSOAttachment],
}

/*
impl<T> TryFrom<&KSOAttachment> for KSO<T> {
	type Error = TwzErr;
	fn try_from(at: &KSOAttachment) -> Result<Self, Self::Error> {
		if at.id != 0 {
			Ok(KSO::<T> {
				obj: Twzobj::init_guid(at.id, crate::obj::ProtFlags::READ),
			})
		} else {
			Err(TwzErr::Invalid)
		}
	}
}
*/

impl KSOAttachment {
	pub fn into_generic_kso(&self) -> KSO<()> {
		KSO::<()> {
			obj: Twzobj::init_guid(self.id, crate::obj::ProtFlags::READ),
		}
	}

	pub fn into_kso<T, const TYPE: KSOType>(&self, flags: crate::obj::ProtFlags) -> Option<KSO<T>> {
		if self.attype == TYPE as u32 {
			Some(KSO::<T> {
				obj: Twzobj::init_guid(self.id, flags),
			})
		} else {
			None
		}
	}
}

impl<T> KSO<T> {
	pub fn id(&self) -> ObjID {
		self.obj.id()
	}

	pub fn name(&self) -> &str {
		let hdr = self.obj.base();
		unsafe {
			std::ffi::CStr::from_ptr(((&hdr.name) as *const u8) as *const i8)
				.to_str()
				.unwrap()
		}
	}

	pub fn base(&self) -> &KSOHdr<T> {
		crate::refs::Pref::into_ref(self.obj.base())
	}

	pub fn base_data(&self) -> &T {
		let hdr = crate::refs::Pref::into_ref(self.obj.base());
		&hdr.kso_specific
	}

	pub fn base_data_mut(&self) -> &mut T {
		let hdr = unsafe { self.obj.base_unchecked_mut() };
		&mut hdr.kso_specific
	}

	pub fn get_dir(&self) -> Option<&KSODirAttachments> {
		let hdr = self.obj.base();
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

	pub fn get_subtree(&self, ty: KSOType) -> Option<KSO<KSODirAttachments>> {
		if let Some(dir) = self.get_dir() {
			for at in dir {
				if at.info == ty as u64 {
					return at.into_kso::<KSODirAttachments, { KSOType::Directory }>(crate::obj::ProtFlags::READ);
				}
			}
		};
		None
	}
}

pub fn get_root() -> KSO<KSODirAttachments> {
	KSO::<KSODirAttachments> {
		obj: Twzobj::init_guid(KSO_ROOT_ID, crate::obj::ProtFlags::READ),
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
