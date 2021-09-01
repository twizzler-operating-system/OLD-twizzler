pub struct Transaction {}
pub enum TransactionErr<E> {
	LogFull,
	Abort(E),
}


