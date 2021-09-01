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
		let ts = twz::sys::ThreadSyncArgs::new_wake(&self.point, num as u64);
		twz::sys::thread_sync(&mut [ts], None);
	}
}

pub struct Event<'a> {
	point: &'a AtomicU64,
	events: u64,
}

impl<'a> Event<'a> {
	pub fn new(hdr: &'a EventHdr, events: u64) -> Event<'a> {
		Event {
			point: &hdr.point,
			events: events,
		}
	}

	pub fn ready(&self) -> u64 {
		let e = self.point.load(std::sync::atomic::Ordering::SeqCst);
		e & self.events
	}

	pub fn clear(&self) -> u64 {
		self.point.fetch_and(!self.events, std::sync::atomic::Ordering::SeqCst) & self.events
	}
}

pub fn wait<'a>(events: &[Event<'a>], timeout: Option<std::time::Duration>) -> Result<Vec<u64>, twz::TwzErr> {
	let readies: Vec<u64> = events
		.iter()
		.filter_map(|e| if e.ready() != 0 { Some(e.ready()) } else { None })
		.collect();
	if readies.len() > 0 {
		return Ok(readies);
	}
	let mut syncs: Vec<twz::sys::ThreadSyncArgs> = events
		.iter()
		.map(|e| twz::sys::ThreadSyncArgs::new_sleep(e.point, e.events))
		.collect();
	let res = twz::sys::thread_sync(&mut syncs, timeout);
	if res != 0 {
		return Err(twz::TwzErr::OSError(res as i32));
	}
	wait(events, timeout)
}
