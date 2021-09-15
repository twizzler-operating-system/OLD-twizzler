use super::id::ObjID;
use super::obj::{GTwzobj, Twzobj};
use super::r#const::{ProtFlags, MAX_FOTE, MAX_SIZE, METAPAGE_SIZE, NULLPAGE_SIZE};
use super::tx::Transaction;
use crate::ptr::Pptr;
use crate::refs::Pref;
use std::sync::atomic::{AtomicU32, AtomicU64, Ordering};

pub type Nonce = u128;

#[repr(C)]
struct MetaExt {
	tag: AtomicU64,
	off: u64,
}

const MI_MAGIC: u32 = 0x54575A4F;

crate::bitflags! {
	pub struct MetaProtFlags: u16 {
		const HASHDATA = 0x1;
		const DFL_READ = 0x4;
		const DFL_WRITE = 0x8;
		const DFL_EXEC = 0x10;
		const DFL_USE = 0x20;
		const DFL_DEL = 0x40;
	}
}

crate::bitflags! {
	pub struct MetaFlags: u16 {
		const SIZED = 0x1;
	}
}

#[repr(C)]
pub struct MetaInfo {
	magic: u32,
	flags: u16,
	p_flags: u16,
	fot_entries: AtomicU32,
	milen: u32,
	nonce: Nonce,
	kuid: ObjID,
	sz: u64,
	pad: u64,
	exts: [MetaExt; 0],
}

crate::bitflags! {
	pub struct FOTEntryFlags: u64 {
		const READ = 0x4;
		const WRITE = 0x8;
		const EXEC = 0x10;
		const USE = 0x20;
		const NAME = 0x1000;

		const VALID = 0x20000;
	}
}

#[repr(C)]
#[derive(Clone, Copy)]
struct NameEntry {
	data: u64,
	resolver: u64,
}

#[repr(C)]
union FOTEntryInternal {
	id: ObjID,
	name: NameEntry,
}

#[repr(C)]
pub struct FOTEntry {
	obj: FOTEntryInternal,
	flags: AtomicU64,
	info: u64,
}

impl<T> Twzobj<T> {
	pub(super) fn metainfo<'a>(&'a self) -> &'a MetaInfo {
		unsafe { self.offset_lea(MAX_SIZE - NULLPAGE_SIZE / 2) }
	}

	pub(super) fn metainfo_mut<'a>(&'a self) -> &'a mut MetaInfo {
		unsafe { self.offset_lea_mut(MAX_SIZE - NULLPAGE_SIZE / 2) }
	}

	fn get_fote_ref(&self, i: u64) -> &mut FOTEntry {
		unsafe { self.offset_lea_mut(MAX_SIZE - (METAPAGE_SIZE + i * std::mem::size_of::<FOTEntry>() as u64)) }
	}

	pub(super) fn add_fote<'a, R>(&self, p: &Pref<'a, R>, _tx: &Transaction) -> u64 {
		/* TODO: use the transaction */
		for i in 1..MAX_FOTE {
			let f = self.get_fote_ref(i);
			if f.flags.load(Ordering::SeqCst) & FOTEntryFlags::VALID.bits() == 0 {
				f.obj = FOTEntryInternal { id: p.obj.id() };
				f.flags.store(
					(FOTEntryFlags::READ | FOTEntryFlags::WRITE | FOTEntryFlags::VALID).bits(),
					Ordering::SeqCst,
				);
				f.info = 0;
				return i;
			}
		}
		panic!("out of FOT entries");
	}

	pub(super) fn resolve_external_ref<'a, R>(&self, ptr: &Pptr<R>) -> Pref<'a, R> {
		let f = self.get_fote_ref(ptr.fot_entry());
		let flags = f.flags.load(Ordering::SeqCst);
		if flags & FOTEntryFlags::VALID.bits() == 0 {
			panic!("tried to resolve an external pointer referencing an invalid FOT entry");
		}
		let id = if flags & FOTEntryFlags::NAME.bits() != 0 {
			todo!()
		} else {
			unsafe { f.obj.id }
		};

		let obj = GTwzobj::init_guid(id, ProtFlags::from_bits(flags as u32));
		let off = unsafe { obj.offset_lea(ptr.offset()) };
		Pref::new(&obj, off)
	}

	pub(super) fn find_metaext_mut<'a, R>(&'a self, tag: u64) -> Option<&mut R> {
		let mi = self.metainfo();
		let exts = mi.exts.as_ptr();
		let mut i = 0;
		loop {
			unsafe {
				let ext = exts.offset(i).as_ref().unwrap();
				let t = ext.tag.load(std::sync::atomic::Ordering::SeqCst);
				if t == tag {
					return Some(self.offset_lea_mut(ext.off));
				} else if t == 0 {
					return None;
				}
			}
			i += 1;
		}
	}

	pub(super) fn find_metaext<'a, R>(&'a self, tag: u64) -> Option<&R> {
		let mi = self.metainfo();
		let exts = mi.exts.as_ptr();
		let mut i = 0;
		loop {
			unsafe {
				let ext = exts.offset(i).as_ref().unwrap();
				let t = ext.tag.load(std::sync::atomic::Ordering::SeqCst);
				if t == tag {
					return Some(self.offset_lea(ext.off));
				} else if t == 0 {
					return None;
				}
			}
			i += 1;
		}
	}

	pub(super) unsafe fn alloc_metaext_unchecked<R: Default>(&self, tag: u64) {
		let mi = self.metainfo_mut();
		let exts = mi.exts.as_mut_ptr();
		let mut i = 0;
		loop {
			let ext = exts.offset(i).as_mut().unwrap();
			let t = ext.tag.load(std::sync::atomic::Ordering::SeqCst);
			if t == 0 {
				self.allocate_copy_item(&mut ext.off, R::default());
				ext.tag.store(tag, std::sync::atomic::Ordering::SeqCst);
				break;
			}
			i += 1;
		}
	}
}
