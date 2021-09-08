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
			println!("cross-obj pointer set {:p} {} {:x}", p.p, fote, p.local());
			self.p = p.local() | fote * MAX_SIZE;
		}

		/*
		let obj = Twzobj::<T>::from_ptr(self);
		let ref_obj = GTwzobj::from_ptr(p.p);
		if obj.is_same_obj(&ref_obj) {
			self.p = unsafe { std::mem::transmute::<&R, u64>(p.p) & (MAX_SIZE - 1) };
		} else {
			todo!()
		}
		*/
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

	pub fn base_ptr<'a>(&'a self) -> Pref<'a, T> {
		Pref::new(self, self.base(None))
	}

	pub fn base<'a>(&'a self, tx: Option<&Transaction>) -> &'a T {
		if let Some(_tx) = tx {
			panic!("")
		} else {
			/* TODO: check log */
			unsafe { self.base_unchecked_mut() }
		}
	}

	pub fn base_mut<'a>(&'a self, tx: &Transaction) -> &'a mut T {
		let base = unsafe { self.base_unchecked_mut() };
		tx.record_base(self);
		base
	}

	pub fn base_mut_pin<'a>(&'a self, tx: &Transaction) -> PrefMut<'a, T> {
		let base = unsafe { self.base_unchecked_mut() };
		tx.record_base(self);
		PrefMut::new(self, unsafe { self.base_unchecked_mut() })
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
