use super::r#const::ProtFlags;
use super::r#const::{MAX_SIZE, NULLPAGE_SIZE};
use super::tx::Transaction;
use super::Twzobj;
use crate::ptr::Pptr;

pub struct Pref<'a, T, R> {
	obj: &'a Twzobj<T>,
	p: &'a R,
}

impl<'a, T, R> std::ops::Deref for Pref<'a, T, R> {
	type Target = R;

	fn deref(&self) -> &Self::Target {
		self.p
	}
}

impl<R> Pptr<R> {
	pub fn set<'a, T>(&self, p: Pref<'a, T, R>, tx: &crate::obj::tx::Transaction) {}

	pub fn lea<'a, T>(&self) -> Pref<'a, T, R> {
		panic!("")
	}
}

impl<T> Twzobj<T> {
	pub(crate) unsafe fn base_unchecked_mut(&self) -> &mut T {
		std::mem::transmute::<u64, &mut T>(self.slot * MAX_SIZE + NULLPAGE_SIZE)
	}

	pub fn new_item<'a, R: Default>(&self, tx: &Transaction) -> Pref<'a, T, R> {
		let p = tx.prep_alloc_free_on_fail(self);
		self.allocate_copy_item(p, R::default());
		p.lea()
	}

	fn fot_get_ptr<R>(&self, tgt: &R, flags: ProtFlags, tx: &Transaction) -> u64 {
		panic!("")
	}

	unsafe fn construct_pptr<R>(entry: u64, tgt: &R) -> u64 {
		entry * MAX_SIZE | (std::mem::transmute::<&R, u64>(tgt) & (MAX_SIZE - 1))
	}

	pub fn make_ptr_flags<R>(&self, tgt: &R, flags: ProtFlags, tx: &Transaction) -> Pptr<R> {
		let entry = self.fot_get_ptr(tgt, flags, tx);
		//TODO tx record ptr.p
		unsafe { Pptr::new(Self::construct_pptr(entry, tgt)) }
	}

	pub fn make_ptr<R>(&self, tgt: &R, tx: &Transaction) -> Pptr<R> {
		self.make_ptr_flags(tgt, ProtFlags::READ | ProtFlags::WRITE, tx)
	}

	pub fn base_ptr<'a>(&'a self) -> Pref<'a, T, T> {
		Pref {
			obj: self,
			p: self.base(None),
		}
	}

	pub fn base<'a>(&'a self, tx: Option<&Transaction>) -> &'a T {
		if let Some(_tx) = tx {
			panic!("")
		} else {
			/* TODO: check log */
			unsafe { self.base_unchecked_mut() }
		}
	}

	pub fn base_mut<'a>(&'a self, _tx: &Transaction) -> &'a mut T {
		panic!("")
	}

	pub(crate) unsafe fn offset_lea<R>(&self, offset: u64) -> &R {
		if offset < MAX_SIZE {
			return std::mem::transmute::<u64, &R>(self.slot * MAX_SIZE + offset);
		}
		panic!("tried to offset an object beyond its maximum size")
	}

	pub(crate) unsafe fn offset_lea_mut<R>(&self, offset: u64) -> &mut R {
		if offset < MAX_SIZE {
			return std::mem::transmute::<u64, &mut R>(self.slot * MAX_SIZE + offset);
		}
		panic!("tried to offset an object beyond its maximum size")
	}
}
