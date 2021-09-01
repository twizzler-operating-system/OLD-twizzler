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
