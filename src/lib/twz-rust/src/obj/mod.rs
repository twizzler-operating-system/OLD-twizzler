use crate::kso::view::{View, ViewFlags};
use crate::name::NameError;
use crate::ptr::Pptr;
use crate::TwzErr;

pub mod r#const;
pub mod create;
pub mod id;
pub mod obj;
pub mod tx;

pub use self::create::*;
pub use self::id::*;
pub use self::obj::*;
pub use self::r#const::*;
pub use self::tx::*;

impl<T> Twzobj<T> {
	pub(crate) fn set_id(&mut self, id: ObjID) {
		self.id = id;
	}

	pub(crate) fn init_slot(id: ObjID, prot: ProtFlags, slot: u64, allocated: bool) -> Twzobj<T> {
		let flags = if allocated { ALLOCATED } else { 0 };
		Twzobj {
			id,
			slot,
			flags,
			prot,
			_pd: std::marker::PhantomData,
		}
	}

	pub fn init_guid(id: ObjID, prot: ProtFlags) -> Twzobj<T> {
		let slot = crate::kso::view::View::current().reserve_slot(id, prot);
		Twzobj::init_slot(id, prot, slot, true)
	}

	pub fn init_name(_name: &str, _prot: ProtFlags) -> Result<Twzobj<T>, NameError> {
		panic!("")
	}

	pub(crate) unsafe fn base_unchecked_mut(&self) -> &mut T {
		std::mem::transmute::<u64, &mut T>(self.slot * MAX_SIZE + NULLPAGE_SIZE)
	}

	fn base_unchecked_mut_uninit(&self) -> &mut std::mem::MaybeUninit<T> {
		unsafe { std::mem::transmute::<u64, &mut std::mem::MaybeUninit<T>>(self.slot * MAX_SIZE + NULLPAGE_SIZE) }
	}

	/* This is unsafe because it returns zero-initialized base memory, which may be invalid */
	unsafe fn internal_create(spec: &CreateSpec) -> Result<Twzobj<T>, TwzErr> {
		let (id, res) = crate::sys::create(spec);
		println!("CREATED {:x} {}", id, res);
		if res != 0 {
			Err(TwzErr::OSError(res as i32))
		} else {
			Ok(Twzobj::init_guid(id, ProtFlags::READ | ProtFlags::WRITE))
		}
	}

	pub fn create_base(spec: &CreateSpec, base: T) -> Result<Twzobj<T>, TwzErr> {
		unsafe {
			let obj: Twzobj<T> = Twzobj::internal_create(spec)?;
			let ob = obj.base_unchecked_mut();
			(ob as *mut T).write(base);
			Ok(obj)
		}
	}

	pub fn create_ctor(
		spec: &CreateSpec,
		ctor: &(dyn Fn(&Twzobj<T>, &mut T, &Transaction)),
	) -> Result<Twzobj<T>, TwzErr> {
		unsafe {
			let obj: Twzobj<T> = Twzobj::internal_create(spec)?;
			let ob = obj.base_unchecked_mut();
			let tx = Transaction {};
			ctor(&obj, ob, &tx);
			Ok(obj)
		}
	}

