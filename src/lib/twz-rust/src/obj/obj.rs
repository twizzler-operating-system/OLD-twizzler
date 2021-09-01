use super::id::ObjID;
use super::r#const::ProtFlags;
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

	pub fn transaction<O, E>(
		&self,
		_f: &(dyn Fn(Transaction) -> Result<O, E> + 'static),
	) -> Result<O, TransactionErr<E>> {
		panic!("")
	}
}
