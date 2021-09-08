pub struct Pptr<T> {
	pub(crate) p: u64,
	_pd: std::marker::PhantomData<T>,
	_pin: std::marker::PhantomPinned,
}

impl<T> Default for Pptr<T> {
	fn default() -> Self {
		Pptr::<T>::new_null()
	}
}

impl<T: std::fmt::Debug> std::fmt::Debug for Pptr<T> {
	fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
		f.debug_struct("Pptr")
			.field("fot_entry", &self.fot_entry())
			.field("offset", &self.offset())
			.field("reference", &self.lea())
			.finish()
	}
}

impl<T> Pptr<T> {
	pub(crate) fn new(p: u64) -> Pptr<T> {
		Pptr {
			p,
			_pd: std::marker::PhantomData,
			_pin: std::marker::PhantomPinned,
		}
	}

	pub const fn new_null() -> Pptr<T> {
		Pptr {
			p: 0,
			_pd: std::marker::PhantomData,
			_pin: std::marker::PhantomPinned,
		}
	}

	pub fn is_internal(&self) -> bool {
		self.p < crate::obj::MAX_SIZE
	}

	pub fn fot_entry(&self) -> u64 {
		self.p / crate::obj::MAX_SIZE
	}

	pub fn offset(&self) -> u64 {
		self.p % crate::obj::MAX_SIZE
	}
}

impl<T> std::fmt::Pointer for Pptr<T> {
	fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
		write!(f, "{:x}", self.p)?;
		if f.alternate() {
			write!(f, "<{}>", std::any::type_name::<T>())
		} else {
			Ok(())
		}
	}
}
