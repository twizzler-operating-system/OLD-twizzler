use twz::event::Event;
use twz::obj::Twzobj;
use twz::ptr::Pptr;
use twz::TwzErr;

twz::bitflags! {
	pub struct PollStates: u32 {
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

pub trait TwzIO {}

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
	states: PollStates,
}

impl PollOutput {
	pub fn is_ready(&self, states: PollStates) -> bool {
		self.states.contains_any(states)
	}

	pub fn events(&self) -> Vec<Event> {
		vec![]
	}
}

pub type ReadResult = Result<ReadOutput, TwzErr>;

pub type PollResult = Result<PollOutput, TwzErr>;

pub fn read<T: TwzIO>(obj: &Twzobj<T>) -> ReadResult {
	todo!()
}

pub fn poll<T: TwzIO>(obj: &Twzobj<T>) -> PollResult {
	Ok(PollOutput {
		states: PollStates::none(),
	})
}
