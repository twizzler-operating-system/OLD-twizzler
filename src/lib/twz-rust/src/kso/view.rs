use crate::kso::KSOHdr;
use crate::obj::{ObjID, ProtFlags, Twzobj};

const NR_VIEW_ENTRIES: usize = 0x20000;
const NR_VIEW_BUCKETS: usize = 1024;

crate::bitflags! {
	pub struct ViewFlags: u32 {
		const READ = 0x4;
		const WRITE = 0x8;
		const EXEC = 0x10;
		const VALID = 0x1000;
	}
}

#[repr(C)]
struct ViewEntry {
	id: ObjID,
	res0: u64,
	flags: u32,
	res1: u32,
}

#[repr(C)]
struct ViewBucket {
	id: ObjID,
	slot: u32,
	flags: u32,
	chain: i32,
	refs: u32,
}

#[repr(C)]
struct ViewSlotAlloc {
	bitmap: [u8; NR_VIEW_ENTRIES / 8],
	buckets: [ViewBucket; NR_VIEW_BUCKETS],
	chain: [ViewBucket; NR_VIEW_ENTRIES],
}

#[repr(C)]
struct ViewHdr {
	hdr: KSOHdr,
	fault_entry: Option<extern "C" fn()>,
	dbl_fault_entry: Option<extern "C" fn()>,
	fault_mask: u64,
	fault_flags: u64,
	entries: [ViewEntry; NR_VIEW_ENTRIES],
	exec_id: ObjID, //TODO: remove?
	lock: crate::mutex::TwzMutex<ViewSlotAlloc>,
}

pub(crate) struct View {
	obj: Twzobj<ViewHdr>,
}

const TWZSLOT_CVIEW: u64 = 0x1fff0;

impl View {
	pub(crate) fn current() -> View {
		let mut view = View {
			obj: Twzobj::init_slot(0 /*TODO: get ID */, TWZSLOT_CVIEW, false),
		};
		let hdr = view.obj.base(None);
		let entry = &hdr.entries[TWZSLOT_CVIEW as usize];
		let id = entry.id;
		view.obj.set_id(id);
		view
	}

	pub(crate) fn reserve_slot(&self, _id: ObjID, _prot: ProtFlags) -> u64 {
		panic!("")
	}

	pub(crate) fn release_slot(&self, _slot: u64) {
		panic!("")
	}

	pub(crate) fn set_upcall_entry(&self, entry: extern "C" fn(), dbl_entry: extern "C" fn()) {
		let hdr = unsafe { self.obj.base_unchecked_mut() };
		hdr.fault_entry = Some(entry);
		hdr.dbl_fault_entry = Some(dbl_entry);
	}
}
