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
}
