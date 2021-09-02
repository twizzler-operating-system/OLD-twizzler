use super::id::ObjID;
use super::r#const::{ProtFlags, MAX_SIZE};
use super::tx::{Transaction, TransactionErr};
use crate::kso::view::View;

pub(super) const ALLOCATED: i32 = 1;

#[derive(Clone)]
pub struct Twzobj<T> {
	pub(super) id: ObjID,
	pub(crate) slot: u64,
	pub(super) flags: i32,
	pub(super) prot: ProtFlags,
	pub(super) _pd: std::marker::PhantomData<T>,
}

pub(super) type GTwzobj = Twzobj<()>;

impl<T> Drop for Twzobj<T> {
	fn drop(&mut self) {
		if self.flags & ALLOCATED != 0 {
			View::current().release_slot(self.id, self.prot, self.slot);
		}
	}
}

impl<T> Twzobj<T> {
	pub(crate) fn set_id(&mut self, id: ObjID) {
		self.id = id;
	}

	pub(crate) fn is_same_obj<X>(&self, other: &Twzobj<X>) -> bool {
		return self.slot == other.slot || self.id() == other.id();
	}

	pub(crate) fn as_generic(&self) -> GTwzobj {
		/* TODO: ******* DUPLICATE REFERENCE */
		GTwzobj {
			id: self.id,
			slot: self.slot,
			flags: 0,
			prot: self.prot,
			_pd: std::marker::PhantomData,
		}
	}

	pub(crate) fn from_ptr<R>(ptr: &R) -> Twzobj<T> {
		Twzobj {
			id: 0,
			slot: unsafe { std::mem::transmute::<&R, u64>(ptr) / MAX_SIZE },
			flags: 0,
			prot: ProtFlags::none(),
			_pd: std::marker::PhantomData,
		}
	}

	pub(crate) fn init_slot(id: ObjID, prot: ProtFlags, slot: u64, allocated: bool) -> Twzobj<T> {
		let flags = if allocated { ALLOCATED } else { 0 };
		Twzobj {
			id,
			slot,
			flags,
			prot,
			_pd: std::marker::PhantomData,
		}
	}

	pub fn init_guid(id: ObjID, prot: ProtFlags) -> Twzobj<T> {
		let slot = crate::kso::view::View::current().reserve_slot(id, prot);
		Twzobj::init_slot(id, prot, slot, true)
	}

	pub fn transaction<O, E, F>(&self, f: F) -> Result<O, TransactionErr<E>>
	where
		F: Fn(Transaction) -> Result<O, E>,
	{
		let tx = Transaction::new(self.as_generic());
		f(tx).map_err(|e| TransactionErr::Abort(e))
	}
}
