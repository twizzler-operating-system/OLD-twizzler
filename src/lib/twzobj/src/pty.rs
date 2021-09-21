use crate::bstream::BstreamHdr;
use crate::io::TwzIOHdr;
use crate::io::{PollStates, ReadFlags, ReadOutput, ReadResult, WriteFlags, WriteOutput, WriteResult};
use twz::event::Event;
use twz::mutex::TwzMutex;
use twz::obj::CreateSpec;
use twz::obj::Twzobj;
use twz::ptr::Pptr;
use twz::refs::Pref;
use twz::TwzErr;

const PTY_BUFFER_SZ: usize = 1024;

const NCCS: usize = 32;

const OPOST: u32 = 1;
const ONLCR: u32 = 4;
const OCRNL: u32 = 0o10;

const ICANON: u32 = 0o0000002;
const ECHO: u32 = 0o0000010;

const VERASE: u32 = 2;
const VEOF: u32 = 4;

#[derive(Clone, Copy)]
#[repr(C)]
struct Termios {
	c_iflag: u32,
	c_oflag: u32,
	c_cflag: u32,
	c_lflag: u32,
	c_line: u8,
	c_cc: [u8; NCCS],
	__c_ispeed: u32,
	__c_ospeed: u32,
}

impl Default for Termios {
	fn default() -> Self {
		Termios {
			c_iflag: 0,
			c_oflag: 0,
			c_cflag: 0,
			c_lflag: 0,
			c_line: 0,
			c_cc: [
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			],
			__c_ispeed: 0,
			__c_ospeed: 0,
		}
	}
}

#[derive(Clone, Copy)]
#[repr(C)]
struct WinSize {
	ws_row: u16,
	ws_col: u16,
	ws_xpixel: u16,
	ws_ypixel: u16,
}

impl Default for WinSize {
	fn default() -> Self {
		WinSize {
			ws_row: 25,
			ws_col: 80,
			ws_xpixel: 0,
			ws_ypixel: 0,
		}
	}
}

#[derive(Clone, Copy)]
#[repr(C)]
struct PtyBuffer {
	bufpos: usize,
	buffer: [u8; PTY_BUFFER_SZ],
}

impl Default for PtyBuffer {
	fn default() -> Self {
		PtyBuffer {
			bufpos: 0,
			buffer: [0; PTY_BUFFER_SZ],
		}
	}
}

#[derive(Default)]
#[repr(C)]
pub struct PtyServerHdr {
	stoc: Pptr<BstreamHdr>,
	ctos: Pptr<BstreamHdr>,
	io: TwzIOHdr,
	termios: Termios,
	wsz: WinSize,
	input_buflock: TwzMutex<PtyBuffer>,
	output_buflock: TwzMutex<PtyBuffer>,
}

#[derive(Default)]
#[repr(C)]
pub struct PtyClientHdr {
	server: Pptr<PtyServerHdr>,
	io: TwzIOHdr,
}

impl PtyBuffer {
	fn enqueue(&mut self, bytes: &[u8]) -> bool {
		if self.bufpos + bytes.len() >= PTY_BUFFER_SZ {
			return false;
		}
		for i in 0..bytes.len() {
			let b = bytes[i];
			self.buffer[self.bufpos] = b;
			self.bufpos += 1;
		}
		true
	}

	fn erase(&mut self) {
		if self.bufpos > 0 {
			self.bufpos -= 1;
		}
	}

	fn drain(&mut self, hdr: &PtyServerHdr, nonblock: bool, towards_client: bool) -> bool {
		let current_len = self.bufpos;
		let bs = if towards_client { hdr.stoc.lea() } else { hdr.ctos.lea() };
		while self.bufpos > 0 {
			let result = bs
				.write(
					&self.buffer[0..self.bufpos],
					if nonblock {
						WriteFlags::NONBLOCK
					} else {
						WriteFlags::none()
					},
				)
				.unwrap();
			if let WriteOutput::Done(thislen) = result {
				if thislen != self.bufpos {
					self.buffer.copy_within(thislen..self.bufpos, 0);
				}
				self.bufpos -= thislen;
			} else {
				return false;
			}
		}
		true
	}
}

impl PtyServerHdr {
	fn transform_write_char(&self, ch: u8) -> Vec<u8> {
		if ch == b'\n' && (self.termios.c_oflag & ONLCR) != 0 {
			vec![b'\r', b'\n']
		} else if ch == b'\r' && (self.termios.c_oflag & OCRNL) != 0 {
			vec![b'\n']
		} else {
			vec![ch]
		}
	}

	fn transform_input_char(&self, ch: u8) -> Vec<u8> {
		todo!()
	}

	fn should_take_action(&self, buf: &[u8]) -> u8 {
		for b in buf {
			if *b == self.termios.c_cc[VEOF as usize] {
				return 2;
			}
			if *b == b'\n' {
				return 1;
			}
		}
		0
	}
}

impl crate::io::TwzIO for PtyServerHdr {
	fn read(&self, buf: &mut [u8], flags: ReadFlags) -> ReadResult {
		let bs = self.ctos.lea();
		let result = bs.read(buf, flags)?;
		if result.is_blocked() {
			let mut output_buffer = self.output_buflock.lock();
			output_buffer.drain(self, flags.contains_any(ReadFlags::NONBLOCK), false);
			return bs.read(buf, flags);
		}
		Ok(result)
	}

