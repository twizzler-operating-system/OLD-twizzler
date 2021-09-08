use crate::obj::Twzobj;
use crate::ptr::Pptr;
use std::ffi::c_void;

/* TODO: handle allocation failures */

extern "C" {
	fn __runtime_twz_object_init_alloc(base: *const c_void, off: u64) -> i32;
	fn __runtime_twz_object_realloc(
		base: *const c_void,
		p: *const c_void,
		owner: *mut *const c_void,
		len: usize,
		flags: u64,
	) -> i32;
	fn __runtime_twz_object_free(base: *const c_void, p: *const c_void, owner: *mut *const c_void, flags: u64) -> i32;
	fn __runtime_twz_object_alloc(
		base: *const c_void,
		len: usize,
		owner: *mut *const c_void,
		flags: u64,
		ctor: extern "C" fn(*mut c_void, *const c_void),
		data: *const c_void,
	) -> i32;
}

impl<T> Twzobj<T> {
	unsafe fn raw_base_void(&self) -> *const c_void {
		std::mem::transmute::<&T, *const c_void>(&*self.base())
	}

	pub(crate) fn raw_init_alloc(&self, offset: usize) {
		unsafe {
			__runtime_twz_object_init_alloc(self.raw_base_void(), offset as u64);
		}
	}

	pub(crate) fn allocate_copy_item<R>(&self, owner: &mut u64, item: R) {
		extern "C" fn do_the_move<R>(tgt: &mut R, src: &R) {
			unsafe {
				std::ptr::copy_nonoverlapping(src as *const R, tgt as *mut R, 1);
			}
		}
		unsafe {
			__runtime_twz_object_alloc(
				self.raw_base_void(),
				std::mem::size_of::<R>(),
				std::mem::transmute::<&mut u64, *mut *const c_void>(owner),
				(std::mem::align_of::<R>() as u64) << 32,
				std::mem::transmute::<extern "C" fn(&mut R, &R), extern "C" fn(*mut c_void, *const c_void)>(
					do_the_move,
				),
				std::mem::transmute::<&R, *const c_void>(&item),
			);
		}
	}

	pub(crate) fn allocate_ctor_item<R, X>(
		&self,
		owner: &mut Pptr<R>,
		ctor: &(dyn Fn(&mut std::mem::MaybeUninit<R>, Option<&X>) + 'static),
		data: Option<&X>,
	) {
		extern "C" fn trampoline<R, X>(
			tgt: &mut std::mem::MaybeUninit<R>,
			src: &(
				&(dyn Fn(&mut std::mem::MaybeUninit<R>, Option<&X>) + 'static),
				Option<&X>,
			),
		) {
			let (ctor, src) = src;
			ctor(tgt, *src);
		}

		unsafe {
			__runtime_twz_object_alloc(
				self.raw_base_void(),
				std::mem::size_of::<R>(),
				std::mem::transmute::<&mut u64, *mut *const c_void>(&mut owner.p),
				(std::mem::align_of::<R>() as u64) << 32,
				std::mem::transmute::<
					extern "C" fn(
						&mut std::mem::MaybeUninit<R>,
						&(
							&(dyn Fn(&mut std::mem::MaybeUninit<R>, Option<&X>) + 'static),
							Option<&X>,
						),
					),
					extern "C" fn(*mut c_void, *const c_void),
				>(trampoline),
				std::mem::transmute::<
					&(
						&(dyn Fn(&mut std::mem::MaybeUninit<R>, Option<&X>) + 'static),
						Option<&X>,
					),
					*const c_void,
				>(&(ctor, data)),
			);
		}
	}
}
