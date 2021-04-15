pub struct Pptr<T> {
    pub(crate) p: u64,
    _pd: std::marker::PhantomData<T>,
}

impl<T> Pptr<T> {
    pub fn new_null() -> Pptr<T> {
        Pptr {
            p: 0,
            _pd: std::marker::PhantomData,
        }
    }
}


