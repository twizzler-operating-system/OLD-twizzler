use crate::io::TwzIOHdr;
use crate::io::{PollStates, ReadFlags, ReadOutput, ReadResult, WriteFlags, WriteOutput, WriteResult};
use std::sync::atomic::{AtomicU32, Ordering};
use twz::event::{Event, EventHdr};
use twz::mutex::TwzMutex;

pub const BSTREAM_METAEXT_TAG: u64 = 0x00000000bbbbbbbb;

#[repr(C)]
#[derive(Default)]
pub struct BstreamHdr {
	ev: EventHdr,
	lock: TwzMutex<BstreamInternal>,
}

#[repr(C)]
struct BstreamInternal {
	flags: u32,
	head: AtomicU32,
	tail: AtomicU32,
	nbits: u32,
	io: TwzIOHdr,
	buffer: [u8; 8192],
}

impl crate::io::TwzIO for BstreamHdr {
	fn poll(&self, events: PollStates) -> Option<twz::event::Event> {
		Some(twz::event::Event::new(&self.ev, events.bits()))
	}

	fn read(&self, buf: &mut [u8], flags: ReadFlags) -> ReadResult {
		let mut internal = self.lock.lock();
		let mut count = 0;
		while count < buf.len() {
			if internal.head.load(Ordering::SeqCst) == internal.tail.load(Ordering::SeqCst) {
				/* empty */
				if count != 0 || flags.contains_any(ReadFlags::NONBLOCK) {
					break;
				}
				if self.ev.clear(PollStates::READ.bits()) != 0 {
					continue;
				}
				drop(internal);
				let event = Event::new(&self.ev, PollStates::READ.bits());
				twz::event::wait(&[&event], None).unwrap();
				internal = self.lock.lock();
				continue;
			}

			let tail = internal.tail.load(Ordering::SeqCst);
			buf[count] = internal.buffer[tail as usize];
			internal
				.tail
				.store((tail + 1) & ((1 << internal.nbits) - 1), Ordering::SeqCst);
			count += 1;
		}
		if internal.head.load(Ordering::SeqCst) == internal.tail.load(Ordering::SeqCst) {
			self.ev.clear(PollStates::READ.bits());
		}

		self.ev.wake(PollStates::WRITE.bits(), None);

		if count == 0 && (flags.contains_any(ReadFlags::NONBLOCK)) {
			Ok(ReadOutput::WouldBlock)
		} else {
			Ok(ReadOutput::Done(count))
		}
	}

	fn write(&self, buf: &[u8], flags: WriteFlags) -> WriteResult {
		let mut internal = self.lock.lock();

		let mut count = 0;
		while count < buf.len() {
			if internal.free_space() <= 1 {
				if count == 0 && !flags.contains_any(WriteFlags::NONBLOCK) {
					if self.ev.clear(PollStates::WRITE.bits()) != 0 {
						continue;
					}

					drop(internal);
					let event = Event::new(&self.ev, PollStates::WRITE.bits());
					twz::event::wait(&[&event], None).unwrap();
					internal = self.lock.lock();
					continue;
				}
				break;
			}

			let head = internal.head.load(Ordering::SeqCst);
			internal.buffer[head as usize] = buf[count];
			internal
				.head
				.store((head + 1) & ((1 << internal.nbits) - 1), Ordering::SeqCst);
			count += 1;
		}

		if internal.free_space() <= 1 {
			self.ev.clear(PollStates::WRITE.bits());
		}

		self.ev.wake(PollStates::READ.bits(), None);
		if count == 0 && (flags.contains_any(WriteFlags::NONBLOCK)) {
			Ok(WriteOutput::WouldBlock)
		} else {
			Ok(WriteOutput::Done(count))
		}
	}
}

impl Default for BstreamInternal {
	fn default() -> Self {
		Self {
			flags: 0,
			head: AtomicU32::new(0),
			tail: AtomicU32::new(0),
			nbits: 12,
			io: TwzIOHdr::default(),
			buffer: [0; 8192],
		}
	}
}

impl BstreamInternal {
	fn free_space(&self) -> usize {
		let head = self.head.load(Ordering::SeqCst);
		let tail = self.tail.load(Ordering::SeqCst);
		(if tail > head {
			tail - head
		} else {
			(1 << self.nbits) - head + tail
		}) as usize
	}
}

impl BstreamHdr {
	pub fn total_size(&self) -> usize {
		std::mem::size_of::<BstreamHdr>() + self.buffer_size()
	}

	pub fn buffer_size(&self) -> usize {
		1usize << self.lock.lock().nbits
	}
}
