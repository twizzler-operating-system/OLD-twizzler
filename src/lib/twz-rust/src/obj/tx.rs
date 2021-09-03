use super::obj::{GTwzobj, Twzobj};
use crate::ptr::Pptr;
use std::sync::atomic::{AtomicU32, Ordering};

pub struct Transaction {
	leader: GTwzobj,
	followers: Vec<GTwzobj>,
}

pub enum TransactionErr<E> {
	LogFull,
	Abort(E),
}

#[repr(C, packed)]
struct RecordEntry<T> {
	ty: u8,
	fl: u8,
	len: u16,
	data: T,
}

const RECORD_ALLOC: u8 = 1;

struct RecordAlloc {
	owned: u64,
}

enum Record<'a> {
	Alloc(&'a RecordAlloc),
}

impl<T> RecordEntry<T> {
	fn data<'a>(&'a self) -> Record<'a> {
		match self.ty {
			RECORD_ALLOC => Record::Alloc(unsafe { std::mem::transmute::<&T, &RecordAlloc>(&self.data) }),
			_ => panic!("unknown transaction record type"),
		}
	}
}

pub(super) const TX_METAEXT: u64 = 0xabbaabba44556655;

#[repr(C)]
struct TxLog {
	len: u32,
	top: AtomicU32,
	pad2: u64,
	log: [u8; 4096],
}

impl TxLog {
	fn reserve<T>(&mut self) -> &mut T {
		let len = std::mem::size_of::<T>();
		let len = (len + 15usize) & (!15usize);
		let top = self.top.fetch_add(len as u32, Ordering::SeqCst);
		unsafe { std::mem::transmute::<&mut u8, &mut T>(&mut self.log[top as usize]) }
	}
}

impl Default for TxLog {
	fn default() -> Self {
		Self {
			len: 4096,
			top: AtomicU32::new(0),
			pad2: 0,
			log: [0; 4096],
		}
	}
}

#[repr(C)]
#[derive(Default)]
pub(super) struct TransactionManager {
	log: TxLog,
}

impl<T> Twzobj<T> {
	pub(super) fn init_tx(&self) {
		unsafe {
			self.alloc_metaext_unchecked::<TransactionManager>(TX_METAEXT);
		}
	}
}

impl Transaction {
	fn get_log(&self) -> Option<&mut TxLog> {
		self.leader.find_metaext_mut(TX_METAEXT)
	}

	pub(super) fn new(leader: GTwzobj) -> Transaction {
		Transaction {
			leader,
			followers: vec![],
		}
	}

	pub(super) fn prep_alloc_free_on_fail<'b, T>(&'b self, obj: &Twzobj<T>) -> &'b mut u64 {
		let log = self.get_log().unwrap(); //TODO
		let entry = log.reserve::<RecordEntry<RecordAlloc>>();
		&mut entry.data.owned
	}

	pub(super) fn record_base<T>(&self, obj: &Twzobj<T>) {}
}
