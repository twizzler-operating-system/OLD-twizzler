use std::sync::atomic::AtomicU64;

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
		let ts = twz::sys::ThreadSyncArgs::new_wake(&self.point, num);
		twz::sys::thread_sync([ts], None);
	}
}

pub struct Event<'a> {
	point: &'a AtomicU64,
	events: u64,
}

impl<'a> Event<'a> {
	fn new(hdr: &'a EventHdr, events: u64) -> Event<'a> {
		Event {
			point: &hdr.point,
			events: events,
		}
	}
}

pub fn wait<'a>(events: &[Event<'a>], timeout: Option<std::time::Duration>) -> Result<Vec<u64>, twz::TwzErr> {
	let readies: Vec<u64> = events.filter_map(|e| if e.ready() { Some(e.ready()) } else { None });
	if readies.len() > 0 {
		return readies;
	}
	let syncs = events.map(|e| twz::sys::ThreadSyncArgs::new_sleep(e.point, e.events));
	let res = twz::sys::thread_sync(syncs, timeout);
	if res != 0 {
		return Err(twz::TwzErr::OSError(res));
	}
	wait(events, timeout)
}
