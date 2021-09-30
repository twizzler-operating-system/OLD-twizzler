use twz::event::Event;
use twz::obj::Twzobj;
use twz::ptr::Pptr;
use twz::TwzErr;

twz::bitflags! {
	pub struct PollStates: u64 {
		const READ = 0x1;
		const WRITE = 0x2;
	}
}

twz::bitflags! {
	pub struct ReadFlags: u32 {
		const NONBLOCK = 0x1;
	}
}

twz::bitflags! {
	pub struct WriteFlags: u32 {
		const NONBLOCK = 0x1;
	}
}

pub const METAEXT_TAG: u64 = 0x0000000010101010;

#[repr(u32)]
pub enum TwzIOType {
	Unknown = 0,
	Bstream = 1,
	PtyClient = 2,
	PtyServer = 3,
}

impl Default for TwzIOType {
	fn default() -> Self {
		Self::Unknown
	}
}

#[repr(C)]
#[derive(Default)]
pub struct TwzIOHdr {
	resv: u32,
	pub io_type: TwzIOType,
	read: Pptr<extern "C" fn() -> ()>,
	write: Pptr<extern "C" fn() -> ()>,
	ioctl: Pptr<extern "C" fn() -> ()>,
	poll: Pptr<extern "C" fn() -> ()>,
}

pub trait TwzIO {
	fn poll(&self, events: PollStates) -> Option<Event> {
		None
	}

	fn read(&self, buf: &mut [u8], read_flags: ReadFlags) -> ReadResult {
		todo!()
	}

	fn write(&self, buf: &[u8], flags: WriteFlags) -> WriteResult {
		todo!()
	}
}

#[derive(Debug, Copy, Clone)]
pub enum ReadOutput {
	Done(usize),
	EOF,
	WouldBlock,
}

#[derive(Debug, Copy, Clone)]
pub enum WriteOutput {
	Done(usize),
	WouldBlock,
}

impl ReadOutput {
	pub fn is_ready(&self) -> bool {
		match self {
			ReadOutput::Done(_) => true,
			_ => false,
		}
	}

	pub fn is_eof(&self) -> bool {
		match self {
			ReadOutput::EOF => true,
			_ => false,
		}
	}

	pub fn is_blocked(&self) -> bool {
		match self {
			ReadOutput::WouldBlock => true,
			_ => false,
		}
	}
}

pub struct PollOutput {
	event: Event,
}

impl PollOutput {
	pub fn is_ready(&self, states: PollStates) -> bool {
		self.event.ready() & states.bits() != 0
	}

	pub fn event(&self) -> &Event {
		&self.event
	}
}

pub type ReadResult = Result<ReadOutput, TwzErr>;

pub type PollResult = Result<PollOutput, TwzErr>;

pub type WriteResult = Result<WriteOutput, TwzErr>;

pub fn read<T: TwzIO>(obj: &Twzobj<T>, buf: &mut [u8], read_flags: ReadFlags) -> ReadResult {
	let base = obj.base();
	base.read(buf, read_flags)
}

pub fn write<T: TwzIO>(obj: &Twzobj<T>, buf: &[u8], flags: WriteFlags) -> WriteResult {
	let base = obj.base();
	base.write(buf, flags)
}

pub fn poll<T: TwzIO>(obj: &Twzobj<T>, events: PollStates) -> PollResult {
	let base = obj.base();
	let event = base.poll(events);
	if let Some(event) = event {
		Ok(PollOutput { event })
	} else {
		Err(TwzErr::Invalid)
	}
}
