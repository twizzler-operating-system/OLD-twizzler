use super::obj::{GTwzobj, Twzobj};
use crate::ptr::Pptr;

pub struct Transaction {
	leader: GTwzobj,
	followers: Vec<GTwzobj>,
}

pub enum TransactionErr<E> {
	LogFull,
	Abort(E),
}

pub(super) const TX_METAEXT: u64 = 0xabbaabba44556655;

#[repr(C)]
struct TxLog {
	len: u32,
	pad: u32,
	pad2: u64,
	log: [u8; 4096],
}

#[repr(C)]
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
	pub(super) fn new(leader: GTwzobj) -> Transaction {
		Transaction {
			leader,
			followers: vec![],
		}
	}

	pub(super) fn prep_alloc_free_on_fail<'a, R, T>(&'a self, obj: &Twzobj<T>) -> &'a mut Pptr<R> {
		panic!("")
	}

	pub(super) fn record_base<T>(&self, obj: &Twzobj<T>) {}
}
