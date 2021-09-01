use super::id::ObjID;
use super::r#const::ProtFlags;
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