	fn write(&self, buf: &[u8], flags: WriteFlags) -> WriteResult {
		if self.termios.c_lflag & ICANON != 0 {
			let mut input_buffer = self.input_buflock.lock();
			let echo_bs = self.ctos.lea();
			for i in 0..buf.len() {
				if buf[i] == self.termios.c_cc[VERASE as usize] {
					input_buffer.erase();
					echo_bs.write(&[buf[i], b' ', buf[i]], WriteFlags::NONBLOCK);
				}
				let tr = self.transform_input_char(buf[i]);
				/* we don't care if this doesn't work, since we discard anything after buffer is
				 * filled.*/
				input_buffer.enqueue(&tr);
				if self.termios.c_lflag & ECHO != 0 {
					/* TODO: can we do better than this best-effort? */
					echo_bs.write(&tr, WriteFlags::NONBLOCK);
				}

				let act = self.should_take_action(&tr);
				let mut eof = false;
				if act == 2 && input_buffer.bufpos > 0 {
					eof = true;
				}
				if act > 0 {
					if !input_buffer.drain(self, flags.contains_any(WriteFlags::NONBLOCK), true) {
						eprintln!("PTY buffer backup, needs fixing");
						/* TODO: urgent */
					}
				}
				if eof { /* TODO: EOF */ }
			}
			if !input_buffer.drain(&self, flags.contains_any(WriteFlags::NONBLOCK), true) {
				if flags.contains_any(WriteFlags::NONBLOCK) {
					return Ok(WriteOutput::WouldBlock);
				}
			}
			Ok(WriteOutput::Done(buf.len()))
		} else {
			/* TODO: sync between echoing and actual writing. Currently, we could have non-echo'd
			 * stuff get past when it should be echoed. */
			let len = if self.termios.c_lflag & ECHO != 0 {
				let echo_bs = self.ctos.lea();
				let result = echo_bs.write(buf, flags)?;
				if let WriteOutput::Done(len) = result {
					len
				} else {
					return Ok(result);
				}
			} else {
				buf.len()
			};
			if len == 0 && flags.contains_any(WriteFlags::NONBLOCK) {
				return Ok(WriteOutput::WouldBlock);
			}
			let bs = self.stoc.lea();
			bs.write(buf, flags)
		}
	}
}

impl crate::io::TwzIO for PtyClientHdr {
	fn poll(&self, events: PollStates) -> Option<Event> {
		let server = self.server.lea();
		if events.contains_any(PollStates::READ) {
			let bs = server.stoc.lea();
			bs.poll(events)
		} else if events.contains_any(PollStates::WRITE) {
			let bs = server.ctos.lea();
			bs.poll(events)
		} else {
			None
		}
	}

	fn read(&self, buf: &mut [u8], flags: ReadFlags) -> ReadResult {
		let server = self.server.lea();
		let bs = server.stoc.lea();
		let result = bs.read(buf, flags)?;
		if result.is_blocked() {
			let mut input_buffer = server.input_buflock.lock();
			input_buffer.drain(&server, flags.contains_any(ReadFlags::NONBLOCK), true);
			return bs.read(buf, flags);
		}
		Ok(result)
	}

	fn write(&self, buf: &[u8], flags: WriteFlags) -> WriteResult {
		let server = self.server.lea();
		let bs = server.ctos.lea();
		if (server.termios.c_oflag & OPOST) != 0 {
			let mut output_buffer = server.output_buflock.lock();
			if !output_buffer.drain(&server, flags.contains_any(WriteFlags::NONBLOCK), false)
				&& flags.contains_any(WriteFlags::NONBLOCK)
			{
				return Ok(WriteOutput::WouldBlock);
			}
			for i in 0..buf.len() {
				let tr = server.transform_write_char(buf[i]);
				let wrote_all = output_buffer.enqueue(&tr);
				if wrote_all {
					panic!("failed to enqueue to output buffer in pty");
				}
				if !output_buffer.drain(&server, flags.contains_any(WriteFlags::NONBLOCK), false) {
					if i == 0 && flags.contains_any(WriteFlags::NONBLOCK) {
						return Ok(WriteOutput::WouldBlock);
					}
					if i > 0 {
						return Ok(WriteOutput::Done(i + 1));
					}
					/* can't have i == 0 here because we would have blocked in drain() */
				}
			}
			Ok(WriteOutput::Done(buf.len()))
		} else {
			bs.write(buf, flags)
		}
	}
}

use crate::io::TwzIO;
pub fn create_pty_pair(
	_client_spec: &CreateSpec,
	_server_spec: &CreateSpec,
) -> Result<(Twzobj<PtyClientHdr>, Twzobj<PtyServerHdr>), TwzErr> {
	let server = Twzobj::<PtyServerHdr>::create_ctor(_server_spec, |obj, tx| {
		let mut base = obj.base_mut(tx);
		base.stoc.set(obj.new_item(tx), tx);
		base.ctos.set(obj.new_item(tx), tx);
		//base.ctos = base.stoc;
	})
	.unwrap();

	let base = server.base();
	let bs = base.stoc.lea();

	let data = b"Hello, Bstream!";
	let mut rcount = 0;
	let mut wcount = 0;
	while true {
		let result: WriteOutput = bs.write(data, crate::io::WriteFlags::none()).unwrap();
		//	println!("write: {:?}", result);

		if let WriteOutput::Done(n) = result {
			wcount += n;
		}

		let mut buffer = [0; 128];
		let result = bs.read(&mut buffer, crate::io::ReadFlags::none()).unwrap();

		if let ReadOutput::Done(n) = result {
			rcount += n;
		}
		if rcount != wcount {
			panic!("");
		}
		//	println!("read: {} {} {:?} : {:?}", wcount, rcount, result, buffer);
	}
	loop {}

	let client = Twzobj::<PtyClientHdr>::create_ctor(_client_spec, |obj, tx| {
		let mut base = obj.base_mut(tx);
		base.server.set(server.base(), tx);
	})
	.unwrap();
	Ok((client, server))
}
