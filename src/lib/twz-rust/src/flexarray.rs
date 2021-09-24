#[derive(Default)]
pub struct FlexArrayField<T>([T; 0]);

pub trait FlexArray<T> {
	fn len(&self) -> usize;
	fn flex_element<'r>(&'r self) -> &'r FlexArrayField<T>;

	fn as_slice<'r>(&'r self) -> &'r [T] {
		use std::mem::transmute;

		unsafe { transmute::<&[T], _>(std::slice::from_raw_parts(transmute(self.flex_element()), self.len())) }
	}
}

pub(crate) unsafe fn flexarray_get_array_start<T, R>(item: &T) -> *const R
where
	T: Sized,
{
	let item = item as *const T;
	core::mem::transmute::<*const T, *const R>(item.offset(1))
}

pub(crate) unsafe fn flexarray_get_array_start_mut<T, R>(item: &mut T) -> *mut R
where
	T: Sized,
{
	let item = item as *mut T;
	core::mem::transmute::<*mut T, *mut R>(item.offset(1))
}
