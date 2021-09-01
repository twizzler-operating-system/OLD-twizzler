use crate::bstream::BstreamHdr;
use crate::io::TwzIOHdr;
use twz::mutex::TwzMutex;
use twz::obj::CreateSpec;
use twz::obj::Twzobj;
use twz::ptr::Pptr;
use twz::TwzErr;

const PTY_BUFFER_SZ: usize = 1024;

const NCCS: usize = 32;

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

#[derive(Default, Clone)]
#[repr(C)]
pub struct PtyServerHdr {
	stoc: Pptr<BstreamHdr>,
	ctos: Pptr<BstreamHdr>,
	io: TwzIOHdr,
	termios: Termios,
	wsz: WinSize,
	buflock: TwzMutex<PtyBuffer>,
}

#[repr(C)]
pub struct PtyClientHdr {
	server: Pptr<PtyServerHdr>,
	io: TwzIOHdr,
}

use twz::obj::ProtFlags;
pub fn create_pty_pair(
	_client_spec: &CreateSpec,
	_server_spec: &CreateSpec,
) -> Result<(Twzobj<PtyClientHdr>, Twzobj<PtyServerHdr>), TwzErr> {
	let server = Twzobj::<PtyServerHdr>::create_ctor(_server_spec, &|obj, base, tx| {
		obj.new_item(&mut base.stoc, tx);
		obj.new_item(&mut base.ctos, tx);
	})
	.unwrap();

	let client = Twzobj::<PtyClientHdr>::create_ctor(_client_spec, &|obj, base, tx| {
		base.server = obj.make_ptr(server.base(None), ProtFlags::READ | ProtFlags::WRITE, tx);
	})
	.unwrap();
	Ok((client, server))
}
