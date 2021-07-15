use crate::obj::ObjID;
use crate::flexarray::{FlexArray, FlexArrayField};

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
#[derive(Debug)]
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
	pub hdr: KSOHdr,
	pub count: u64,
	pub flags: u64,
	pub attached: FlexArrayField<KSOAttachment>,
}

pub struct KSOAttachIterator<'a> {
    count: usize,
    curr: usize,
    attach: &'a [KSOAttachment],
}

impl<'a> Iterator for KSOAttachIterator<'a> {
    type Item = &'a KSOAttachment;
    fn next(&mut self) -> Option<Self::Item> {
        if self.curr >= self.count {
            None
        } else {
            let ret = &self.attach[self.curr];
            self.curr += 1;
            Some(ret)
        }
    }
}

impl FlexArray<KSOAttachment> for KSORootHdr {
    fn len(&self) -> usize { self.count as usize }
    fn flex_element(&self) -> &FlexArrayField<KSOAttachment> { &self.attached }
}

impl<'a> IntoIterator for &'a KSORootHdr {
    type Item = &'a KSOAttachment;
    type IntoIter = KSOAttachIterator<'a>;

    fn into_iter(self) -> Self::IntoIter {
        KSOAttachIterator {
            count: self.count as usize,
            curr: 0,
            attach: self.as_slice(),
        }
    }
}

pub const KSO_ROOT_ID: ObjID = 1;


