use crate::ptr::Pptr;

pub struct Vec<T> {
	_buf: Pptr<T>,
	_len: usize,
	_cap: usize,
}

impl<T> Vec<T> {
	pub const fn new() -> Self {
		Vec {
			_buf: Pptr::new_null(),
			_len: 0,
			_cap: 0,
		}
	}
}
