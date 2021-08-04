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
const TWZSLOT_ALLOC_START: u64 = 0x10010;
const TWZSLOT_ALLOC_MAX: u64 = 0x19fff;

fn lookup_bucket(allocator: &mut ViewSlotAlloc, id: ObjID, flags: u32) -> Option<&mut ViewBucket> {
	let mut idx = (id % NR_VIEW_BUCKETS as u128) as i32;
	let mut first = true;
	loop {
		let bucket = if first {
			&allocator.buckets[idx as usize]
		} else {
			&allocator.chain[idx as usize]
		};
		if bucket.id == id && bucket.flags == flags {
			let bucket = if first {
				&mut allocator.buckets[idx as usize]
			} else {
				&mut allocator.chain[idx as usize]
			};
			return Some(bucket);
		}
		idx = bucket.chain - 1;
		first = false;
		if idx == -1 {
			break;
		}
	}
	return None;
}

fn insert_obj(allocator: &mut ViewSlotAlloc, id: ObjID, flags: u32, slot: u32) -> &mut ViewBucket {
	let mut idx = (id % NR_VIEW_BUCKETS as u128) as i32;
	let mut prev;
	let mut first = true;
	loop {
		let bucket = if first {
			&allocator.buckets[idx as usize]
		} else {
			&allocator.chain[idx as usize]
		};
		if bucket.id == 0 {
			let bucket = if first {
				&mut allocator.buckets[idx as usize]
			} else {
				&mut allocator.chain[idx as usize]
			};
			bucket.id = id;
			bucket.slot = slot;
			bucket.flags = flags;
			bucket.refs = 1;
			return bucket;
		}
		prev = Some((first, idx));
		idx = bucket.chain - 1;
		if bucket.chain == 0 {
			break;
		}
		first = false;
	}
	for i in 0..NR_VIEW_ENTRIES {
		let bucket = &allocator.chain[i];
		if bucket.id == 0 && bucket.chain == 0 {
			let (b, pidx) = prev.expect("searching view chain with no prev");
			let bucket = if b {
				&mut allocator.buckets[pidx as usize]
			} else {
				&mut allocator.chain[pidx as usize]
			};
			bucket.chain = i as i32 + 1;

			let bucket = &mut allocator.chain[i];
			bucket.id = id;
			bucket.flags = flags;
			bucket.slot = slot;
			bucket.refs = 1;
			return bucket;
		}
	}
	panic!("failed to allocate bucket for view entry");
}

fn alloc_slot(allocator: &mut ViewSlotAlloc) -> u32 {
	for i in (TWZSLOT_ALLOC_START / 8)..((TWZSLOT_ALLOC_MAX / 8) + 1) {
		let idx = i as usize;
		if allocator.bitmap[idx] != 0xff {
			for bit in 0..8 {
				if (allocator.bitmap[idx] & (1u8 << bit)) == 0 {
					allocator.bitmap[idx] |= 1u8 << bit;
					return (i * 8 + bit) as u32;
				}
			}
		}
	}
	panic!("out of slots");
}

fn dealloc_slot(allocator: &mut ViewSlotAlloc, slot: u32) {
	allocator.bitmap[slot as usize / 8] &= !(1 << ((slot % 8) as u8));
}

impl View {
	pub(crate) fn current() -> View {
		let mut view = View {
			obj: Twzobj::init_slot(0, ProtFlags::READ | ProtFlags::WRITE, TWZSLOT_CVIEW, false),
		};
		let hdr = view.obj.base(None);
		let entry = &hdr.entries[TWZSLOT_CVIEW as usize];
		let id = entry.id;
		view.obj.set_id(id);
		view
	}

	pub(crate) fn set_entry(entry: usize, id: ObjID, flags: ViewFlags) {
		panic!("")
	}

	pub(crate) fn reserve_slot(&self, id: ObjID, prot: ProtFlags) -> u64 {
		let allocator = &mut *unsafe { self.obj.base_unchecked_mut() }.lock.lock();
		let bucket = lookup_bucket(allocator, id, prot.bits());
		if let Some(bucket) = bucket {
			bucket.refs += 1;
			return bucket.slot as u64;
		} else {
			let slot = alloc_slot(allocator);
			insert_obj(allocator, id, prot.bits(), slot);
			return slot as u64;
		}
	}

	pub(crate) fn release_slot(&self, id: ObjID, prot: ProtFlags, slot: u64) {
		let allocator = &mut *unsafe { self.obj.base_unchecked_mut() }.lock.lock();
		let bucket = lookup_bucket(allocator, id, prot.bits()).expect("tried to release slot with no bucket");
		assert!(bucket.slot == slot as u32);
		let old = bucket.refs;
		assert!(old > 0);
		bucket.refs -= 1;
		if old == 1 {
			bucket.slot = 0;
			bucket.id = 0;
			bucket.flags = 0;
			dealloc_slot(allocator, slot as u32);
		}
	}

	pub(crate) fn set_upcall_entry(&self, entry: extern "C" fn(), dbl_entry: extern "C" fn()) {
		let hdr = unsafe { self.obj.base_unchecked_mut() };
		hdr.fault_entry = Some(entry);
		hdr.dbl_fault_entry = Some(dbl_entry);
	}
}
