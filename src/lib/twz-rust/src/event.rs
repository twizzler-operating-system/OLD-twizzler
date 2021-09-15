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

	pub fn signal(&self, events: u64, num: usize) {
		self.point.fetch_or(events, std::sync::atomic::Ordering::SeqCst);
		let ts = ThreadSyncArgs::new_wake(&self.point, num as u64);
		thread_sync(&mut [ts], None);
	}
}

pub struct Event {
	point: *const AtomicU64,
	obj: GTwzobj,
	events: u64,
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
}

pub fn wait(events: &[Event], timeout: Option<std::time::Duration>) -> Result<Vec<u64>, crate::TwzErr> {
	let readies: Vec<u64> = events
		.iter()
		.filter_map(|e| if e.ready() != 0 { Some(e.ready()) } else { None })
		.collect();
	if readies.len() > 0 {
		return Ok(readies);
	}
	let mut syncs: Vec<ThreadSyncArgs> = events
		.iter()
		.map(|e| ThreadSyncArgs::new_sleep(unsafe { e.point.as_ref() }.unwrap(), e.events))
		.collect();
	let res = thread_sync(&mut syncs, timeout);
	if res != 0 {
		return Err(crate::TwzErr::OSError(res as i32));
	}
	wait(events, timeout)
}
