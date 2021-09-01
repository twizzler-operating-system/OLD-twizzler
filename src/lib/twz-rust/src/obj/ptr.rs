use super::r#const::ProtFlags;
use super::r#const::{MAX_SIZE, NULLPAGE_SIZE};
use super::tx::Transaction;
use super::Twzobj;
use crate::ptr::Pptr;
impl<T> Twzobj<T> {
	pub(crate) unsafe fn base_unchecked_mut(&self) -> &mut T {
		std::mem::transmute::<u64, &mut T>(self.slot * MAX_SIZE + NULLPAGE_SIZE)
	}

	fn base_unchecked_mut_uninit(&self) -> &mut std::mem::MaybeUninit<T> {
		unsafe { std::mem::transmute::<u64, &mut std::mem::MaybeUninit<T>>(self.slot * MAX_SIZE + NULLPAGE_SIZE) }
	}
	pub fn copy_item<R: Copy>(&self, ptr: &mut Pptr<R>, item: R) {
		self.allocate_copy_item(ptr, item);
	}

	pub fn new_item<R: Default>(&self, ptr: &mut Pptr<R>, tx: &Transaction) {
		self.allocate_copy_item(ptr, R::default());
	}

	fn fot_get_ptr<R>(&self, tgt: &R, flags: ProtFlags, tx: &Transaction) -> u64 {
		panic!("")
	}

	unsafe fn construct_pptr<R>(entry: u64, tgt: &R) -> u64 {
		entry * MAX_SIZE | (std::mem::transmute::<&R, u64>(tgt) & (MAX_SIZE - 1))
	}

	pub fn store_ptr<R>(&self, ptr: &mut Pptr<R>, tgt: &R, flags: ProtFlags, tx: &Transaction) {
		let entry = self.fot_get_ptr(tgt, flags, tx);
		//TODO tx record ptr.p
		ptr.p = unsafe { Self::construct_pptr(entry, tgt) };
	}

	pub fn make_ptr<R>(&self, tgt: &R, flags: ProtFlags, tx: &Transaction) -> Pptr<R> {
		let entry = self.fot_get_ptr(tgt, flags, tx);
		//TODO tx record ptr.p
		unsafe { Pptr::new(Self::construct_pptr(entry, tgt)) }
	}

	pub fn base<'a>(&'a self, tx: Option<&Transaction>) -> &'a T {
		if let Some(_tx) = tx {
			panic!("")
		} else {
			/* TODO: check log */
			unsafe { self.base_unchecked_mut() }
		}
	}

	pub fn base_mut<'a>(&'a self, _tx: &Transaction) -> &'a T {
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

	pub fn lea<R>(&self, _ptr: &Pptr<R>) -> &R {
		panic!("")
	}

	pub fn lea_mut<R>(&self, _ptr: &Pptr<R>, _tx: &Transaction) -> &mut R {
		panic!("")
	}
}
