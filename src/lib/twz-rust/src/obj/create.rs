use super::id::ObjID;
use super::obj::Twzobj;
use super::r#const::{ProtFlags, NULLPAGE_SIZE};
use super::tx::Transaction;
use crate::TwzErr;
crate::bitflags! {
	pub struct CreateFlags: u64 {
		const HASH_DATA = 0x1;
		const DFL_READ = 0x4;
		const DFL_WRITE = 0x8;
		const DFL_EXEC = 0x10;
		const DFL_USE = 0x20;
		const DFL_DEL = 0x40;
		const ZERO_NONCE = 0x1000;
	}
}

#[derive(Debug, Copy, Clone)]
pub enum BackingType {
	Normal = 0,
}

#[derive(Debug, Copy, Clone)]
pub enum LifetimeType {
	Volatile = 0,
	Persistent = 1,
}

#[derive(Debug, Copy, Clone)]
pub enum KuSpec {
	None,
	Obj(ObjID),
}

#[derive(Debug, Copy, Clone)]
pub enum TieSpec {
	View,
	Obj(ObjID),
}

#[derive(Debug, Copy, Clone)]
pub struct SrcSpec {
	pub(crate) srcid: ObjID,
	pub(crate) start: u64,
	pub(crate) length: u64,
}

pub struct CreateSpec {
	pub(crate) srcs: Vec<SrcSpec>,
	pub(crate) ku: KuSpec,
	pub(crate) lt: LifetimeType,
	pub(crate) bt: BackingType,
	pub(crate) ties: Vec<TieSpec>,
	pub(crate) flags: CreateFlags,
}

impl CreateSpec {
	pub fn new(lt: LifetimeType, bt: BackingType, flags: CreateFlags) -> CreateSpec {
		CreateSpec {
			srcs: vec![],
			ku: KuSpec::None,
			lt,
			bt,
			ties: vec![],
			flags,
		}
	}
	pub fn src(mut self, src: SrcSpec) -> CreateSpec {
		self.srcs.push(src);
		self
	}
	pub fn tie(mut self, tie: TieSpec) -> CreateSpec {
		self.ties.push(tie);
		self
	}
	pub fn ku(mut self, kuspec: KuSpec) -> CreateSpec {
		self.ku = kuspec;
		self
	}
}

impl<T: Default> Twzobj<T> {
	/* This is unsafe because it returns zero-initialized base memory, which may be invalid */
	unsafe fn internal_create(spec: &CreateSpec) -> Result<Twzobj<T>, TwzErr> {
		let (id, res) = crate::sys::create(spec);
		if res != 0 {
			Err(TwzErr::OSError(res as i32))
		} else {
			let obj = Twzobj::init_guid(id, ProtFlags::READ | ProtFlags::WRITE);
			obj.raw_init_alloc(NULLPAGE_SIZE as usize + std::mem::size_of::<T>());
			obj.init_tx();
			Ok(obj)
		}
	}

	pub fn create(spec: &CreateSpec) -> Result<Twzobj<T>, TwzErr> {
		Self::create_base(spec, T::default())
	}

	pub fn create_base(spec: &CreateSpec, base: T) -> Result<Twzobj<T>, TwzErr> {
		unsafe {
			let obj: Twzobj<T> = Twzobj::internal_create(spec)?;
			let ob = obj.base_unchecked_mut();
			/* TODO: add this write to a transaction */
			(ob as *mut T).write(base);
			Ok(obj)
		}
	}

	pub fn create_ctor<F>(spec: &CreateSpec, ctor: F) -> Result<Twzobj<T>, TwzErr>
	where
		F: FnOnce(&Self, &Transaction),
	{
		unsafe {
			let obj: Twzobj<T> = Twzobj::internal_create(spec)?;
			let ob = obj.base_unchecked_mut();
			let tx = Transaction::new(obj.as_generic());
			/* TODO: add this write to the transaction */
			(ob as *mut T).write(T::default());
			ctor(&obj, &tx);
			Ok(obj)
		}
	}
}
