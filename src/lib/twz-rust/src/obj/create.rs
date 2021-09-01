use super::id::ObjID;
use super::r#const::ProtFlags;

crate::bitflags! {
	pub struct CreateFlags: u64 {
		const HASH_DATA = 0x1;
		const DFL_READ = 0x4;
		const DFL_WRITE = 0x8;
		const DFL_EXEC = 0x10;
		const DFL_USE = 0x20;
		const DFL_DEL = 0x40;
		const ZERO_NONCE = 0x1000;
	}
}

#[derive(Debug, Copy, Clone)]
pub enum BackingType {
	Normal = 0,
}

#[derive(Debug, Copy, Clone)]
pub enum LifetimeType {
	Volatile = 0,
	Persistent = 1,
}

#[derive(Debug, Copy, Clone)]
pub enum KuSpec {
	None,
	Obj(ObjID),
}

#[derive(Debug, Copy, Clone)]
pub enum TieSpec {
	View,
	Obj(ObjID),
}

#[derive(Debug, Copy, Clone)]
pub struct SrcSpec {
	pub(crate) srcid: ObjID,
	pub(crate) start: u64,
	pub(crate) length: u64,
}

pub struct CreateSpec {
	pub(crate) srcs: Vec<SrcSpec>,
	pub(crate) ku: KuSpec,
	pub(crate) lt: LifetimeType,
	pub(crate) bt: BackingType,
	pub(crate) ties: Vec<TieSpec>,
	pub(crate) flags: CreateFlags,
}

impl CreateSpec {
	pub fn new(lt: LifetimeType, bt: BackingType, flags: CreateFlags) -> CreateSpec {
		CreateSpec {
			srcs: vec![],
			ku: KuSpec::None,
			lt,
			bt,
			ties: vec![],
			flags,
		}
	}
	pub fn src(mut self, src: SrcSpec) -> CreateSpec {
		self.srcs.push(src);
		self
	}
	pub fn tie(mut self, tie: TieSpec) -> CreateSpec {
		self.ties.push(tie);
		self
	}
	pub fn ku(mut self, kuspec: KuSpec) -> CreateSpec {
		self.ku = kuspec;
		self
	}
}
