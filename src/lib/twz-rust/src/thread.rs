use crate::obj::{ObjID, Twzobj};
use crate::kso::{KSOHdr, KSOAttachment};

const THRD_SYNCPOINTS: usize = 128;
const TWZ_THRD_MAX_SCS: usize = 32;

pub const SYNC_STATE: usize = 0;
pub const SYNC_READY: usize = 1;
pub const SYNC_EXIT: usize = 2;

#[repr(C)]
pub struct ThreadRepr {
	hdr: KSOHdr,
	reprid: ObjID,
    /* TODO atomics */
	syncs: [std::sync::atomic::AtomicU64; THRD_SYNCPOINTS],
	syncinfos: [std::sync::atomic::AtomicU64; THRD_SYNCPOINTS],
	attached: [KSOAttachment; TWZ_THRD_MAX_SCS],
}

impl ThreadRepr {
    pub fn syncs(&self, which: usize) -> &std::sync::atomic::AtomicU64 {
        &self.syncs[which]
    }
}

pub struct TwzThread {
    pub(crate) header: *mut ThreadRepr,
}

impl TwzThread {
    pub fn repr(&self) -> &ThreadRepr {
        unsafe { self.header.as_ref().expect("failed to get thread repr header") }
    }

    pub fn repr_obj(&self) -> Result<Twzobj, crate::TwzErr> {
        Twzobj::init_guid(self.repr().reprid)
    }
}
