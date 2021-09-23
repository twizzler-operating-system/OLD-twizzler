use crate::obj::GTwzobj;
use crate::sys::{thread_sync, ThreadSyncArgs};
use std::sync::atomic::AtomicU64;

#[derive(Default)]
pub struct EventHdr {
	point: AtomicU64,
}

impl EventHdr {
	pub fn new() -> EventHdr {
		EventHdr {
			point: AtomicU64::new(0),
		}
	}

	pub fn wake(&self, events: u64, num: Option<usize>) {
		let old = self.point.fetch_or(events, std::sync::atomic::Ordering::SeqCst);
		if (old & events) != events {
			let ts = ThreadSyncArgs::new_wake(&self.point, num.unwrap_or(!0usize) as u64);
			thread_sync(&mut [ts], None);
		}
	}

	pub fn clear(&self, events: u64) -> u64 {
		self.point.fetch_and(!events, std::sync::atomic::Ordering::SeqCst) & events
	}
}

pub struct Event {
	pub(crate) point: *const AtomicU64,
	obj: GTwzobj,
	pub(crate) events: u64,
}

impl Event {
	pub fn new(hdr: &EventHdr, events: u64) -> Event {
		Event {
			point: &hdr.point as *const AtomicU64,
			obj: GTwzobj::from_ptr_locked(hdr),
			events: events,
		}
	}

	pub fn ready(&self) -> u64 {
		let e = unsafe { &*self.point }.load(std::sync::atomic::Ordering::SeqCst);
		e & self.events
	}

	pub fn clear(&self) -> u64 {
		unsafe { &*self.point }.fetch_and(!self.events, std::sync::atomic::Ordering::SeqCst) & self.events
	}

	pub fn events(&self) -> u64 {
		self.events
	}

	pub(crate) fn thread_sync(&self) -> Option<ThreadSyncArgs> {
		let val = unsafe { &*self.point }.load(std::sync::atomic::Ordering::SeqCst);
		if val & self.events != 0 {
			None
		} else {
			Some(ThreadSyncArgs::new_sleep(unsafe { self.point.as_ref() }.unwrap(), val))
		}
	}
}

pub fn wait(events: &[&Event], timeout: Option<std::time::Duration>) -> Result<Vec<(usize, u64)>, crate::TwzErr> {
	let mut syncs = vec![];
	let mut rets = vec![];
	for i in 0..events.len() {
		let event = events[i];
		let val = unsafe { &*event.point }.load(std::sync::atomic::Ordering::SeqCst);
		if val & event.events != 0 {
			rets.push((i, val & event.events));
		} else if rets.len() == 0 {
			syncs.push(ThreadSyncArgs::new_sleep(unsafe { event.point.as_ref() }.unwrap(), val));
		}
	}

	if rets.len() > 0 {
		return Ok(rets);
	}
	let res = thread_sync(&mut syncs, timeout);
	if res != 0 {
		return Err(crate::TwzErr::OSError(res as i32));
	}
	wait(events, timeout)
}
