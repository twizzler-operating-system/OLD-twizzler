use super::Twzobj;

pub type ObjID = u128;

impl<T> Twzobj<T> {
	pub fn id(&self) -> ObjID {
		self.id
	}
}
