use super::Twzobj;
use std::sync::Arc;

pub type ObjID = u128;

impl<T> Twzobj<T> {
	pub fn id(&self) -> ObjID {
		if self.internal.id == 0 {
			todo!();
		}
		self.internal.id
	}

	pub(crate) fn set_id(&mut self, id: ObjID) {
		Arc::get_mut(&mut self.internal).unwrap().id = id;
	}
}