	pub fn create_base_ctor(
		spec: &CreateSpec,
		base: T,
		ctor: &(dyn Fn(&Twzobj<T>, &mut T) + 'static),
	) -> Result<Twzobj<T>, TwzErr> {
		unsafe {
			let obj: Twzobj<T> = Twzobj::internal_create(spec)?;
			let ob = obj.base_unchecked_mut();
			(ob as *mut T).write(base);
			ctor(&obj, ob);
			Ok(obj)
		}
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

	pub fn transaction<O, E>(
		&self,
		_f: &(dyn Fn(Transaction) -> Result<O, E> + 'static),
	) -> Result<O, TransactionErr<E>> {
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

/*
use crate::libtwz::*;
use crate::ptr::*;
use crate::*;
pub const OBJ_MAX_SIZE: u64 = 1 << 30;
pub const OBJ_NULLPAGE_SIZE: u64 = 0x1000;
pub type ObjID = u128;

pub(crate) struct LibtwzData {
	pub(crate) data: *mut u8,
}

unsafe impl Send for LibtwzData {}

pub(crate) fn id_from_lohi(lo: u64, hi: u64) -> ObjID {
	lo as u128 | (hi as u128) << 64
}

pub fn objid_split(id: ObjID) -> (u64, u64) {
	((id >> 64) as u64, (id & 0xffffffffffffffff) as u64)
}

pub fn objid_join(hi: i64, lo: i64) -> ObjID {
	u128::from(lo as u64) | (u128::from(hi as u64)) << 64
}

impl LibtwzData {
	fn new() -> LibtwzData {
		LibtwzData {
			data: unsafe {
				std::alloc::alloc(
					std::alloc::Layout::from_size_align(libtwz_twzobj_data_size(), libtwz_twzobj_align_size())
						.expect("failed to calculate memory layout for libtwz::twzobj"),
				)
			},
		}
	}
}

impl std::ops::Drop for LibtwzData {
	fn drop(&mut self) {
		unsafe {
			std::alloc::dealloc(
				self.data,
				std::alloc::Layout::from_size_align(libtwz_twzobj_data_size(), libtwz_twzobj_align_size())
					.expect("failed to calculate memory layout for libtwz::twzobj"),
			);
		}
	}
}

#[derive(Clone)]
pub struct Twzobj {
	slot: u64,
	id: ObjID,
	pub(crate) libtwz_data: std::sync::Arc<std::sync::Mutex<Option<LibtwzData>>>,
}

impl std::ops::Drop for Twzobj {
	/* TODO: release the slot */
	fn drop(&mut self) {}
}

pub struct Tx {}

enum SpecBaseType<T> {
	None,
	Base(T),
	Ctor(fn(&Twzobj, &mut T)),
}

impl<T: Clone> Clone for SpecBaseType<T> {
	fn clone(&self) -> SpecBaseType<T> {
		match self {
			SpecBaseType::None => SpecBaseType::None,
			SpecBaseType::Base(t) => SpecBaseType::Base(t.clone()),
			SpecBaseType::Ctor(f) => SpecBaseType::Ctor(*f),
		}
	}
}

impl<T: Copy> Copy for SpecBaseType<T> {}

pub struct ObjCreateSpec<'a, T> {
	src: Option<&'a Twzobj>,
	ku: Option<&'a Twzobj>,
	flags: i32,
	base: SpecBaseType<T>,
}

impl<'a, T> ObjCreateSpec<'a, T> {
	pub fn clone_with_base<B>(&self, t: B) -> ObjCreateSpec<B> {
		ObjCreateSpec {
			src: self.src,
			ku: self.ku,
			flags: self.flags,
			base: SpecBaseType::Base(t),
		}
	}

	pub fn clone_with_ctor<B>(&self, t: fn(&Twzobj, &mut B)) -> ObjCreateSpec<B> {
		ObjCreateSpec {
			src: self.src,
			ku: self.ku,
			flags: self.flags,
			base: SpecBaseType::Ctor(t),
		}
	}
}

impl<T: Copy> Copy for ObjCreateSpec<'_, T> {}
impl<T: Clone> Clone for ObjCreateSpec<'_, T> {
	fn clone(&self) -> Self {
		ObjCreateSpec {
			src: self.src,
			ku: self.ku,
			flags: self.flags,
			base: self.base.clone(),
		}
	}
}

impl Twzobj {
	pub fn id(&self) -> ObjID {
		self.id
	}

	pub(crate) fn alloc_libtwz_data(&self) {
		let mut data = self.libtwz_data.lock().unwrap();
		if data.is_none() {
			*data = Some(LibtwzData::new());
			unsafe {
				libtwz::twz_c::twz_object_init_ptr(
					data.as_mut().unwrap().data as *mut std::ffi::c_void,
					std::mem::transmute::<&i8, *const i8>(self.base_unchecked::<i8>()),
				);
			}
		}
	}

	pub fn ptr_get_obj<T>(&self, p: &crate::ptr::Pptr<T>) -> Result<Twzobj, crate::TwzErr> {
		if p.is_internal() {
			Self::init_guid(self.id)
		} else {
			let r = libtwz::ptr_load_foreign(self, p.p);
			let obj = Twzobj {
				id: 0,
				slot: r / OBJ_MAX_SIZE,
				libtwz_data: std::sync::Arc::new(std::sync::Mutex::new(None)),
			};
			obj.alloc_libtwz_data();

			Ok(obj)
		}
	}

	pub fn init_guid(id: ObjID) -> Result<Twzobj, TwzErr> {
		let slot = unsafe { twz_c::twz_view_allocate_slot(std::ptr::null_mut(), id, VE_READ | VE_WRITE) };
		if slot < 0 {
			return Err(TwzErr::OutOfSlots);
		}
		Ok(Twzobj {
			slot: slot as u64,
			id: id,
			libtwz_data: std::sync::Arc::new(std::sync::Mutex::new(None)),
		})
	}

	pub fn init_name(name: &str) -> Result<Twzobj, TwzErr> {
		let mut id = 0;
		let s = std::ffi::CString::new(name).unwrap();
		let res = unsafe { twz_c::twz_name_resolve(std::ptr::null_mut(), s.as_ptr(), std::ptr::null(), 0, &mut id) };
		if res < 0 {
			return Err(TwzErr::NameResolve(-res));
		}
		Twzobj::init_guid(id)
	}

	pub const TWZ_OBJ_CREATE_HASHDATA: i32 = 0x1;
	pub const TWZ_OBJ_CREATE_DFL_READ: i32 = 0x4;
	pub const TWZ_OBJ_CREATE_DFL_WRITE: i32 = 0x8;
	pub const TWZ_OBJ_CREATE_DFL_EXEC: i32 = 0x10;
	pub const TWZ_OBJ_CREATE_DFL_USE: i32 = 0x20;
	pub const TWZ_OBJ_CREATE_DFL_DEL: i32 = 0x40;

	pub fn create<T>(
		flags: i32,
		base: Option<T>,
		kuid: Option<&Twzobj>,
		src: Option<&Twzobj>,
	) -> Result<Twzobj, TwzErr> {
		let res = libtwz::twz_object_create(flags, kuid.map_or(0, |o| o.id), src.map_or(0, |o| o.id));
		match res {
			Err(e) => return Err(TwzErr::OSError(-e)),
			Ok(id) => {
				let mut obj = Twzobj::init_guid(id)?;
				let mut alloc_offset = std::mem::size_of::<T>();
				alloc_offset = (alloc_offset + 0x1000) & !0x1000; //TODO calc this better
				let res = libtwz::twz_object_init_alloc(&mut obj, alloc_offset);
				if res < 0 {
					return Err(TwzErr::OSError(-res));
				}
				if let Some(base) = base {
					let obj_base = unsafe { obj.base_unchecked_mut::<T>() };
					*obj_base = base;
					persist::flush_lines(obj_base);
					persist::pfence();
				}
				Ok(obj)
			}
		}
	}

	pub fn create_spec_ctor<T>(spec: &ObjCreateSpec<T>) -> Result<Twzobj, TwzErr> {
		match &spec.base {
			SpecBaseType::Ctor(t) => {
				let obj = Twzobj::create::<()>(spec.flags, None, spec.ku, spec.src)?;
				let b = unsafe { obj.base_unchecked_mut::<T>() };
				t(&obj, b);
				Ok(obj)
			}
			SpecBaseType::None => Twzobj::create::<T>(spec.flags, None, spec.ku, spec.src),
			_ => Err(TwzErr::Invalid),
		}
	}

	pub fn create_spec<T: Copy>(spec: &ObjCreateSpec<T>) -> Result<Twzobj, TwzErr> {
		match &spec.base {
			SpecBaseType::Base(t) => Twzobj::create::<T>(spec.flags, Some(*t), spec.ku, spec.src),
			SpecBaseType::Ctor(t) => {
				let obj = Twzobj::create::<()>(spec.flags, None, spec.ku, spec.src)?;
				let b = unsafe { obj.base_unchecked_mut::<T>() };
				t(&obj, b);
				Ok(obj)
			}
			SpecBaseType::None => Twzobj::create::<T>(spec.flags, None, spec.ku, spec.src),
		}
	}

	pub fn move_item<T: Copy, E>(
		&self,
		item: T,
		p: &mut Pptr<T>,
		_tx: Option<&mut Tx>,
	) -> Result<(), crate::TxResultErr<E>> {
		extern "C" fn do_the_move<T: Copy>(tgt: &mut T, src: &T) {
			*tgt = *src;
		}
		let res = libtwz::twz_object_alloc_move(self, p, 0, do_the_move, item);
		if res < 0 {
			Err(crate::TxResultErr::OSError(-res))
		} else {
			Ok(())
		}
	}

	pub fn move_item_vol<T: Copy>(&self, item: T) -> Result<&mut T, crate::TwzErr> {
		extern "C" fn do_the_move<T: Copy>(tgt: &mut T, src: &T) {
			*tgt = *src;
		}
		let mut p = crate::ptr::Pptr::new_null();
		let res = libtwz::twz_object_alloc_move(self, &mut p, 0, do_the_move, item);
		if res < 0 {
			Err(crate::TwzErr::OSError(-res))
		} else {
			Ok(unsafe { self.offset_lea_mut(p.p) })
		}
	}

	pub(crate) fn move_slice<T: Copy>(&self, slice: &[T], p: &mut u64) -> Result<(), crate::TwzErr> {
		let res = libtwz::twz_object_alloc_slice_move(self, p, 0, slice);
		if res < 0 {
			Err(crate::TwzErr::OSError(-res))
		} else {
			Ok(())
		}
	}

	pub(crate) fn free_slice<T>(&self, slice: &mut crate::pslice::Pslice<T>) {
		libtwz::twz_object_free::<T>(self, &mut slice.p, 0);
	}

	pub(crate) fn free_item<T>(&self, item: &T) {
		let mut p = unsafe { std::mem::transmute::<&T, u64>(item) };
		libtwz::twz_object_free::<T>(self, &mut p, 0);
	}

	pub fn transaction<F, T, E>(&self, func: F) -> Result<T, crate::TxResultErr<E>>
	where
		F: FnOnce(&mut Tx) -> Result<T, crate::TxResultErr<E>>,
	{
		let mut tx = Tx {};
		func(&mut tx)
	}

	pub fn lea<T>(&self, p: &Pptr<T>) -> &T {
		if p.is_internal() {
			unsafe { self.offset_lea::<T>(p.p) }
		} else {
			let r = libtwz::ptr_load_foreign(self, p.p);
			unsafe { std::mem::transmute::<u64, &T>(r) }
		}
	}

	pub fn lea_mut<T>(&self, p: &Pptr<T>, _tx: &mut Tx) -> &mut T {
		if p.is_internal() {
			unsafe { self.offset_lea_mut::<T>(p.p) }
		} else {
			let r = libtwz::ptr_load_foreign(self, p.p);
			unsafe { std::mem::transmute::<u64, &mut T>(r) }
		}
	}

	pub unsafe fn base_unchecked_mut<T>(&self) -> &mut T {
		std::mem::transmute::<u64, &mut T>(self.slot * OBJ_MAX_SIZE + OBJ_NULLPAGE_SIZE)
	}

	pub unsafe fn base_unchecked<T>(&self) -> &T {
		std::mem::transmute::<u64, &T>(self.slot * OBJ_MAX_SIZE + OBJ_NULLPAGE_SIZE)
	}

	pub fn base<T>(&self) -> &T {
		unsafe { &*std::mem::transmute::<u64, *const T>(self.slot * OBJ_MAX_SIZE + OBJ_NULLPAGE_SIZE) }
	}

	pub fn base_mut<T>(&self, _tx: &mut Tx) -> &mut T {
		unsafe { std::mem::transmute::<u64, &mut T>(self.slot * OBJ_MAX_SIZE + OBJ_NULLPAGE_SIZE) }
	}

	pub unsafe fn offset_lea_mut<T>(&self, offset: u64) -> &mut T {
		if offset < OBJ_MAX_SIZE {
			return std::mem::transmute::<u64, &mut T>(self.slot * OBJ_MAX_SIZE + offset);
		}
		panic!("")
	}

	pub unsafe fn offset_lea<T>(&self, offset: u64) -> &T {
		if offset < OBJ_MAX_SIZE {
			return std::mem::transmute::<u64, &T>(self.slot * OBJ_MAX_SIZE + offset);
		}
		panic!("")
	}

	pub unsafe fn store_ptr_unchecked<T>(&self, pptr: &mut Pptr<T>, dest: &T, dest_obj: &Twzobj) -> Result<(), TwzErr> {
		let mut p = std::mem::transmute::<*const T, u64>(dest as *const T);
		p = p & (OBJ_MAX_SIZE - 1);
		let res = libtwz::ptr_store_guid(self, &mut pptr.p, p, dest_obj);
		if res < 0 {
			Err(TwzErr::OSError(-res))
		} else {
			Ok(())
		}
	}
}

*/
