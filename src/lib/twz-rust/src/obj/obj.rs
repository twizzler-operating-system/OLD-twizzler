use super::id::ObjID;
use super::r#const::{ProtFlags, MAX_SIZE};
use super::tx::{Transaction, TransactionErr};
use crate::kso::view::View;
use std::sync::Arc;

pub(super) const ALLOCATED: i32 = 1;
pub(super) const LOCKED: i32 = 2;

#[derive(Debug)]
pub(crate) struct ObjInternal {
	pub(super) id: ObjID,
	pub(super) slot: u64,
	flags: i32,
	prot: ProtFlags,
}

#[derive(Clone, Debug)]
pub struct Twzobj<T> {
	pub(super) internal: Arc<ObjInternal>,
	_pd: std::marker::PhantomData<T>,
}

pub(crate) type GTwzobj = Twzobj<()>;

impl Drop for ObjInternal {
	fn drop(&mut self) {
		if self.flags & ALLOCATED != 0 {
			View::current().release_slot(self.id, self.prot, self.slot);
		} else if self.flags & LOCKED != 0 {
			View::current().put_slot(self.slot);
		}
	}
}

fn slot_from_ptr<R>(p: &R) -> u64 {
	let p = unsafe { std::mem::transmute::<&R, u64>(p) };
	p / MAX_SIZE
}

impl<T> Twzobj<T> {
	pub(crate) fn is_same_obj<X>(&self, other: &Twzobj<X>) -> bool {
		return self.internal.slot == other.internal.slot || self.id() == other.id();
	}

	pub(crate) fn as_generic(&self) -> GTwzobj {
		GTwzobj {
			internal: Arc::clone(&self.internal),
			_pd: std::marker::PhantomData,
		}
	}

	/* TODO: audit this and make it unsafe? */
	pub(crate) fn from_ptr<R>(ptr: &R) -> Twzobj<T> {
		let slot = slot_from_ptr(ptr);
		Self::init_slot(0, ProtFlags::none(), slot, false, false)
	}

	pub(crate) fn from_ptr_locked<R>(ptr: &R) -> Twzobj<T> {
		let slot = slot_from_ptr(ptr);
		crate::kso::view::View::current().get_slot(slot);
		Self::init_slot(0, ProtFlags::none(), slot, false, true)
	}

	pub(crate) fn init_slot(id: ObjID, prot: ProtFlags, slot: u64, allocated: bool, locked: bool) -> Twzobj<T> {
		if allocated && locked {
			panic!("slot cannot be both locked and allocated at the same time");
		}
		let flags = if allocated {
			ALLOCATED
		} else if locked {
			LOCKED
		} else {
			0
		};
		Twzobj {
			internal: Arc::new(ObjInternal { id, slot, flags, prot }),
			_pd: std::marker::PhantomData,
		}
	}

	pub fn init_guid(id: ObjID, prot: ProtFlags) -> Twzobj<T> {
		let slot = crate::kso::view::View::current().reserve_slot(id, prot);
		Twzobj::init_slot(id, prot, slot, true, false)
	}

	pub fn transaction<O, E, F>(&self, f: F) -> Result<O, TransactionErr<E>>
	where
		F: Fn(Transaction) -> Result<O, E>,
	{
		let tx = Transaction::new(self.as_generic());
		f(tx).map_err(|e| TransactionErr::Abort(e))
	}
}
