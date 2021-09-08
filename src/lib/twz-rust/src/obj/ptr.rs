use super::r#const::ProtFlags;
use super::r#const::{MAX_SIZE, NULLPAGE_SIZE};
use super::tx::Transaction;
use super::{GTwzobj, Twzobj};
use crate::ptr::Pptr;
use crate::refs::{Pref, PrefMut};

fn same_slot<A, B>(a: &A, b: &B) -> bool {
	let a = unsafe { std::mem::transmute::<&A, u64>(a) };
	let b = unsafe { std::mem::transmute::<&B, u64>(b) };
	let a = a & !(MAX_SIZE - 1);
	let b = b & !(MAX_SIZE - 1);
	a == b
}

impl<R> Pptr<R> {
	pub fn set<'a>(&mut self, p: Pref<'a, R>, tx: &Transaction) {
		if same_slot(self, p.p) {
			self.p = unsafe { std::mem::transmute::<&R, u64>(p.p) & (MAX_SIZE - 1) };
		} else {
			let obj = GTwzobj::from_ptr(self);
			let fote = obj.add_fote(&p, tx);
			self.p = p.local() | fote * MAX_SIZE;
		}
	}

	pub fn lea<'a>(&'a self) -> Pref<'a, R> {
		let obj = GTwzobj::from_ptr(self);
		if self.is_internal() {
			let off = unsafe { obj.offset_lea(self.p) };
			return Pref { obj, p: off };
		}
		self.lea_obj(obj)
	}

	pub fn lea_obj<'a>(&self, obj: GTwzobj) -> Pref<'a, R> {
		if self.is_internal() {
			Pref {
				obj: obj.as_generic(),
				p: unsafe { obj.offset_lea(self.offset()) },
			}
		} else {
			obj.resolve_external_ref(self)
		}
	}
}

impl<T> Twzobj<T> {
	pub(crate) unsafe fn base_unchecked_mut(&self) -> &mut T {
		std::mem::transmute::<u64, &mut T>(self.internal.slot * MAX_SIZE + NULLPAGE_SIZE)
	}

	pub fn new_item<'a, R: Default>(&self, tx: &'a Transaction) -> Pref<'a, R> {
		let p = tx.prep_alloc_free_on_fail(self);
		self.allocate_copy_item(p, R::default());
		Pref::new(self, unsafe { self.offset_lea(*p) })
	}

	unsafe fn construct_pptr<R>(entry: u64, tgt: &R) -> u64 {
		entry * MAX_SIZE | (std::mem::transmute::<&R, u64>(tgt) & (MAX_SIZE - 1))
	}

	pub fn base<'a>(&'a self) -> Pref<'a, T> {
		/* TODO: check log */
		Pref::new(self, unsafe { self.base_unchecked_mut() })
	}

	pub fn base_mut<'a>(&'a self, tx: &Transaction) -> PrefMut<'a, T> {
		let base = unsafe { self.base_unchecked_mut() };
		tx.record_base(self);
		PrefMut::new(self, base)
	}

	pub(crate) unsafe fn offset_lea<'a, 'b, R>(&'a self, offset: u64) -> &'b R {
		if offset < MAX_SIZE {
			return std::mem::transmute::<u64, &R>(self.internal.slot * MAX_SIZE + offset);
		}
		panic!("tried to offset an object beyond its maximum size")
	}

	pub(crate) unsafe fn offset_lea_mut<R>(&self, offset: u64) -> &mut R {
		if offset < MAX_SIZE {
			return std::mem::transmute::<u64, &mut R>(self.internal.slot * MAX_SIZE + offset);
		}
		panic!("tried to offset an object beyond its maximum size")
	}
}
