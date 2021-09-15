use crate::obj::r#const::MAX_SIZE;
use crate::obj::{GTwzobj, Twzobj};

use std::fmt::{Debug, Error, Formatter};
use std::marker::PhantomData;
use std::ops::{Deref, DerefMut};

pub struct Pref<'a, R> {
	pub(crate) obj: GTwzobj,
	reference: &'a R,
	_pd: PhantomData<&'a ()>,
}

impl<'a, R> Deref for Pref<'a, R> {
	type Target = R;

	fn deref(&self) -> &Self::Target {
		self.reference
	}
}

impl<'a, R> Pref<'a, R> {
	pub(super) fn as_ref(p: &'a Pref<'a, R>) -> &'a R {
		&**p
	}
}

impl<'a, R> Pref<'a, R> {
	/* TODO: get rid of / clean up this */
	pub(crate) unsafe fn into_ref(p: Pref<'a, R>) -> &'a R {
		p.reference
	}

	pub(super) fn new<T>(obj: &Twzobj<T>, p: &'a R) -> Pref<'a, R> {
		Pref {
			obj: obj.as_generic(),
			reference: p,
			_pd: PhantomData,
		}
	}

	pub(super) fn local(p: &Pref<'a, R>) -> u64 {
		unsafe { std::mem::transmute::<&R, u64>(p.reference) & (MAX_SIZE - 1) }
	}
}

impl<'a, R> Debug for Pref<'a, R>
where
	R: Debug + 'a,
{
	fn fmt(&self, fmt: &mut Formatter<'_>) -> Result<(), Error> {
		fmt.debug_struct("Pref")
			.field("object", &self.obj)
			.field("offset", &Pref::local(self))
			.field("reference", &*self)
			.finish()
	}
}

impl<'a, R> Clone for Pref<'a, R> {
	fn clone(&self) -> Pref<'a, R> {
		Pref::new(&self.obj, self.reference)
	}
}

pub struct PrefMut<'a, R> {
	pub(crate) obj: GTwzobj,
	reference: &'a mut R,
	_pd: PhantomData<&'a ()>,
}

impl<'a, R> Deref for PrefMut<'a, R> {
	type Target = R;

	fn deref(&self) -> &Self::Target {
		self.reference
	}
}

impl<'a, R> DerefMut for PrefMut<'a, R> {
	fn deref_mut(&mut self) -> &mut Self::Target {
		self.reference
	}
}

impl<'a, R> PrefMut<'a, R> {
	pub(super) fn as_ref(p: &'a mut PrefMut<'a, R>) -> &'a mut R {
		&mut **p
	}
}

impl<'a, R> PrefMut<'a, R> {
	pub(super) fn new<T>(obj: &Twzobj<T>, p: &'a mut R) -> PrefMut<'a, R> {
		PrefMut {
			obj: obj.as_generic(),
			reference: p,
			_pd: PhantomData,
		}
	}

	pub(super) fn local(p: &PrefMut<'a, R>) -> u64 {
		unsafe { std::mem::transmute::<&R, u64>(p.reference as &R) & (MAX_SIZE - 1) }
	}
}

impl<'a, R> Debug for PrefMut<'a, R>
where
	R: Debug + 'a,
{
	fn fmt(&self, fmt: &mut Formatter<'_>) -> Result<(), Error> {
		fmt.debug_struct("PrefMut")
			.field("object", &self.obj)
			.field("offset", &PrefMut::local(self))
			.field("reference", &*self)
			.finish()
	}
}
