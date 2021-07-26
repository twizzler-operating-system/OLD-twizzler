use crate::libtwz;
use crate::obj::Twzobj;

type Result<T> = std::result::Result<T, crate::TwzErr>;

#[repr(C)]
#[derive(Copy, Clone, Debug)]
pub struct QueueEntry<T: Copy> {
	cmd_id: u32,
	info: u32,
	item: T,
}

impl<T: Copy> QueueEntry<T> {
	pub fn new(item: T, info: u32) -> QueueEntry<T> {
		QueueEntry {
			cmd_id: 0,
			info: info,
			item: item,
		}
	}

	pub(crate) fn new_uninit() -> QueueEntry<T> {
		let item: T = unsafe { std::mem::MaybeUninit::zeroed().assume_init() };
		QueueEntry {
			cmd_id: 0,
			info: 0,
			item: item,
		}
	}

	pub fn item(&self) -> &T {
		&self.item
	}

	pub fn item_mut(&mut self) -> &mut T {
		&mut self.item
	}

	pub fn consume(self) -> T {
		self.item
	}
}

pub struct Queue<S: Copy, C: Copy> {
	obj: crate::obj::Twzobj,
	_pd: std::marker::PhantomData<(S, C)>,
}

pub fn send<T: Copy>(obj: &crate::obj::Twzobj, item: T, flags: i32) -> crate::queue::Result<()> {
	let res = libtwz::queue_submit(obj, &item, flags);
	if res < 0 {
		Err(crate::TwzErr::OSError(res))
	} else {
		Ok(())
	}
}

pub fn recv<T: Copy>(obj: &crate::obj::Twzobj, flags: i32) -> crate::queue::Result<T> {
	let mut item: T = unsafe { std::mem::MaybeUninit::zeroed().assume_init() };
	let res = libtwz::queue_receive(obj, &mut item, flags);
	if res < 0 {
		Err(crate::TwzErr::OSError(res))
	} else {
		Ok(item)
	}
}

pub fn complete<T: Copy>(obj: &crate::obj::Twzobj, item: T, flags: i32) -> crate::queue::Result<()> {
	let res = libtwz::queue_complete(obj, &item, flags);
	if res < 0 {
		Err(crate::TwzErr::OSError(res))
	} else {
		Ok(())
	}
}

pub fn get_completion<T: Copy>(obj: &crate::obj::Twzobj, flags: i32) -> crate::queue::Result<T> {
	let mut item: T = unsafe { std::mem::MaybeUninit::zeroed().assume_init() };
	let res = libtwz::queue_get_completion(obj, &mut item, flags);
	if res < 0 {
		Err(crate::TwzErr::OSError(res))
	} else {
		Ok(item)
	}
}

pub fn create<S: Copy, C: Copy>(
	flags: i32,
	sqlen: usize,
	cqlen: usize,
	kuid: Option<crate::obj::Twzobj>,
) -> crate::queue::Result<crate::obj::Twzobj> {
	let id = libtwz::twz_object_create(flags, kuid.map_or(0, |o| o.id()), 0).map_err(|e| crate::TwzErr::OSError(e))?;
	let obj = crate::obj::Twzobj::init_guid(id)?;
	let res = libtwz::queue_init_hdr::<S, C>(&obj, sqlen, cqlen);
	if res < 0 {
		Err(crate::TwzErr::OSError(res))
	} else {
		Ok(obj)
	}
}

impl<S: Copy, C: Copy> Queue<S, C> {
	pub fn new_private(sqlen: usize, cqlen: usize) -> crate::queue::Result<Queue<S, C>> {
		Ok(Queue {
			obj: create::<QueueEntry<S>, QueueEntry<C>>(
				crate::obj::Twzobj::TWZ_OBJ_CREATE_DFL_READ | crate::obj::Twzobj::TWZ_OBJ_CREATE_DFL_WRITE,
				sqlen,
				cqlen,
				None,
			)?,
			_pd: std::marker::PhantomData,
		})
	}

