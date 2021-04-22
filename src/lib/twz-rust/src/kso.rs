use crate::obj::ObjID;

pub enum KSOType {
	None = 0,
	View = 1,
	SecCtx = 2,
	Thread = 3,
	Root = 4,
    Bus = 5,
	Device = 6,
	Max = 7,
}

const KSO_NAME_MAXLEN: usize = 1024;

#[repr(C)]
pub struct KSOAttachment {
	id: ObjID,
	info: u64,
	attype: u32,
	flags: u32,
}

#[repr(C)]
pub struct KSOHdr {
	name: [u8; KSO_NAME_MAXLEN],
	version: u32,
	resv: u32,
	resv2: u64,
}

#[repr(C)]
pub struct KSORootHdr {
	hdr: KSOHdr,
	count: u64,
	flags: u64,
	attached: [KSOAttachment],
}

pub const KSO_ROOT_ID: ObjID = 1;


