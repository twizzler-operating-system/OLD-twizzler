use crate::bstream::BstreamHdr;
use crate::io::TwzIOHdr;
use twz::mutex::TwzMutex;
use twz::obj::ObjCreateSpec;
use twz::obj::Twzobj;
use twz::ptr::Pptr;
use twz::TwzErr;

const PTY_BUFFER_SZ: usize = 1024;

const NCCS: usize = 32;

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

#[repr(C)]
struct WinSize {
	ws_row: u16,
	ws_col: u16,
	ws_xpixel: u16,
	ws_ypixel: u16,
}

#[repr(C)]
struct PtyBuffer {
	bufpos: usize,
	buffer: [u8; PTY_BUFFER_SZ],
}

#[repr(C)]
struct PtyServerHdr {
	stoc: Pptr<BstreamHdr>,
	ctos: Pptr<BstreamHdr>,
	io: TwzIOHdr,
	termios: Termios,
	wsz: WinSize,
	buflock: TwzMutex<PtyBuffer>,
}

#[repr(C)]
struct PtyClientHdr {
	server: Pptr<PtyServerHdr>,
	io: TwzIOHdr,
}

pub fn create_pty_pair(
	client_spec: &ObjCreateSpec<()>,
	server_spec: &ObjCreateSpec<()>,
) -> Result<(Twzobj, Twzobj), TwzErr> {
	let server = Twzobj::create_spec(server_spec)?;
	panic!("")
}