	pub fn from_obj(obj: crate::obj::Twzobj) -> Queue<S, C> {
		Queue {
			obj: obj,
			_pd: std::marker::PhantomData,
		}
	}

	pub fn obj(&self) -> &crate::obj::Twzobj {
		&self.obj
	}

	pub fn send(&self, item: QueueEntry<S>, flags: i32) -> crate::queue::Result<()> {
		send::<QueueEntry<S>>(&self.obj, item, flags)
	}

	pub fn recv(&self, flags: i32) -> crate::queue::Result<QueueEntry<C>> {
		recv::<QueueEntry<C>>(&self.obj, flags)
	}

	pub fn complete(&self, item: QueueEntry<S>, flags: i32) -> crate::queue::Result<()> {
		complete::<QueueEntry<S>>(&self.obj, item, flags)
	}

	pub fn get_completion(&self, flags: i32) -> crate::queue::Result<QueueEntry<C>> {
		get_completion::<QueueEntry<C>>(&self.obj, flags)
	}
}

struct QueueCompleter<T: Copy, F> {
	callback: Box<fn(T, F)>,
	args: F,
}

pub struct QueueHandler<S: Copy, C: Copy, T> {
	outstanding: std::collections::HashMap<u32, QueueCompleter<C, T>>,
	ids: std::vec::Vec<u32>,
	idcounter: u32,
	queue: std::rc::Rc<Queue<S, C>>,
}

impl<S: Copy, C: Copy, F> QueueHandler<S, C, F> {
	pub fn new(queue: Queue<S, C>) -> QueueHandler<S, C, F> {
		QueueHandler::<S, C, F> {
			outstanding: std::collections::HashMap::new(),
			ids: vec![],
			idcounter: 0,
			queue: std::rc::Rc::new(queue),
		}
	}

	pub fn queue(&self) -> std::rc::Rc<Queue<S, C>> {
		self.queue.clone()
	}

	pub fn send_callback(
		&mut self,
		mut item: QueueEntry<S>,
		flags: i32,
		callback: fn(C, F),
		args: F,
	) -> crate::queue::Result<()> {
		let id = {
			let id = if let Some(id) = self.ids.pop() {
				id
			} else {
				let id = self.idcounter;
				self.idcounter += 1;
				id
			};
			self.outstanding.insert(
				id,
				QueueCompleter {
					callback: Box::new(callback),
					args: args,
				},
			);
			id
		};
		item.info = id;
		let res = self.queue.send(item, flags);
		if res.is_err() {
			self.outstanding.remove(&id);
			self.ids.push(id);
		}
		res
	}

	pub fn check_completions_callback(&mut self, flags: i32) -> crate::queue::Result<Option<QueueEntry<C>>> {
		let res = self.queue.get_completion(flags)?;
		let completer = {
			let completer = self.outstanding.remove(&res.info);
			if completer.is_some() {
				self.ids.push(res.info);
			}
			completer
		};
		if let Some(completer) = completer {
			let cb = completer.callback;
			(cb)(res.item, completer.args);
			Ok(None)
		} else {
			Ok(Some(res))
		}
	}
}

use crate::TwzErr;

#[derive(Debug)]
pub enum MultiResultState<S, C> {
	ReadyS(S),
	ReadyC(C),
	SleepReady,
	Error(i32),
}

#[derive(Debug)]
pub struct MultiResult<S, C> {
	pub queue_results: std::vec::Vec<(usize, MultiResultState<S, C>)>,
	pub sleep_results: std::vec::Vec<(usize, MultiResultState<S, C>)>,
}

#[derive(Copy, Clone, PartialEq)]
pub enum Direction {
	Submission = 0,
	Completion = 1,
}

use crate::libtwz::QueueMultiSpec;

