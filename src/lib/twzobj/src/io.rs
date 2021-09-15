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

#[repr(C)]
#[derive(Default)]
pub struct TwzIOHdr {
	read: Pptr<extern "C" fn() -> ()>,
	write: Pptr<extern "C" fn() -> ()>,
	ioctl: Pptr<extern "C" fn() -> ()>,
	poll: Pptr<extern "C" fn() -> ()>,
}

pub trait TwzIO {
	fn poll(&self, events: PollStates) -> Option<Event>
	where
		Self: Sized,
	{
		None
	}
}

pub enum ReadOutput {
	Ready(Box<[u8]>),
	WouldBlock,
}

impl ReadOutput {
	pub fn is_ready(&self) -> bool {
		todo!()
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

pub fn read<T: TwzIO>(obj: &Twzobj<T>, non_block: bool) -> ReadResult {
	Ok(ReadOutput::WouldBlock)
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
