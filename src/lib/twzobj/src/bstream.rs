use crate::event::EventHdr;
use crate::io::TwzIOHdr;
use std::sync::atomic::AtomicU32;
use twz::mutex::TwzMutex;

const BSTREAM_METAEXT_TAG: u64 = 0x00000000bbbbbbbb;

#[repr(C)]
pub struct BstreamHdr {
	lock: TwzMutex<BstreamInternal>,
}

#[repr(C)]
struct BstreamInternal {
	flags: u32,
	head: AtomicU32,
	tail: AtomicU32,
	nbits: u32,
	ev: EventHdr,
	io: TwzIOHdr,
}

impl BstreamHdr {
	pub fn total_size(&self) -> usize {
		std::mem::size_of::<BstreamHdr>() + self.buffer_size()
	}

	pub fn buffer_size(&self) -> usize {
		1usize << self.lock.lock().nbits
	}
}
