use twz::ptr::Pptr;

#[repr(C)]
#[derive(Default)]
pub struct TwzIOHdr {
	read: Pptr<extern "C" fn() -> ()>,
	write: Pptr<extern "C" fn() -> ()>,
	ioctl: Pptr<extern "C" fn() -> ()>,
	poll: Pptr<extern "C" fn() -> ()>,
}
