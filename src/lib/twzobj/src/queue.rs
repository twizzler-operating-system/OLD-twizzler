use twz::obj::{CreateSpec, Twzobj};
use twz::TwzErr;

#[repr(C)]
#[derive(Default)]
pub struct QueueHdr {}

use std::marker::PhantomData;
pub struct Queue<S, C> {
	obj: Twzobj<QueueHdr>,
	_pd: PhantomData<(S, C)>,
}

#[derive(Debug)]
pub enum QueueError {
	WouldBlock,
	Unknown,
}

twz::bitflags! {
	pub struct QueueFlags : u32 {
		const NONBLOCKING = 1;
	}
}

#[repr(C)]
pub struct QueueEntry<T> {
	cmdid: u32,
	info: u32,
	data: T,
}

impl<T> QueueEntry<T> {
	pub fn new(info: u32, item: T) -> QueueEntry<T> {
		QueueEntry {
			cmdid: 0,
			info,
			data: item,
		}
	}

	pub fn info(&self) -> u32 {
		self.info
	}
	pub fn data(self) -> T {
		self.data
	}
}

use std::fmt;
impl<T: std::fmt::Debug> std::fmt::Debug for QueueEntry<T> {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
		f.debug_struct("QueueEntry")
			.field("info", &self.info)
			.field("data", &self.data)
			.finish()
	}
}

use std::ffi::c_void;
use std::mem::{size_of, transmute, MaybeUninit};
extern "C" {
	fn queue_hdr_submit(hdr: *mut c_void, item: *const c_void, flags: u32) -> i32;
	fn queue_hdr_receive(hdr: *mut c_void, item: *mut c_void, flags: u32) -> i32;
	fn queue_hdr_complete(hdr: *mut c_void, item: *const c_void, flags: u32) -> i32;
	fn queue_hdr_get_finished(hdr: *mut c_void, item: *mut c_void, flags: u32) -> i32;
	fn queue_hdr_init(hdr: *mut c_void, sqlen: usize, sqstride: usize, cqlen: usize, sqstride: usize) -> i32;
}

impl<S: Copy, C: Copy> Queue<S, C> {
	pub fn create(spec: &CreateSpec, sqlen: usize, cqlen: usize) -> Result<Queue<S, C>, TwzErr> {
		let queue = Twzobj::<QueueHdr>::create_ctor(spec, |obj, tx| {
			let mut base = obj.base_mut(tx);
			unsafe {
				queue_hdr_init(
					transmute::<&mut QueueHdr, *mut c_void>(&mut *base),
					sqlen,
					size_of::<QueueEntry<S>>(),
					cqlen,
					size_of::<QueueEntry<C>>(),
				);
			}
		})
		.unwrap();
		Ok(Queue {
			obj: queue,
			_pd: PhantomData,
		})
	}

	pub fn submit(&self, item: &QueueEntry<S>, flags: QueueFlags) -> Result<(), QueueError> {
		let ret = unsafe {
			queue_hdr_submit(
				transmute::<&mut QueueHdr, *mut c_void>(self.obj.base_unchecked_mut()),
				transmute::<&QueueEntry<S>, *const c_void>(item),
				flags.bits(),
			)
		};
		if ret == -11 {
			Err(QueueError::WouldBlock)
		} else if ret == 0 {
			Ok(())
		} else {
			Err(QueueError::Unknown)
		}
	}
	pub fn receive(&self, flags: QueueFlags) -> Result<QueueEntry<S>, QueueError> {
		let mut item = MaybeUninit::uninit();
		let ret = unsafe {
			queue_hdr_receive(
				transmute::<&mut QueueHdr, *mut c_void>(self.obj.base_unchecked_mut()),
				transmute::<*mut QueueEntry<S>, *mut c_void>(item.as_mut_ptr()),
				flags.bits(),
			)
		};
		if ret == -11 {
			Err(QueueError::WouldBlock)
		} else if ret == 0 {
			Ok(unsafe { item.assume_init() })
		} else {
			Err(QueueError::Unknown)
		}
	}
	pub fn complete(&self, item: &QueueEntry<C>, flags: QueueFlags) -> Result<(), QueueError> {
		let ret = unsafe {
			queue_hdr_complete(
				transmute::<&mut QueueHdr, *mut c_void>(self.obj.base_unchecked_mut()),
				transmute::<&QueueEntry<C>, *const c_void>(item),
				flags.bits(),
			)
		};
		if ret == -11 {
			Err(QueueError::WouldBlock)
		} else if ret == 0 {
			Ok(())
		} else {
			Err(QueueError::Unknown)
		}
	}

	pub fn get_completed(&self, flags: QueueFlags) -> Result<QueueEntry<C>, QueueError> {
		let mut item = MaybeUninit::uninit();
		let ret = unsafe {
			queue_hdr_get_finished(
				transmute::<&mut QueueHdr, *mut c_void>(self.obj.base_unchecked_mut()),
				transmute::<*mut QueueEntry<C>, *mut c_void>(item.as_mut_ptr()),
				flags.bits(),
			)
		};
		if ret == -11 {
			Err(QueueError::WouldBlock)
		} else if ret == 0 {
			Ok(unsafe { item.assume_init() })
		} else {
			Err(QueueError::Unknown)
		}
	}
}

enum Callback<'a, C> {
	Call(Box<Fn(u32, C) + 'a>),
	Ignore,
	Wait,
}

use std::collections::HashMap;
pub struct ManagedQueue<'a, S, C> {
	pub queue: Queue<S, C>,
	idcounter: u32,
	idvec: Vec<u32>,
	outstanding: HashMap<u32, Callback<'a, C>>,
}

impl<'a, S: Copy, C: Copy> ManagedQueue<'a, S, C> {
	pub fn new(q: Queue<S, C>) -> ManagedQueue<'a, S, C> {
		Self {
			queue: q,
			idcounter: 0,
			idvec: vec![],
			outstanding: HashMap::new(),
		}
	}
	fn submit(&mut self, item: S, handler: Callback<'a, C>, flags: QueueFlags) -> Result<u32, QueueError> {
		let id = self.idvec.pop().unwrap_or_else(|| {
			let id = self.idcounter;
			self.idcounter += 1;
			id
		});
		self.queue.submit(&QueueEntry::new(id, item), flags)?;
		self.outstanding.insert(id, handler);
		Ok(id)
	}

	pub fn submit_callback<F: Fn(u32, C) + 'a>(&mut self, item: S, cb: F, flags: QueueFlags) -> Result<u32, QueueError> {
		self.submit(item, Callback::Call(Box::new(cb)), flags)
	}

	pub fn check_finished(&mut self, block: bool) -> Result<Vec<C>, QueueError> {
		let mut retvec = vec![];
		loop {
			let flags = if block && retvec.len() == 0 {
				QueueFlags::none()
			} else {
				QueueFlags::NONBLOCKING
			};
			let ret = self.queue.get_completed(flags);
			match ret {
				Err(e) => {
					if retvec.len() > 0 {
						return Ok(retvec);
					}
					return Err(e);
				}
				Ok(c) => {
					if let Some(handler) = self.outstanding.remove(&c.info()) {
						let id = c.info();
						match handler {
							Callback::Call(f) => f(id, c.data()),
							Callback::Wait => retvec.push(c.data()),
							Callback::Ignore => {}
						}
						self.idvec.push(id);
					} else {
						retvec.push(c.data())
					}
				}
			}
		}
	}
}
