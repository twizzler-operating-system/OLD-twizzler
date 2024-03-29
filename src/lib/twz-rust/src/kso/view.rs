use crate::kso::KSO;
use crate::obj::{ObjID, ProtFlags, Twzobj, MAX_SIZE};
use std::sync::atomic::AtomicU32;
use std::sync::atomic::Ordering;

/* TODO (opt): when releasing, we could speed things up by lazily releasing slots, tracking
 * "previously released slots" in a list and occasionally releasing all of them to amortize the
 * bookkeeping cost. */

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

impl From<ProtFlags> for ViewFlags {
	fn from(v: ProtFlags) -> ViewFlags {
		ViewFlags {
			val: ViewFlags::all().bits() & v.bits(),
		}
	}
}

#[repr(C)]
struct ViewEntry {
	id: ObjID,
	res0: u64,
	flags: AtomicU32,
	lock: AtomicU32,
}

#[repr(C)]
struct ViewBucket {
	id: ObjID,
	slot: u32,
	flags: u32,
	chain: i32,
	resv: u32,
}

#[repr(C)]
struct ViewSlotAlloc {
	bitmap: [u8; NR_VIEW_ENTRIES / 8],
	buckets: [ViewBucket; NR_VIEW_BUCKETS],
	chain: [ViewBucket; NR_VIEW_ENTRIES],
}

#[repr(C)]
struct ViewData {
	fault_entry: Option<extern "C" fn()>,
	dbl_fault_entry: Option<extern "C" fn()>,
	fault_mask: u64,
	fault_flags: u64,
	entries: [ViewEntry; NR_VIEW_ENTRIES],
	exec_id: ObjID, //TODO: remove?
	lock: crate::mutex::TwzMutex<ViewSlotAlloc>,
}

pub(crate) struct View {
	kso: KSO<ViewData>,
}

const TWZSLOT_CVIEW: u64 = 0x1fff0;
const TWZSLOT_ALLOC_START: u64 = 0x10010;
const TWZSLOT_ALLOC_MAX: u64 = 0x19fff;

fn lookup_bucket(allocator: &mut ViewSlotAlloc, id: ObjID, flags: u32) -> Option<&mut ViewBucket> {
	let mut idx = (id % NR_VIEW_BUCKETS as u128) as i32;
	let mut first = true;
	loop {
		/* On first pass, we'll look through the actual hash table buckets. If we're doing more
		 * than one pass through the loop, then we are following a chain, and we take from the
		 * chain buckets. */
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
		/* On first pass, we'll look through the actual hash table buckets. If we're doing more
		 * than one pass through the loop, then we are following a chain, and we take from the
		 * chain buckets. */
		let bucket = if first {
			&allocator.buckets[idx as usize]
		} else {
			&allocator.chain[idx as usize]
		};
		/* If we find a deleted (or empty) entry in the chain somewhere, just use that. */
		if bucket.id == 0 {
			let bucket = if first {
				&mut allocator.buckets[idx as usize]
			} else {
				&mut allocator.chain[idx as usize]
			};
			bucket.id = id;
			bucket.slot = slot;
			bucket.flags = flags;
			return bucket;
		}
		prev = Some((first, idx));
		idx = bucket.chain - 1;
		if bucket.chain == 0 {
			break;
		}
		first = false;
	}
	/* Reserve a new chain entry for ourselves. This could probably be sped up with a "search from
	 * last" type deal. */
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
			kso: KSO::<ViewData> {
				obj: Twzobj::init_slot(0, ProtFlags::READ | ProtFlags::WRITE, TWZSLOT_CVIEW, false, false),
			},
		};
		let hdr = view.kso.base_data();
		let entry = &hdr.entries[TWZSLOT_CVIEW as usize];
		let id = entry.id;
		view.kso.obj.set_id(id);
		view
	}

	pub(crate) fn id(&self) -> ObjID {
		self.kso.obj.id()
	}

	pub(crate) fn lock_entry(&mut self, slot: usize) {
		let hdr = self.kso.base_data_mut();
		let entry = &mut hdr.entries[slot];
		let old = entry.lock.fetch_add(1, Ordering::SeqCst);
		if old == u32::MAX {
			panic!("failed to lock slot entry");
		}
	}

	pub(crate) fn unlock_entry(&mut self, slot: usize) -> bool {
		let hdr = self.kso.base_data_mut();
		let entry = &mut hdr.entries[slot];
		let old = entry.lock.fetch_sub(1, Ordering::SeqCst);
		if old == 0 {
			panic!("failed to unlock slot entry");
		}
		old == 1
	}

	pub(crate) fn get_slot(&mut self, slot: u64) {
		self.lock_entry(slot as usize);
	}

	pub(crate) fn put_slot(&mut self, slot: u64) {
		self.unlock_entry(slot as usize);
	}

	pub(crate) fn set_entry(&mut self, slot: usize, id: ObjID, mut flags: ViewFlags) {
		let hdr = self.kso.base_data_mut();
		let entry = &mut hdr.entries[slot];
		let old = entry.flags.fetch_and(!ViewFlags::VALID.bits(), Ordering::SeqCst);
		entry.id = id;
		entry.res0 = 0;
		if flags.contains_any(ViewFlags::WRITE) {
			flags = flags & !ViewFlags::EXEC;
		}
		entry
			.flags
			.store(flags.bits() | ViewFlags::VALID.bits(), Ordering::SeqCst);

		/* If the entry previously had a valid mapping, we need to invalidate that mapping.
		 * TODO (opt):  we should probably track a list of "to-invalidate" mappings and
		 * invalidate them all in one batch instead of one at a time. */
		if old & ViewFlags::VALID.bits() != 0 {
			let invl = crate::sys::InvalidateOp::new_current(
				crate::sys::InvalidateCurrent::View,
				slot as u64 * MAX_SIZE,
				MAX_SIZE as u32,
			);
			crate::sys::invalidate(&mut [invl]);
		}
	}

	pub(crate) fn reserve_slot(&mut self, id: ObjID, prot: ProtFlags) -> u64 {
		let (new, slot) = {
			let allocator = &mut *self.kso.base_data_mut().lock.lock();
			let bucket = lookup_bucket(allocator, id, prot.bits());
			if let Some(bucket) = bucket {
				(false, bucket.slot as u64)
			} else {
				let slot = alloc_slot(allocator);
				insert_obj(allocator, id, prot.bits(), slot);
				(true, slot as u64)
			}
		};
		self.lock_entry(slot as usize);
		if new {
			self.set_entry(slot as usize, id, prot.into());
		}
		return slot;
	}

	pub(crate) fn release_slot(&mut self, id: ObjID, prot: ProtFlags, slot: u64) {
		if self.unlock_entry(slot as usize) {
			let allocator = &mut *self.kso.base_data_mut().lock.lock();
			let bucket = lookup_bucket(allocator, id, prot.bits()).expect("tried to release slot with no bucket");
			assert!(bucket.slot == slot as u32);
			bucket.slot = 0;
			bucket.id = 0;
			bucket.flags = 0;
			dealloc_slot(allocator, slot as u32);
		}
	}

	pub(crate) fn set_upcall_entry(&self, entry: extern "C" fn(), dbl_entry: extern "C" fn()) {
		let hdr = self.kso.base_data_mut();
		hdr.fault_entry = Some(entry);
		hdr.dbl_fault_entry = Some(dbl_entry);
	}
}
