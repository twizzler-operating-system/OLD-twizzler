use crate::obj::{ObjID, ProtFlags, Twzobj};

#[repr(C)]
struct ViewHdr {}

pub(crate) struct View {
	obj: Twzobj<ViewHdr>,
}

const TWZSLOT_CVIEW: u64 = 0x1fff0;

#[allow(unreachable_code)]
impl View {
	pub(crate) fn current() -> View {
		panic!("");
		View {
			obj: Twzobj::init_slot(0 /*TODO: get ID */, TWZSLOT_CVIEW),
		}
	}

	pub(crate) fn reserve_slot(&self, id: ObjID, prot: ProtFlags) -> u64 {
		panic!("")
	}

	pub(crate) fn release_slot(&self, slot: u64) {}

	pub(crate) fn set_upcall_entry(&self, entry: extern "C" fn(), dbl_entry: extern "C" fn()) {}
}