pub fn wait_multiple<S: Copy, C: Copy>(
	queues: std::vec::Vec<(Direction, &Queue<S, C>)>,
	sleeps: std::vec::Vec<(&std::sync::atomic::AtomicU64, u64)>,
) -> std::result::Result<MultiResult<QueueEntry<S>, QueueEntry<C>>, TwzErr> {
	let mut results: std::vec::Vec<(Option<QueueEntry<S>>, Option<QueueEntry<C>>)> = vec![(None, None); queues.len()];
	let mut queue_specs: std::vec::Vec<QueueMultiSpec> = queues
		.iter()
		.enumerate()
		.map(|(i, (d, q))| {
			if *d == Direction::Submission {
				results[i] = (Some(QueueEntry::<S>::new_uninit()), None);
				QueueMultiSpec::new_subm(q, &mut results[i].0.as_mut().unwrap())
			} else {
				results[i] = (None, Some(QueueEntry::<C>::new_uninit()));
				QueueMultiSpec::new_cmpl(q, &mut results[i].1.as_mut().unwrap())
			}
		})
		.collect();
	for sleep in &sleeps {
		queue_specs.push(QueueMultiSpec::new_sleep(sleep.0, sleep.1));
	}
	if queue_specs.len() == 0 {
		return Ok(MultiResult {
			queue_results: vec![],
			sleep_results: vec![],
		});
	}
	let res = crate::libtwz::queue_multi_wait(queue_specs.len(), &mut queue_specs);
	if res < 0 {
		return Err(TwzErr::OSError(-res as i32));
	}

	let mut result = MultiResult {
		queue_results: vec![],
		sleep_results: vec![],
	};

	for (i, spec) in queue_specs.iter().enumerate() {
		if spec.ret > 0 {
			if spec.is_queue() {
				if spec.sq == 0 {
					result
						.queue_results
						.push((i, MultiResultState::ReadyS(results[i].0.unwrap())));
				} else {
					result
						.queue_results
						.push((i, MultiResultState::ReadyC(results[i].1.unwrap())));
				}
			} else {
				result
					.sleep_results
					.push((i - queues.len(), MultiResultState::SleepReady));
			}
		} else if spec.ret < 0 {
			result.queue_results.push((i, MultiResultState::Error(-spec.ret)));
		}
	}
	Ok(result)
}

pub trait QueueItemBuf<T: Copy> {
	fn get_buf_ptr(&mut self) -> &mut crate::ptr::Pptr<T>;
}

pub fn buffer_item<T: QueueItemBuf<B> + Copy, B: Copy, S: Copy, C: Copy>(
	queue: &Queue<S, C>,
	buffer: &Twzobj,
	item: T,
	bufitem: B,
) -> std::result::Result<QueueEntry<T>, TwzErr> {
	let bufitem_ptr = buffer.move_item_vol(bufitem)?;
	let mut qe = QueueEntry::new(item, 0);
	unsafe {
		queue
			.obj()
			.store_ptr_unchecked(qe.item_mut().get_buf_ptr(), bufitem_ptr, &buffer)?;
	}
	Ok(qe)
}

pub fn buffer_slice<T: QueueItemBuf<crate::pslice::Pslice<B>> + Copy, B: Copy, S: Copy, C: Copy>(
	queue: &Queue<S, C>,
	buffer: &Twzobj,
	item: T,
	bufitem: &[B],
) -> std::result::Result<QueueEntry<T>, TwzErr> {
	let slice = crate::pslice::Pslice::from_slice(&buffer, bufitem)?;
	let bufitem_ptr = buffer.move_item_vol(slice)?;
	let mut qe = QueueEntry::new(item, 0);
	unsafe {
		queue
			.obj()
			.store_ptr_unchecked(qe.item_mut().get_buf_ptr(), bufitem_ptr, &buffer)?;
	}
	Ok(qe)
}

pub fn free_buffer_slice<B: Copy, S: Copy, C: Copy>(
	_queue: &Queue<S, C>,
	buffer: &Twzobj,
	slice: &crate::pslice::Pslice<B>,
) {
	slice.free_slice(buffer);
	buffer.free_item(slice);
}
