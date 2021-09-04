use crate::obj::{GTwzobj, Twzobj};

pub struct Pref<'a, R> {
	obj: GTwzobj,
	p: &'a R,
}

impl<'a, R> std::ops::Deref for Pref<'a, R> {
	type Target = R;

	fn deref(&self) -> &Self::Target {
		self.p
	}
}

impl<'a, R> Pref<'a, R> {
	pub(super) fn new<T>(obj: &Twzobj<T>, p: &'a R) -> Pref<'a, R> {
		Pref {
			obj: obj.as_generic(),
			p,
		}
	}
}

pub struct PrefMut<'a, R> {
	obj: GTwzobj,
	p: &'a mut R,
}

impl<'a, R> std::ops::Deref for PrefMut<'a, R> {
	type Target = R;

	fn deref(&self) -> &Self::Target {
		self.p
	}
}

impl<'a, R> std::ops::DerefMut for PrefMut<'a, R> {
	fn deref_mut(&mut self) -> &mut Self::Target {
		self.p
	}
}

impl<'a, R> PrefMut<'a, R> {
	pub(super) fn new<T>(obj: &Twzobj<T>, p: &'a mut R) -> PrefMut<'a, R> {
		PrefMut {
			obj: obj.as_generic(),
			p,
		}
	}
}
