use super::id::ObjID;
use super::meta::FOTEntry;
use crate::kso::view::ViewFlags;

pub const MAX_SIZE: u64 = 1 << 30;
pub const NULLPAGE_SIZE: u64 = 0x1000;

pub(super) const MAX_FOTE: u64 = ((1u64 << 20) - NULLPAGE_SIZE) / std::mem::size_of::<FOTEntry>() as u64;
pub(super) const TOP_DATA: u64 = MAX_SIZE - (NULLPAGE_SIZE + MAX_FOTE * std::mem::size_of::<FOTEntry>() as u64);

pub fn objid_split(id: ObjID) -> (u64, u64) {
	((id >> 64) as u64, (id & 0xffffffffffffffff) as u64)
}

pub fn objid_join(hi: i64, lo: i64) -> ObjID {
	u128::from(lo as u64) | (u128::from(hi as u64)) << 64
}

crate::bitflags! {
	pub struct ProtFlags: u32 {
		const READ = ViewFlags::READ.bits();
		const WRITE = ViewFlags::WRITE.bits();
		const EXEC = ViewFlags::EXEC.bits();
	}
}
