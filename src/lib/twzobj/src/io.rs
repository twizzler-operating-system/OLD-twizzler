#[repr(C)]
pub struct TwzIOHdr {
	read: extern "C" fn() -> (),
	write: extern "C" fn() -> (),
	ioctl: extern "C" fn() -> (),
	poll: extern "C" fn() -> (),
}
