use crate::obj::r#const::MAX_SIZE;
use crate::obj::{GTwzobj, Twzobj};

use std::fmt::{Debug, Error, Formatter};
use std::marker::PhantomData;
use std::mem::ManuallyDrop;
use std::ops::{Deref, DerefMut};

#[repr(C)]
union PrefReference<R>
where
	R: Deref,
{
	reference: ManuallyDrop<R>,
	address: u64,
}

pub struct Pref<'a, R>
where
	R: Deref + 'a,
{
	pub(crate) obj: GTwzobj,
	managed_ref: PrefReference<R>,
	_pd: PhantomData<&'a ()>,
}

impl<'a, R> Deref for Pref<'a, R>
where
	R: Deref + 'a,
{
	type Target = R::Target;

	fn deref(&self) -> &Self::Target {
		unsafe { (*self.managed_ref.reference).deref() }
	}
}

impl<'a, R> DerefMut for Pref<'a, R>
where
	R: Deref + DerefMut + 'a,
{
	fn deref_mut(&mut self) -> &mut Self::Target {
		unsafe { (*self.managed_ref.reference).deref_mut() }
	}
}

impl<'a, R> Pref<'a, R>
where
	R: Deref + Copy + 'a,
{
	pub(super) fn as_ref(p: &Pref<'a, R>) -> R {
		unsafe { *p.managed_ref.reference }
	}
}

impl<'a, R> Pref<'a, R>
where
	R: Deref + 'a,
{
	pub(crate) fn into_ref(p: Pref<'a, R>) -> R {
		unsafe { ManuallyDrop::<R>::into_inner(p.managed_ref.reference) }
	}

	pub(super) fn new<T>(obj: &Twzobj<T>, p: R) -> Pref<'a, R> {
		Pref {
			obj: obj.as_generic(),
			managed_ref: PrefReference {
				reference: ManuallyDrop::new(p),
			},
			_pd: PhantomData,
		}
	}

	pub(super) fn local(p: &Pref<'a, R>) -> u64 {
		unsafe { p.managed_ref.address & (MAX_SIZE - 1) }
	}
}

impl<'a, R> Debug for Pref<'a, R>
where
	R: Deref + Debug + 'a,
{
	fn fmt(&self, fmt: &mut Formatter<'_>) -> Result<(), Error> {
		todo!()
	}
}

impl<'a, R> Clone for Pref<'a, R>
where
	R: Deref + Copy + 'a,
{
	fn clone(&self) -> Self {
		Pref::new(&self.obj, Pref::as_ref(self))
	}
}
